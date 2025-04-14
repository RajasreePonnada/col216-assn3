#include "core.h"
#include <iostream>
#include <stdexcept>
#include <cstring> // For memset or potentially manual parsing helpers

Core::Core(int core_id, const std::string& trace_filename, Cache* l1_cache, Stats* statistics) :
    id(core_id), cache(l1_cache), stats(statistics)
{
    if (!cache) {
        throw std::invalid_argument("Core must have a valid cache pointer.");
    }
     if (!stats) {
        throw std::invalid_argument("Core must have a valid stats pointer.");
    }

    // --- Optimization: Use fopen ---
    trace_file_ptr = fopen(trace_filename.c_str(), "r");
    if (!trace_file_ptr) {
         // Combine error message creation
         throw std::runtime_error("Could not open trace file: " + trace_filename + " (fopen failed)");
    }
    // --- End Optimization ---
}

Core::~Core() {
    // --- Optimization: Use fclose ---
    if (trace_file_ptr) {
        fclose(trace_file_ptr);
        trace_file_ptr = nullptr; // Good practice
    }
    // --- End Optimization ---
}

bool Core::isFinished() const {
    // Finished if trace is done AND the core is not stalled processing the last instruction
    // trace_finished flag is now the primary indicator of EOF
    return trace_finished && !core_stalled_on_cache;
}


// --- Optimization: New function to read and parse using fgets/sscanf ---
bool Core::readAndParseNextAccess() {
    if (!trace_file_ptr || feof(trace_file_ptr)) {
        trace_finished = true;
        return false;
    }

    // Read next line directly into buffer
    if (fgets(line_buffer, CORE_LINE_BUFFER_SIZE, trace_file_ptr) == nullptr) {
        // Either EOF or read error
        trace_finished = true;
        // Check for actual error vs just EOF
        // if (ferror(trace_file_ptr)) {
        //     std::cerr << "Error reading trace file for core " << id << std::endl;
        // }
        return false;
    }

    // Parse the line using sscanf
    char type_char;
    unsigned int read_addr; // Use unsigned int to match %x

    // Use %c for the operation type, skip whitespace, then %x for hex address
    // The space in " %x" skips leading whitespace before the address.
    int items_scanned = sscanf(line_buffer, "%c %x", &type_char, &read_addr);

    if (items_scanned == 2) { // Successfully parsed both items
        current_access.type = (type_char == 'R' || type_char == 'r') ? Operation::READ : Operation::WRITE;
        // Assuming type is always R or W, could add validation.
        if (type_char != 'R' && type_char != 'r' && type_char != 'W' && type_char != 'w') {
             // std::cerr << "Warning: Invalid operation type '" << type_char << "' in trace for core " << id << std::endl;
             // Treat as read? Or skip? Let's skip.
             return readAndParseNextAccess(); // Try reading the *next* line
        }
        current_access.address = static_cast<addr_t>(read_addr); // Cast to our addr_t type
        return true; // Success
    } else {
        // Parsing failed - could be empty line, comment, malformed line
        // std::cerr << "Warning: Skipping malformed trace line in core " << id << ": " << line_buffer << std::endl;
        // Try reading the next line recursively
        return readAndParseNextAccess();
    }
}
// --- End Optimization ---


void Core::tick(cycle_t global_cycle) {
    internal_cycle = global_cycle; // Update internal view of time

    // 1. Check if we were stalled and if the cache is now ready
    if (core_stalled_on_cache) {
        if (!cache->isStalled()) {
            // Cache finished the request that caused the stall.
            core_stalled_on_cache = false;
            processing_access = false; // The pending access is now complete.
            // Ready to fetch next instruction in the *next* cycle.
        } else {
            // Still stalled, record stall cycle for stats
            stats->incrementStallCycles(id);
            return; // Do nothing else this cycle
        }
    }

    // 2. If not stalled and not finished, try to fetch and execute next instruction
    if (!core_stalled_on_cache && !trace_finished) {
        // If we are not currently processing an access (i.e., last one hit or just unstalled)
        // then read and parse the next line from the trace.
        if (!processing_access) {
            if (readAndParseNextAccess()) { // Use the optimized function
                processing_access = true; // Mark that we are attempting this access
            } else {
                // readAndParseNextAccess returned false, meaning EOF or error
                trace_finished = true; // Ensure flag is set
                processing_access = false;
                return; // No more instructions
            }
        }

        // If we have an access to process this cycle
        if (processing_access) {
            // Issue access to cache. Cache::access returns true for hit, false for miss.
            bool hit = cache->access(current_access.address, current_access.type, global_cycle);

            if (hit) {
                // Hit! Instruction completes this cycle.
                processing_access = false; // Ready for next instruction next cycle.
            } else {
                // Miss! Cache initiated miss handling. Core must stall.
                core_stalled_on_cache = true;
                stats->incrementStallCycles(id); // Record the first stall cycle
                // processing_access remains true, we retry this access when unstalled (implicitly handled by stall logic)
            }
        }
    }
}