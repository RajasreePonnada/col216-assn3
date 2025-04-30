#ifndef CORE_H
#define CORE_H

#include <string>
#include <cstdio> // For FILE*, fopen, fgets, sscanf, fclose
#include <memory>
#include "defs.h"
#include "cache.h"
#include "stats.h"

// Optimization done By Defining a buffer size for reading lines
#define CORE_LINE_BUFFER_SIZE 256 // will be enough for "R/W 0xADDRESS\n\0"

class Core
{
private:
    int id;
    Cache *cache; // Pointer to its L1 cache
    Stats *stats; // Pointer to global stats object

    FILE *trace_file_ptr = nullptr;
    char line_buffer[CORE_LINE_BUFFER_SIZE];

    bool trace_finished = false;
    cycle_t internal_cycle = 0;

    bool core_stalled_on_cache = false;
    // Flag for post-miss completion cycle
    bool needs_completion_cycle = false;

    MemAccess current_access;
    bool processing_access = false;

    bool readAndParseNextAccess();

public:
    Core(int core_id, const std::string &trace_filename, Cache *l1_cache, Stats *statistics);
    ~Core();

    // Execute one cycle worth of work for this core
    void tick(cycle_t global_cycle);

    bool isFinished() const;
    cycle_t getCycle() const { return internal_cycle; } // Return cycles processed by this core
};

#endif