#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <cmath> // For log2
#include <map>   // For tracking MSHRs or pending requests

#include "defs.h"
#include "cache_set.h"
#include "bus.h"
#include "stats.h"

class Bus;
class Stats;
class Cache {
private:
    int id; // Core ID
    unsigned int num_sets;
    unsigned int associativity;
    unsigned int block_size; // Bytes
    unsigned int block_bits; // b
    unsigned int set_bits;   // s

    std::vector<CacheSet> sets;
    Bus* bus; // Pointer to the shared bus
    Stats* stats; // Pointer to statistics collector

    // Helper methods
    addr_t getTag(addr_t address) const;
    unsigned int getIndex(addr_t address) const;
    addr_t getBlockAddress(addr_t address) const;
    addr_t reconstructAddress(addr_t tag, unsigned int index) const;

    // --- State for handling pending misses (simple version) ---
    // Using a map to potentially handle multiple outstanding misses later (MSHR idea)
    // Key: Block Address, Value: Details of the pending request
    struct PendingRequest {
        Operation original_op;
        int target_way = -1;        // Which way is allocated for the incoming data
        bool writeback_pending = false; // Is a WB for the victim also pending?
        addr_t victim_addr = 0;      // Address of the block being written back
        cycle_t request_init_cycle = 0; // When the miss handling started
    };
    std::map<addr_t, PendingRequest> pending_requests; // Tracks block addresses waiting for bus data

    // --- Cache internal state ---
    bool stalled = false; // Is the CORE stalled due to this cache?


    // Private cache logic functions
    void handleMiss(addr_t address, unsigned int index, addr_t tag, Operation op, cycle_t current_cycle);
    void allocateBlock(addr_t block_addr, unsigned int index, addr_t tag, int& way_index, cycle_t current_cycle); // Finds/evicts way
    void initiateWriteback(addr_t victim_address, unsigned int victim_set_index, int victim_way_index, cycle_t current_cycle);


public:
    Cache(int core_id, unsigned int s, unsigned int E, unsigned int b, Bus* shared_bus, Stats* statistics);

    // Called by the Core
    // Returns true if the access is a hit (completes in 1 cycle), false if miss (stalls core)
    bool access(addr_t address, Operation op, cycle_t current_cycle);

    // Called by the Bus during snooping
    // Returns results of the snoop (e.g., if data was supplied)
    SnoopResult snoopRequest(BusTransaction transaction, addr_t address, cycle_t current_cycle);

    // Called by the Bus when a requested transaction completes
    // The request that completed is passed in.
    void handleBusCompletion(const BusRequest& completed_request, cycle_t current_cycle);

    // Called by Core/Simulator to check stall status
    bool isStalled() const { return stalled; }

    // Helper for Bus snooping check
    bool isBlockShared(addr_t address);

};

#endif // CACHE_H