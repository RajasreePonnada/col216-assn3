#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <vector>
#include <string>
#include <memory> 
#include "defs.h"
#include "core.h"
#include "cache.h"
#include "bus.h"
#include "stats.h"

class Simulator {
private:
    // Configuration
    unsigned int s, E, b; // Cache parameters
    unsigned int block_size;
    std::string trace_base_name;
    std::string output_file;

    Stats statistics;
    std::unique_ptr<Bus> bus;
    std::vector<std::unique_ptr<Cache>> caches;
    std::vector<std::unique_ptr<Core>> cores;

    cycle_t global_cycle = 0;

    bool checkCompletion(); // Checks if all cores are finished

public:
    Simulator(unsigned int s_bits, unsigned int E_assoc, unsigned int b_bits,
              const std::string& trace_name, const std::string& outfile = "");

    void run();

    void printStats();

    // Returns the cycle count when the *last* core finished.
    cycle_t getMaxCycles() const;
};

#endif 