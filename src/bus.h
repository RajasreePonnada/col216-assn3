#ifndef BUS_H
#define BUS_H

#include <vector>
#include <queue>
#include <memory> // For std::shared_ptr or std::weak_ptr if needed
#include <numeric> // For std::iota
#include "defs.h"
#include "cache.h"
class Cache; // Forward declaration
class Stats; // Forward declaration

class Bus {
private:
    // One queue per core to facilitate round-robin
    std::vector<std::queue<BusRequest>> requests_per_core;
    std::vector<int> core_priority_order; // Order for checking queues in RR
    int arbitration_pointer = 0; // Index into core_priority_order

    bool busy = false;
    cycle_t transaction_end_cycle = 0;
    BusRequest current_transaction;
    int current_winner = -1; // Core ID whose transaction is currently on the bus

    // ... other private members ...
    uint64_t total_bus_transactions = 0; // *** ADDED ***

    // Pointers to all caches for snooping and responses
    std::vector<Cache*> caches; // Use pointers to avoid slicing, manage lifetime carefully

    unsigned int block_size_bytes; // Needed to calculate traffic
    unsigned int words_per_block;
    Stats* stats; // Pointer to statistics collector

    // Private methods
    bool arbitrate(cycle_t current_cycle); // Returns true if a winner was chosen
    SnoopResult processSnooping(const BusRequest& request, int requestingCoreId, cycle_t current_cycle); // Returns combined snoop results
    void startTransaction(const BusRequest& request, const SnoopResult& snoop_result, cycle_t current_cycle);

    

public:
    Bus(unsigned int block_size, Stats* statistics);

    void registerCache(Cache* cache);

    // Called by caches to request bus access
    // Returns true if successfully queued (could return false if queue limits exist)
    bool addRequest(const BusRequest& request);

    // Called by the simulator each cycle
    void tick(cycle_t current_cycle);

    bool isBusy() const { return busy; }
    // ... constructor, other methods ...
    uint64_t getTotalTransactions() const { return total_bus_transactions; } // *** ADDED Getter ***

};

#endif // BUS_H