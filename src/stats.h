#ifndef STATS_H
#define STATS_H

#include "defs.h"
#include "bus.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <numeric>

class Bus;
class Stats
{
public:
    std::vector<uint64_t> read_instructions;
    std::vector<uint64_t> write_instructions;
    std::vector<cycle_t> total_cycles;
    std::vector<uint64_t> cache_misses;
    std::vector<uint64_t> cache_accesses;
    std::vector<uint64_t> cache_evictions;
    std::vector<uint64_t> writebacks;
    std::vector<uint64_t> invalidations_received;
    std::vector<uint64_t> data_traffic_caused_bytes;

    uint64_t total_invalidations = 0;
    uint64_t total_bus_traffic_bytes = 0;
    std::vector<cycle_t> stall_cycles;
    uint64_t overall_bus_transactions = 0;

    Stats();

    void recordInvalidationReceived(int coreId, int count = 1);
    void addBusTraffic(uint64_t bytes, int causingCoreId);
    void recordAccess(int coreId, Operation op);
    void recordMiss(int coreId);
    void recordEviction(int coreId);
    void recordWriteback(int coreId);
    void addBusTraffic(uint64_t bytes);
    void setCoreCycles(int coreId, cycle_t cycles);
    void incrementStallCycles(int coreId, cycle_t cycles = 1);

    void printFinalStats(
        const std::string &trace_prefix,
        unsigned int s,
        unsigned int E,
        unsigned int b,
        const Bus *bus
    );
};

#endif
