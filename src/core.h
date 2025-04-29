#ifndef CORE_H
#define CORE_H

#include <string>
#include <cstdio> // For FILE*, fopen, fgets, sscanf, fclose
#include <memory>
#include "defs.h"
#include "cache.h"
#include "stats.h"

// Optimization: Define a buffer size for reading lines
#define CORE_LINE_BUFFER_SIZE 256 // Should be enough for "R/W 0xADDRESS\n\0"

class Core {
private:
    int id;
    Cache* cache; // Pointer to its L1 cache
    Stats* stats; // Pointer to global stats object

    // --- Optimization: C-style I/O ---
    FILE* trace_file_ptr = nullptr;
    char line_buffer[CORE_LINE_BUFFER_SIZE]; // Reusable buffer for fgets
    // --- End Optimization ---

    bool trace_finished = false;
    cycle_t internal_cycle = 0; // Tracks cycles this core has been active/simulated

    // Stall tracking
    bool core_stalled_on_cache = false; // Is the core itself stalled?
     // *** ADDED: Flag for post-miss completion cycle ***
     bool needs_completion_cycle = false;
     
    MemAccess current_access; // The access being processed or that caused stall
    bool processing_access = false; // Are we currently trying to execute an access?
    

    // --- Optimization: Helper to get next access ---
    bool readAndParseNextAccess();
    // --- End Optimization ---

public:
    Core(int core_id, const std::string& trace_filename, Cache* l1_cache, Stats* statistics);
    ~Core(); // Need to close the file pointer

    // Execute one cycle worth of work for this core
    void tick(cycle_t global_cycle);

    bool isFinished() const;
    cycle_t getCycle() const { return internal_cycle; } // Return cycles processed by this core

};

#endif // CORE_H