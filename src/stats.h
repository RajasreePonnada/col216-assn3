#ifndef STATS_H
#define STATS_H

#include "defs.h"
#include "bus.h"
#include <vector>
#include <iostream>
#include <iomanip> // For formatting output
#include <numeric> // For std::accumulate
class Bus;
class Stats
{
public:
    // Per-core stats
    std::vector<uint64_t> read_instructions;
    std::vector<uint64_t> write_instructions;
    std::vector<cycle_t> total_cycles;     // Final cycle number when core finished
    std::vector<uint64_t> cache_misses;    // Read+Write misses
    std::vector<uint64_t> cache_accesses;  // Read+Write accesses
    std::vector<uint64_t> cache_evictions; // Number of valid blocks replaced
    std::vector<uint64_t> writebacks;      // Number of M blocks evicted/downgraded requiring WB

    // *** ADDED: Per-core bus-related stats ***
    std::vector<uint64_t> invalidations_received;    // Invalidations this core's cache received
    std::vector<uint64_t> data_traffic_caused_bytes; // Traffic caused by this core's requests

    // Bus stats
    uint64_t total_invalidations = 0;     // BusUpgr or BusRdX causing invalidation
    uint64_t total_bus_traffic_bytes = 0; // Data bytes on bus (fetches, WBs, C2C)

    // Track stalls directly if possible (more accurate idle time)
    std::vector<cycle_t> stall_cycles; // Cycles core was stalled waiting for cache

    uint64_t overall_bus_transactions = 0;

    Stats();

    // Method signatures need update
    // void recordInvalidationReceived(int coreId, int count = 1); // Renamed and takes coreId
    // void addBusTraffic(uint64_t bytes, int causingCoreId);      // Takes causingCoreId

    void recordAccess(int coreId, Operation op);
    void recordMiss(int coreId);
    void recordEviction(int coreId);
    void recordWriteback(int coreId);       // Called when WB is initiated
    void recordInvalidation(int count = 1); // Allow recording multiple invalidations (e.g., BusUpgr)
    void addBusTraffic(uint64_t bytes);
    void setCoreCycles(int coreId, cycle_t cycles);
    void incrementStallCycles(int coreId, cycle_t cycles = 1);

    // void printFinalStats(unsigned int block_size);
    // Modify print function signature
    void printFinalStats(
        const std::string &trace_prefix,
        unsigned int s,
        unsigned int E,
        unsigned int b,
        const Bus *bus // Pass const pointer to Bus to get transaction count
    );
};

#endif // STATS_H