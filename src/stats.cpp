#include "stats.h"
#include "bus.h"
#include <numeric> // For std::accumulate
#include <cmath>   // For std::max if needed, though direct comparison works
#include <iomanip> // Include for formatting

class Bus;

Stats::Stats() :
    read_instructions(NUM_CORES, 0),
    write_instructions(NUM_CORES, 0),
    total_cycles(NUM_CORES, 0),
    cache_misses(NUM_CORES, 0),
    cache_accesses(NUM_CORES, 0),
    cache_evictions(NUM_CORES, 0),
    writebacks(NUM_CORES, 0),
    stall_cycles(NUM_CORES, 0), // Initialize stall cycles
    // *** Initialize new vectors ***
    invalidations_received(NUM_CORES, 0),
    data_traffic_caused_bytes(NUM_CORES, 0)
    {} // Other global stats default to 0

    // Renamed method to record invalidations RECEIVED by a specific core's cache
void Stats::recordInvalidationReceived(int coreId, int count) {
    if (coreId >= 0 && coreId < NUM_CORES) {
        invalidations_received[coreId] += count;
    }
    // Also increment the global count (total invalidations broadcast)
    total_invalidations += count;
}

// Modified method to track total traffic AND traffic caused by specific core
void Stats::addBusTraffic(uint64_t bytes, int causingCoreId) {
    total_bus_traffic_bytes += bytes; // Update global total
    if (causingCoreId >= 0 && causingCoreId < NUM_CORES) {
        data_traffic_caused_bytes[causingCoreId] += bytes;
    } else {
         // It seems the error originates *before* this, as the message is "Invalid core ID... in bus request."
         // But adding an error log here might confirm if this function receives bad data.
         std::cerr << "ERROR (Stats::addBusTraffic): Received invalid causingCoreId: " << causingCoreId << std::endl;
    }
}

void Stats::recordAccess(int coreId, Operation op) {
    cache_accesses[coreId]++;
    if (op == Operation::READ) {
        read_instructions[coreId]++;
    } else {
        write_instructions[coreId]++;
    }
}

void Stats::recordMiss(int coreId) {
    cache_misses[coreId]++;
}

void Stats::recordEviction(int coreId) {
    cache_evictions[coreId]++;
}

void Stats::recordWriteback(int coreId) {
    writebacks[coreId]++;
}

// void Stats::recordInvalidation(int count) {
//     total_invalidations += count;
// }

void Stats::addBusTraffic(uint64_t bytes) {
    total_bus_traffic_bytes += bytes;
}

void Stats::setCoreCycles(int coreId, cycle_t cycles) {
    total_cycles[coreId] = cycles;
}

void Stats::incrementStallCycles(int coreId, cycle_t cycles) {
    stall_cycles[coreId] += cycles;
}


// --- Rewrite printFinalStats (incorporating new stats) ---
void Stats::printFinalStats(
    const std::string& trace_prefix,
    unsigned int s,
    unsigned int E,
    unsigned int b,
    const Bus* bus
) {
    // --- Calculate derived parameters ---
    unsigned long long block_size_bytes = 1ULL << b;
    unsigned long long num_sets = 1ULL << s;
    double cache_size_kb = static_cast<double>(num_sets) * E * block_size_bytes / 1024.0;

    // Get total bus transactions from the Bus object
    if (bus) {
        overall_bus_transactions = bus->getTotalTransactions();
    } else {
        overall_bus_transactions = 0;
    }

    // --- Print Simulation Parameters ---
    std::cout << "Simulation Parameters:" << std::endl;
    std::cout << "  Trace Prefix: " << trace_prefix << std::endl;
    std::cout << "  Set Index Bits: " << s << std::endl;
    std::cout << "  Associativity: " << E << std::endl;
    std::cout << "  Block Bits: " << b << std::endl;
    std::cout << "  Block Size (Bytes): " << block_size_bytes << std::endl;
    std::cout << "  Number of Sets: " << num_sets << std::endl;
    // std::cout << "  Cache Size (KB per core): " << std::fixed << std::setprecision(2) << cache_size_kb << std::endl;
    std::cout << "  MESI Protocol: Enabled" << std::endl;
    std::cout << "  Write Policy: Write-back, Write-allocate" << std::endl;
    std::cout << "  Replacement Policy: LRU" << std::endl;
    std::cout << "  Bus: Central snooping bus" << std::endl;
    std::cout << std::endl;

    // --- Print Per-Core Statistics ---
    cycle_t max_cycles = 0;
    for (int i = 0; i < NUM_CORES; ++i) {
        max_cycles = std::max(max_cycles, total_cycles[i]);

        uint64_t total_instructions = read_instructions[i] + write_instructions[i];
        double miss_rate_percent = (cache_accesses[i] == 0) ? 0.0 : (static_cast<double>(cache_misses[i]) / cache_accesses[i]) * 100.0;

        std::cout << "Core " << i << " Statistics:" << std::endl;
        std::cout << "  Total Instructions: " << total_instructions << std::endl;
        std::cout << "  Total Reads: " << read_instructions[i] << std::endl;
        std::cout << "  Total Writes: " << write_instructions[i] << std::endl;
        std::cout << "  Total Execution Cycles: " << total_cycles[i] << std::endl;
        std::cout << "  Idle Cycles: " << stall_cycles[i] << std::endl;
        std::cout << "  Cache Misses: " << cache_misses[i] << std::endl;
        std::cout << "  Cache Miss Rate: " << std::fixed << std::setprecision(4) << miss_rate_percent << "%" << std::endl;
        std::cout << "  Cache Evictions: " << cache_evictions[i] << std::endl;
        std::cout << "  Writebacks: " << writebacks[i] << std::endl; // Writebacks *initiated* by core i
        // *** ADDED Per-Core Bus Stats ***
        std::cout << "  Bus Invalidations Received: " << invalidations_received[i] << std::endl;
        std::cout << "  Data Traffic Caused (Bytes): " << data_traffic_caused_bytes[i] << std::endl;
        std::cout << std::endl;
    }

     // --- Print Overall Bus Summary ---
    std::cout << "Overall Bus Summary:" << std::endl;
    std::cout << "  Total Bus Transactions: " << overall_bus_transactions << std::endl;
    std::cout << "  Total Bus Traffic (Bytes): " << total_bus_traffic_bytes << std::endl;
    // std::cout << "  Total Bus Invalidations Sent: " << total_invalidations << std::endl; // Total broadcast on bus
    std::cout << std::endl;

    // std::cout << "Maximum Execution Time (Across Cores): " << max_cycles << " cycles" << std::endl;
    // std::cout << "========================================" << std::endl;
}