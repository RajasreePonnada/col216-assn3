#include "core.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

Core::Core(int core_id, const std::string& trace_filename, Cache* l1_cache, Stats* statistics) :
    id(core_id), cache(l1_cache), stats(statistics)
{
    if (!cache) {
        throw std::invalid_argument("Core must have a valid cache pointer.");
    }
    if (!stats) {
        throw std::invalid_argument("Core must have a valid stats pointer.");
    }

    trace_file_ptr = fopen(trace_filename.c_str(), "r");
    if (!trace_file_ptr) {
        throw std::runtime_error("Could not open trace file: " + trace_filename + " (fopen failed)");
    }
}

Core::~Core() {
    if (trace_file_ptr) {
        fclose(trace_file_ptr);
        trace_file_ptr = nullptr;
    }
}

bool Core::isFinished() const {
    return trace_finished && !core_stalled_on_cache && !needs_completion_cycle;
}

bool Core::readAndParseNextAccess() {
    if (!trace_file_ptr || feof(trace_file_ptr)) {
        trace_finished = true;
        return false;
    }

    if (fgets(line_buffer, CORE_LINE_BUFFER_SIZE, trace_file_ptr) == nullptr) {
        trace_finished = true;
        return false;
    }

    char type_char;
    unsigned int read_addr;
    int items_scanned = sscanf(line_buffer, "%c %x", &type_char, &read_addr);

    if (items_scanned == 2) {
        current_access.type = (type_char == 'R' || type_char == 'r') ? Operation::READ : Operation::WRITE;
        if (type_char != 'R' && type_char != 'r' && type_char != 'W' && type_char != 'w') {
            return readAndParseNextAccess();
        }
        current_access.address = static_cast<addr_t>(read_addr);
        return true;
    } else {
        return readAndParseNextAccess();
    }
}

void Core::tick(cycle_t global_cycle) {
    internal_cycle = global_cycle;

    if (needs_completion_cycle) {
        needs_completion_cycle = false;
        core_stalled_on_cache = false;
        processing_access = false;
        return;
    }

    if (core_stalled_on_cache) {
        if (!cache->isStalled()) {
            needs_completion_cycle = true;
            stats->incrementStallCycles(id);
            return;
        } else {
            stats->incrementStallCycles(id);
            return;
        }
    }

    if (!core_stalled_on_cache && !trace_finished) {
        if (!processing_access) {
            if (readAndParseNextAccess()) {
                processing_access = true;
            } else {
                trace_finished = true;
                processing_access = false;
                return;
            }
        }

        if (processing_access) {
            bool hit = cache->access(current_access.address, current_access.type, global_cycle);

            if (hit) {
                processing_access = false;
            } else {
                core_stalled_on_cache = true;
                stats->incrementStallCycles(id);
            }
        }
    }
}
