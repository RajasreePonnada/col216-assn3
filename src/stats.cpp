#include "stats.h"
#include <numeric> // For std::accumulate
#include <cmath>   // For std::max if needed, though direct comparison works

Stats::Stats() :
    read_instructions(NUM_CORES, 0),
    write_instructions(NUM_CORES, 0),
    total_cycles(NUM_CORES, 0),
    cache_misses(NUM_CORES, 0),
    cache_accesses(NUM_CORES, 0),
    cache_evictions(NUM_CORES, 0),
    writebacks(NUM_CORES, 0),
    stall_cycles(NUM_CORES, 0) // Initialize stall cycles
    {}

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

void Stats::recordInvalidation(int count) {
    total_invalidations += count;
}

void Stats::addBusTraffic(uint64_t bytes) {
    total_bus_traffic_bytes += bytes;
}

void Stats::setCoreCycles(int coreId, cycle_t cycles) {
    total_cycles[coreId] = cycles;
}

void Stats::incrementStallCycles(int coreId, cycle_t cycles) {
    stall_cycles[coreId] += cycles;
}


void Stats::printFinalStats(unsigned int block_size) {
    std::cout << "========== Simulation Results ==========" << std::endl;
    cycle_t max_cycles = 0; // Find max cycles across cores

    for (int i = 0; i < NUM_CORES; ++i) {
        double miss_rate = (cache_accesses[i] == 0) ? 0.0 : static_cast<double>(cache_misses[i]) / cache_accesses[i];
        uint64_t total_instructions = read_instructions[i] + write_instructions[i];

        // Idle cycles = Total cycles core was active - cycles spent on hits (assuming 1 cycle/hit)
        // More accurate: Total cycles - instructions executed (if 1 instr/cycle when not stalled)
        // Even better: Use directly tracked stall cycles.
        // Note: total_cycles[i] is the cycle the core *finished*.
        // Idle cycles = total_cycles - non_stalled_cycles
        // Assuming non_stalled_cycles = total_instructions (1 cycle per instruction)
        cycle_t computed_idle_cycles = (total_cycles[i] >= total_instructions) ? (total_cycles[i] - total_instructions) : 0;

        // Compare with tracked stall cycles (should be similar if assumptions match)
        cycle_t tracked_idle_cycles = stall_cycles[i];

        max_cycles = std::max(max_cycles, total_cycles[i]);


        std::cout << "--- Core " << i << " ---" << std::endl;
        std::cout << "  Instructions (R/W):    " << read_instructions[i] << " / " << write_instructions[i] << " (Total: " << total_instructions << ")" << std::endl;
        std::cout << "  Total Core Cycles:     " << total_cycles[i] << std::endl;
        // std::cout << "  Idle Cycles (Computed):" << computed_idle_cycles << std::endl; // Keep one version
        std::cout << "  Idle Cycles (Tracked): " << tracked_idle_cycles << std::endl; // Use tracked stalls
        std::cout << "  Cache Misses:          " << cache_misses[i] << std::endl;
        std::cout << "  Cache Accesses:        " << cache_accesses[i] << std::endl;
        std::cout << "  Cache Miss Rate:       " << std::fixed << std::setprecision(4) << miss_rate << std::endl;
        std::cout << "  Cache Evictions:       " << cache_evictions[i] << std::endl;
        std::cout << "  Writebacks to Memory:  " << writebacks[i] << std::endl;
        std::cout << std::endl;
    }

    std::cout << "--- Overall ---" << std::endl;
    std::cout << "  Max Execution Cycles (Across Cores): " << max_cycles << std::endl;
    std::cout << std::endl;

    std::cout << "--- Bus Stats ---" << std::endl;
    std::cout << "  Total Invalidations Sent: " << total_invalidations << std::endl;
    std::cout << "  Total Data Traffic:       " << total_bus_traffic_bytes << " Bytes" << std::endl;

     // Calculate traffic related to writebacks specifically
    uint64_t total_wbs = std::accumulate(writebacks.begin(), writebacks.end(), 0ULL);
    uint64_t wb_traffic = total_wbs * block_size;
    uint64_t fetch_traffic = total_bus_traffic_bytes - wb_traffic; // Approximation, includes C2C traffic

    std::cout << "    Writeback Traffic:      " << wb_traffic << " Bytes" << std::endl;
    std::cout << "    Fetch/C2C Traffic:      " << fetch_traffic << " Bytes" << std::endl;


    std::cout << "========================================" << std::endl;

    // You might want to output this to a file as well for the report plotting
}