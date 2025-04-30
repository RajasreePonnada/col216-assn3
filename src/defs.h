#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <cstdint> // For uint32_t, uint64_t
#include <vector>  // Added for SnoopResult sharers

// Type Definitions
using addr_t = uint32_t;       // 32-bit addresses
using cycle_t = uint64_t;     // Cycle counter (can get large)
const int NUM_CORES = 4;

// MESI States
enum class MESIState {
    INVALID,
    SHARED,
    EXCLUSIVE,
    MODIFIED
};

// Cache Operation Types
enum class Operation {
    READ,
    WRITE
};

// Bus Transaction Types
enum class BusTransaction {
    NoTransaction, // Default/idle state
    BusRd,      // Read request seeking shared data
    BusRdX,     // Read request seeking exclusive ownership (for writing)
    BusUpgr,    // Request to upgrade from S to M (invalidate others)
    Writeback,  // Writing a dirty block back to memory
    // CacheToCache is implicit in BusRd/BusRdX responses now
};

// Latencies
const cycle_t L1_HIT_CYCLES = 1;
const cycle_t MEM_ACCESS_CYCLES = 100; // Additional cycles for memory fetch/writeback
// const cycle_t BUS_UPDATE_CYCLES = 2; // Word transfer (BusUpdate) - Not directly used, C2C block transfer used
const cycle_t C2C_BLOCK_TRANSFER_CYCLE_FACTOR = 2; // Per word (N = block_size / 4)

// Struct to represent a memory access request
struct MemAccess {
    Operation type;
    addr_t address;
};

// Struct for bus requests
struct BusRequest {
    int requestingCoreId = -1;
    BusTransaction type = BusTransaction::NoTransaction;
    addr_t address = 0;
    cycle_t request_cycle = 0; // Cycle when request was added to queue
};

// Struct for snooping results
struct SnoopResult {
    bool data_supplied = false; // Did a cache supply data (was M or E)?
    bool was_dirty = false;     // If data supplied, was the state M? (for potential implicit WB)
    bool is_shared = false;     // Is the block shared after this snoop? (Helps determine E vs S on read miss)
    std::vector<int> sharers;   // List of cores sharing the block (if needed for complex protocols)
};


#endif