#ifndef CACHE_LINE_H
#define CACHE_LINE_H

#include "defs.h"

struct CacheLine {
    MESIState state = MESIState::INVALID;
    addr_t tag = 0;
    cycle_t lastUsedCycle = 0; // For LRU tracking

    // No actual data stored, just tag and state

    CacheLine() = default; // Default constructor

    bool isValid() const {
        return state != MESIState::INVALID;
    }
};

#endif // CACHE_LINE_H