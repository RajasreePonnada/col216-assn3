#ifndef BUS_H
#define BUS_H

#include <vector>
#include <queue>
#include <memory> 
#include <numeric> 
#include "defs.h"
#include "cache.h"
class Cache; 
class Stats;

class Bus {
private:
    // One queue per core to facilitate round-robin
    std::vector<std::queue<BusRequest>> requests_per_core;
    std::vector<int> core_priority_order; 
    int arbitration_pointer = 0; 

    bool busy = false;
    cycle_t transaction_end_cycle = 0;
    BusRequest current_transaction;
    int current_winner = -1;


    uint64_t total_bus_transactions = 0;

    std::vector<Cache*> caches; 

    unsigned int block_size_bytes; 
    unsigned int words_per_block;
    Stats* stats; 

    bool arbitrate(cycle_t current_cycle); 
    SnoopResult processSnooping(const BusRequest& request, int requestingCoreId, cycle_t current_cycle); 
    void startTransaction(const BusRequest& request, const SnoopResult& snoop_result, cycle_t current_cycle);

    

public:
    Bus(unsigned int block_size, Stats* statistics);

    void registerCache(Cache* cache);

    bool addRequest(const BusRequest& request);

    void tick(cycle_t current_cycle);

    bool isBusy() const { return busy; }

    uint64_t getTotalTransactions() const { return total_bus_transactions; } 

};

#endif 