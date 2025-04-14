#ifndef STATS_H
#define STATS_H

#include "defs.h"
#include <vector>
#include <iostream>
#include <iomanip> // For formatting output
#include <numeric> // For std::accumulate

class Stats {
public:
    // Per-core stats
    std::vector<uint64_t> read_instructions;
    std::vector<uint64_t> write_instructions;
    std::vector<cycle_t> total_cycles; // Final cycle number when core finished
    std::vector<uint64_t> cache_misses; // Read+Write misses
    std::vector<uint64_t> cache_accesses; // Read+Write accesses
    std::vector<uint64_t> cache_evictions; // Number of valid blocks replaced
    std::vector<uint64_t> writebacks;      // Number of M blocks evicted/downgraded requiring WB

    // Bus stats
    uint64_t total_invalidations = 0;     // BusUpgr or BusRdX causing invalidation
    uint64_t total_bus_traffic_bytes = 0; // Data bytes on bus (fetches, WBs, C2C)

    // Track stalls directly if possible (more accurate idle time)
    std::vector<cycle_t> stall_cycles; // Cycles core was stalled waiting for cache

    Stats();

    void recordAccess(int coreId, Operation op);
    void recordMiss(int coreId);
    void recordEviction(int coreId);
    void recordWriteback(int coreId); // Called when WB is initiated
    void recordInvalidation(int count = 1); // Allow recording multiple invalidations (e.g., BusUpgr)
    void addBusTraffic(uint64_t bytes);
    void setCoreCycles(int coreId, cycle_t cycles);
    void incrementStallCycles(int coreId, cycle_t cycles = 1);

    void printFinalStats(unsigned int block_size);
};

#endif // STATS_H