#include "bus.h"
#include "cache.h"
#include "stats.h"
#include <stdexcept>
#include <iostream>

Bus::Bus(unsigned int block_size, Stats *statistics) : requests_per_core(NUM_CORES),
                                                       core_priority_order(NUM_CORES),
                                                       block_size_bytes(block_size),
                                                       words_per_block(block_size / 4), // Assuming word size is 4 bytes
                                                       stats(statistics)
{
    if (block_size == 0 || (block_size % 4 != 0))
    {
        throw std::invalid_argument("Block size must be non-zero and multiple of 4.");
    }
    if (stats == nullptr)
    {
        throw std::invalid_argument("Stats pointer cannot be null for Bus.");
    }
    // Initialize priority order (e.g., 0, 1, 2, 3)
    std::iota(core_priority_order.begin(), core_priority_order.end(), 0);
}

void Bus::registerCache(Cache *cache)
{
    if (caches.size() >= NUM_CORES)
    {
        throw std::runtime_error("Cannot register more than NUM_CORES caches with the bus.");
    }
    caches.push_back(cache);
}

bool Bus::addRequest(const BusRequest &request)
{
    if (request.requestingCoreId < 0 || request.requestingCoreId >= NUM_CORES)
    {
        std::cerr << "Error: Invalid core ID " << request.requestingCoreId << " in bus request." << std::endl;
        return false; // Invalid request
    }
    requests_per_core[request.requestingCoreId].push(request);
    return true;
}

void Bus::tick(cycle_t current_cycle)
{
    // 1. Check if current transaction finishes this cycle
    if (busy && current_cycle >= transaction_end_cycle)
    {
        // Transaction completed
        Cache *ownerCache = caches[current_transaction.requestingCoreId];

        // Notify the cache that its request is done (data available/writeback done etc.)
        ownerCache->Cache::handleBusCompletion(current_transaction, current_cycle);

        // Reset bus state
        busy = false;
        current_winner = -1;
        current_transaction = {}; // Clear current transaction
    }

    // 2. If bus is not busy, arbitrate and start the next transaction
    if (!busy)
    {
        if (arbitrate(current_cycle))
        { // Sets current_winner and current_transaction
            // Broadcast snoop messages to OTHERS and check for C2C transfer/sharing
            SnoopResult snoop_result = processSnooping(current_transaction, current_winner, current_cycle);

            // Start the transaction (sets bus busy state and timer)
            startTransaction(current_transaction, snoop_result, current_cycle);
        }
    }
}

// Round-Robin Arbitration
bool Bus::arbitrate(cycle_t current_cycle)
{
    int checked_cores = 0;
    int current_arbitration_index = arbitration_pointer;

    while (checked_cores < NUM_CORES)
    {
        int core_to_check = core_priority_order[current_arbitration_index];

        if (!requests_per_core[core_to_check].empty())
        {
            current_winner = core_to_check;
            current_transaction = requests_per_core[core_to_check].front();
            requests_per_core[core_to_check].pop();

            arbitration_pointer = (current_arbitration_index + 1) % NUM_CORES;

            if (current_transaction.request_cycle == 0)
            {
                current_transaction.request_cycle = current_cycle;
            }

            return true; // Winner found
        }

        current_arbitration_index = (current_arbitration_index + 1) % NUM_CORES;
        checked_cores++;
    }

    current_winner = -1;
    return false;
}

SnoopResult Bus::processSnooping(const BusRequest &request, int requestingCoreId, cycle_t current_cycle)
{
    SnoopResult combined_result;
    int sharer_count = 0;
    int invalidation_count = 0;

    // Snoop all *other* caches
    for (int i = 0; i < caches.size(); ++i)
    {
        if (i == requestingCoreId)
            continue; // Don't snoop self

        SnoopResult result = caches[i]->snoopRequest(request.type, request.address, current_cycle);

        if (result.data_supplied)
        {
            if (!combined_result.data_supplied)
            {
                combined_result = result;
            }
        }
        if (caches[i]->isBlockShared(request.address))
        {
            sharer_count++;
            combined_result.sharers.push_back(i);
        }
    }

    if (request.type == BusTransaction::BusRd || request.type == BusTransaction::BusRdX || request.type == BusTransaction::BusUpgr)
    {
        if (sharer_count > 0)
        {
            combined_result.is_shared = true;
        }
        if (request.type == BusTransaction::BusRd && sharer_count == 0 && !combined_result.data_supplied)
        {
            combined_result.is_shared = false;
        }
        else
        {
            combined_result.is_shared = true;
        }

        if (request.type == BusTransaction::BusRdX || request.type == BusTransaction::BusUpgr)
        {
            combined_result.is_shared = false;
        }
    }

    return combined_result;
}

void Bus::startTransaction(const BusRequest &request, const SnoopResult &snoop_result, cycle_t current_cycle)
{
    busy = true;
    cycle_t latency = 0;
    uint64_t traffic = 0;
    bool is_data_transfer = false;

    if (request.type != BusTransaction::NoTransaction)
    { // Ensure it's a real transaction
        total_bus_transactions++;
    }

    switch (request.type)
    {
    case BusTransaction::BusRd:
    case BusTransaction::BusRdX:
        is_data_transfer = true;
        if (snoop_result.data_supplied)
        {
            // Cache-to-cache transfer
            latency = C2C_BLOCK_TRANSFER_CYCLE_FACTOR * words_per_block; // 2N cycles
            traffic = block_size_bytes;
        }
        else
        {
            // Fetch from memory
            latency = MEM_ACCESS_CYCLES;
            traffic = block_size_bytes;
        }
        break;

    case BusTransaction::Writeback:
        is_data_transfer = true;
        latency = MEM_ACCESS_CYCLES;
        traffic = block_size_bytes; // Writing block back
        break;

    case BusTransaction::BusUpgr:
        latency = 1;
        traffic = 0;
        break;
    case BusTransaction::NoTransaction:
        busy = false;
        return;
    }

    transaction_end_cycle = current_cycle + latency;

    // Record data traffic if any
    if (is_data_transfer && traffic > 0)
    {
        stats->addBusTraffic(traffic, request.requestingCoreId);
    }
}