#ifndef CACHE_SET_H
#define CACHE_SET_H

#include <vector>
#include <limits> // For numeric_limits
#include "cache_line.h"
#include "defs.h"

class CacheSet {
private:
    std::vector<CacheLine> lines;
    int associativity;

public:
    explicit CacheSet(int E) : associativity(E), lines(E) {} // Use explicit

    // Find a line matching the tag, return index (-1 if not found)
    int findLine(addr_t tag) const { // Made const
        for (int i = 0; i < associativity; ++i) {
            // Check isValid first to potentially short-circuit
            if (lines[i].isValid() && lines[i].tag == tag) {
                return i; // Found
            }
        }
        return -1; // Not found
    }

    // Get the index of the LRU line to be replaced
    int getLRUVictim() {
        int lru_index = 0;
        cycle_t min_cycle = std::numeric_limits<cycle_t>::max();
        bool found_valid = false;

        // First pass: Find the minimum lastUsedCycle among valid lines
        for (int i = 0; i < associativity; ++i) {
             if (lines[i].isValid()) {
                 found_valid = true;
                 if (lines[i].lastUsedCycle < min_cycle) {
                    min_cycle = lines[i].lastUsedCycle;
                    lru_index = i;
                 }
             }
        }

        // If no valid lines exist (shouldn't happen if replacement is needed unless set is empty initially)
        // or if multiple lines have the same minimum cycle, the first one found (lru_index) is chosen.
        // The logic prioritizes replacing valid lines based on LRU.
        // If findInvalidLine() is called first, this handles the case where all lines are valid.

        if (!found_valid) {
            // This case should ideally not be reached if getLRUVictim is called only when needed,
            // return 0 as a fallback.
            return 0;
        }

        return lru_index;
    }

    // Update LRU status for the given line index (call on hit or fill)
    void updateLRU(int index, cycle_t currentCycle) {
         if (index >= 0 && index < associativity) {
            lines[index].lastUsedCycle = currentCycle;
         }
    }

     // Get a reference to a specific line
    CacheLine& getLine(int index) {
        return lines[index];
    }
     // Get a const reference to a specific line
    const CacheLine& getLine(int index) const { // Const version
        return lines[index];
    }


    // Check if there is an invalid line available, return index or -1
    int findInvalidLine() const { // Made const
        for (int i = 0; i < associativity; ++i) {
            if (!lines[i].isValid()) {
                return i;
            }
        }
        return -1; // No invalid lines
    }
};

#endif // CACHE_SET_H