#include "cache.h"
#include <stdexcept>
#include <iostream>

Cache::Cache(int core_id, unsigned int s, unsigned int E, unsigned int b, Bus *shared_bus, Stats *statistics) : id(core_id),
                                                                                                                num_sets(s == 0 ? 1 : (1 << s)),
                                                                                                                associativity(E),
                                                                                                                block_size(1 << b),
                                                                                                                block_bits(b),
                                                                                                                set_bits(s),
                                                                                                                sets(num_sets, CacheSet(E)),
                                                                                                                bus(shared_bus),
                                                                                                                stats(statistics)
{
    if (bus == nullptr || stats == nullptr)
    {
        throw std::invalid_argument("Cache requires valid Bus and Stats pointers.");
    }
    if (E == 0 || block_size == 0)
    {
        throw std::invalid_argument("Associativity and Block Size must be non-zero.");
    }
    bus->registerCache(this);
}

addr_t Cache::getTag(addr_t address) const
{
    if ((set_bits + block_bits) >= 32)
        return 0;
    return address >> (set_bits + block_bits);
}

unsigned int Cache::getIndex(addr_t address) const
{
    if (set_bits == 0)
        return 0;
    addr_t mask = (1 << set_bits) - 1;
    return (address >> block_bits) & mask;
}

addr_t Cache::getBlockAddress(addr_t address) const
{
    if (block_bits >= 32)
        return 0;
    addr_t mask = ~((1ULL << block_bits) - 1);
    return address & mask;
}

addr_t Cache::reconstructAddress(addr_t tag, unsigned int index) const
{
    return (tag << (set_bits + block_bits)) | (index << block_bits);
}

bool Cache::access(addr_t address, Operation op, cycle_t current_cycle)
{
    if (stalled)
    {
        std::cerr << "Warning: Core " << id << " accessed cache while stalled!" << std::endl;
        return false;
    }

    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    stats->recordAccess(id, op);

    int way_index = sets[index].findLine(tag);

    if (way_index != -1)
    {
        CacheLine &line = sets[index].getLine(way_index);
        MESIState current_state = line.state;

        if (op == Operation::READ)
        {
            sets[index].updateLRU(way_index, current_cycle);
            return true;
        }
        else
        {
            if (current_state == MESIState::MODIFIED)
            {
                sets[index].updateLRU(way_index, current_cycle);
                return true;
            }
            else if (current_state == MESIState::EXCLUSIVE)
            {
                line.state = MESIState::MODIFIED;
                sets[index].updateLRU(way_index, current_cycle);
                return true;
            }
            else
            {
                stats->recordMiss(id);
                stalled = true;
                handleMiss(address, index, tag, op, current_cycle);
                return false;
            }
        }
    }
    else
    {
        stats->recordMiss(id);
        stalled = true;
        handleMiss(address, index, tag, op, current_cycle);
        return false;
    }
}

void Cache::handleMiss(addr_t address, unsigned int index, addr_t tag, Operation op, cycle_t current_cycle)
{
    addr_t block_addr = getBlockAddress(address);

    if (pending_requests.count(block_addr))
    {
        return;
    }

    int existing_way = sets[index].findLine(tag);
    if (op == Operation::WRITE && existing_way != -1 && sets[index].getLine(existing_way).state == MESIState::SHARED)
    {
        PendingRequest pending;
        pending.original_op = op;
        pending.target_way = existing_way;
        pending.request_init_cycle = current_cycle;
        pending_requests[block_addr] = pending;
        BusRequest bus_req;
        bus_req.requestingCoreId = id;
        bus_req.type = BusTransaction::BusUpgr;
        bus_req.address = block_addr;
        bus_req.request_cycle = current_cycle;
        bus->addRequest(bus_req);
        return;
    }

    int target_way = -1;
    allocateBlock(block_addr, index, tag, target_way, current_cycle);

    if (target_way == -1)
    {
        std::cerr << "Error: Core " << id << " could not allocate block for miss addr " << std::hex << address << std::dec << "!" << std::endl;
        stalled = false;
        return;
    }

    PendingRequest pending;
    pending.original_op = op;
    pending.target_way = target_way;
    pending.request_init_cycle = current_cycle;
    pending_requests[block_addr] = pending;

    BusRequest bus_req;
    bus_req.requestingCoreId = id;
    bus_req.type = (op == Operation::READ) ? BusTransaction::BusRd : BusTransaction::BusRdX;
    bus_req.address = block_addr;
    bus_req.request_cycle = current_cycle;
    bus->addRequest(bus_req);
}

void Cache::allocateBlock(addr_t block_addr, unsigned int index, addr_t tag, int &way_index, cycle_t current_cycle)
{
    way_index = sets[index].findInvalidLine();

    if (way_index == -1)
    {
        way_index = sets[index].getLRUVictim();
        CacheLine &victim_line = sets[index].getLine(way_index);

        if (victim_line.isValid())
        {
            stats->recordEviction(id);

            if (victim_line.state == MESIState::MODIFIED)
            {
                addr_t victim_addr = reconstructAddress(victim_line.tag, index);
                initiateWriteback(victim_addr, index, way_index, current_cycle);
            }
        }
    }

    CacheLine &target_line = sets[index].getLine(way_index);
    target_line.state = MESIState::INVALID;
    target_line.tag = tag;
    target_line.lastUsedCycle = current_cycle;
}

void Cache::initiateWriteback(addr_t victim_address, unsigned int victim_set_index, int victim_way_index, cycle_t current_cycle)
{
    stats->recordWriteback(id);

    BusRequest wb_req;
    wb_req.requestingCoreId = id;
    wb_req.type = BusTransaction::Writeback;
    wb_req.address = victim_address;
    wb_req.request_cycle = current_cycle;
    bus->addRequest(wb_req);
}

SnoopResult Cache::snoopRequest(BusTransaction transaction, addr_t address, cycle_t current_cycle)
{
    SnoopResult result;
    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    int way_index = sets[index].findLine(tag);

    if (way_index != -1)
    {
        CacheLine &line = sets[index].getLine(way_index);
        MESIState current_state = line.state;

        switch (transaction)
        {
        case BusTransaction::BusRd:
            if (current_state == MESIState::MODIFIED)
            {
                addr_t my_block_addr = reconstructAddress(line.tag, index);
                initiateWriteback(my_block_addr, index, way_index, current_cycle);
                line.state = MESIState::SHARED;
                result.data_supplied = true;
                result.was_dirty = true;
            }
            else if (current_state == MESIState::EXCLUSIVE)
            {
                line.state = MESIState::SHARED;
                result.data_supplied = true;
            }
            break;

        case BusTransaction::BusRdX:
            if (current_state == MESIState::MODIFIED)
            {
                addr_t my_block_addr = reconstructAddress(line.tag, index);
                initiateWriteback(my_block_addr, index, way_index, current_cycle);
                result.data_supplied = true;
                result.was_dirty = true;
                line.state = MESIState::INVALID;
                stats->recordInvalidationReceived(id);
            }
            else if (current_state == MESIState::EXCLUSIVE)
            {
                result.data_supplied = false;
                line.state = MESIState::INVALID;
                stats->recordInvalidationReceived(id);
            }
            else if (current_state == MESIState::SHARED)
            {
                result.data_supplied = false;
                line.state = MESIState::INVALID;
                stats->recordInvalidationReceived(id);
            }
            break;

        case BusTransaction::BusUpgr:
            if (current_state == MESIState::SHARED)
            {
                line.state = MESIState::INVALID;
                stats->recordInvalidationReceived(id);
            }
            break;

        case BusTransaction::Writeback:
            break;
        case BusTransaction::NoTransaction:
            break;
        }
    }

    if (way_index != -1 && sets[index].getLine(way_index).isValid())
    {
        result.is_shared = true;
    }

    return result;
}

bool Cache::isBlockShared(addr_t address)
{
    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    int way_index = sets[index].findLine(tag);
    if (way_index != -1)
    {
        return sets[index].getLine(way_index).isValid();
    }
    return false;
}

void Cache::handleBusCompletion(const BusRequest &completed_request, cycle_t current_cycle)
{
    if (completed_request.type == BusTransaction::Writeback)
    {
        return;
    }

    addr_t block_addr = completed_request.address;
    auto pending_it = pending_requests.find(block_addr);
    if (pending_it == pending_requests.end())
    {
        return;
    }

    PendingRequest &pending = pending_it->second;
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);
    int way_index = pending.target_way;

    if (way_index < 0 || way_index >= associativity)
    {
        std::cerr << "Error: Invalid target way index in pending request for Core " << id << ", Addr " << std::hex << block_addr << std::dec << std::endl;
        pending_requests.erase(pending_it);
        stalled = false;
        return;
    }

    CacheLine &line = sets[index].getLine(way_index);

    bool shared_after_snoop = false;
    switch (completed_request.type)
    {
    case BusTransaction::BusRd:
        if (shared_after_snoop)
        {
            line.state = MESIState::SHARED;
        }
        else
        {
            line.state = MESIState::EXCLUSIVE;
        }
        break;
    case BusTransaction::BusRdX:
        line.state = MESIState::MODIFIED;
        break;
    case BusTransaction::BusUpgr:
        line.state = MESIState::MODIFIED;
        break;
    default:
        break;
    }

    line.lastUsedCycle = current_cycle;
    pending_requests.erase(pending_it);
    stalled = false;
}
