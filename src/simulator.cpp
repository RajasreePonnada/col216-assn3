#include "simulator.h"
#include <iostream>
#include <limits>   
#include <algorithm> 
#include <stdexcept> 
Simulator::Simulator(unsigned int s_bits, unsigned int E_assoc, unsigned int b_bits,
                     const std::string &trace_name, const std::string &outfile) : s(s_bits), E(E_assoc), b(b_bits),
                                                                                  block_size(1 << b_bits),
                                                                                  trace_base_name(trace_name),
                                                                                  output_file(outfile)
{
    if (block_size == 0 || E == 0)
    {
        throw std::runtime_error("Block size and associativity must be > 0.");
    }

    // Create Bus first (needs block size and stats)
    bus = std::make_unique<Bus>(block_size, &statistics);

    // Create Caches (need bus and stats)
    for (int i = 0; i < NUM_CORES; ++i)
    {
        caches.push_back(std::make_unique<Cache>(i, s, E, b, bus.get(), &statistics));
    }

    // Create Cores (need cache and stats)
    for (int i = 0; i < NUM_CORES; ++i)
    {
        std::string filename = trace_base_name + "_proc" + std::to_string(i) + ".trace";
        try
        {
            cores.push_back(std::make_unique<Core>(i, filename, caches[i].get(), &statistics));
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "Fatal Error: Could not initialize Core " << i << ": " << e.what() << std::endl;
            // Rethrow or handle cleanup if necessary
            throw; // Stop simulation setup
        }
    }
    // Bus registration is now handled inside Cache constructor.
}

bool Simulator::checkCompletion()
{
    for (const auto &core : cores)
    {
        if (!core->isFinished())
        {
            return false; // At least one core is still running
        }
    }
    return true; // All cores are finished
}

void Simulator::run()
{
    // std::cout << "Starting simulation..." << std::endl;
    global_cycle = 0; // Start at cycle 1

    while (true)
    {
        global_cycle++;

        // 1. Tick the bus (handles ongoing transactions, arbitration for *next* cycle's grant)
        bus->tick(global_cycle);

        // 2. Tick each core (fetch/execute or handle stalls)
        for (const auto &core : cores)
        {
            if (!core->isFinished())
            {
                core->tick(global_cycle);
            }
        }

        // 3. Check for completion AFTER ticking everything for the current cycle
        if (checkCompletion())
        {
            // Record final cycle counts for each core *before* breaking
            for (int i = 0; i < NUM_CORES; ++i)
            {
                // The stat total_cycles will store this global end cycle.
                statistics.setCoreCycles(i, global_cycle);
            }
            // std::cout << "Simulation finished at cycle " << global_cycle << std::endl;
            break; // All cores finished their traces and resolved pending misses
        }
    }

    // std::cout << "Simulation finished at cycle " << global_cycle << std::endl;
}

void Simulator::printStats() {
    statistics.printFinalStats(
        trace_base_name,
        s,
        E,
        b,
        bus.get() // Pass raw pointer to the Bus object
    );
}

cycle_t Simulator::getMaxCycles() const
{
    // Find the maximum value stored in the statistics total_cycles vector
    cycle_t max_c = 0;
    if (statistics.total_cycles.empty())
        return 0; 

    for (cycle_t c : statistics.total_cycles)
    {
        if (c > max_c)
        {
            max_c = c;
        }
    }
    return max_c; 
}