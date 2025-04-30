#ifndef CACHE_SET_H
#define CACHE_SET_H

#include <vector>
#include <limits> 
#include "cache_line.h"
#include "defs.h"

class CacheSet {
private:
    std::vector<CacheLine> lines;
    int associativity;

public:
    explicit CacheSet(int E) : associativity(E), lines(E) {}

    // Find a line matching the tag
    int findLine(addr_t tag) const { 
        for (int i = 0; i < associativity; ++i) {
            if (lines[i].isValid() && lines[i].tag == tag) {
                return i; // Found
            }
        }
        return -1; 
    }


    int getLRUVictim() {
        int lru_index = 0;
        cycle_t min_cycle = std::numeric_limits<cycle_t>::max();
        bool found_valid = false;


        for (int i = 0; i < associativity; ++i) {
             if (lines[i].isValid()) {
                 found_valid = true;
                 if (lines[i].lastUsedCycle < min_cycle) {
                    min_cycle = lines[i].lastUsedCycle;
                    lru_index = i;
                 }
             }
        }


        if (!found_valid) {
            return 0;
        }

        return lru_index;
    }

    void updateLRU(int index, cycle_t currentCycle) {
         if (index >= 0 && index < associativity) {
            lines[index].lastUsedCycle = currentCycle;
         }
    }

    CacheLine& getLine(int index) {
        return lines[index];
    }
    const CacheLine& getLine(int index) const { 
        return lines[index];
    }

    int findInvalidLine() const {
        for (int i = 0; i < associativity; ++i) {
            if (!lines[i].isValid()) {
                return i;
            }
        }
        return -1; // No invalid lines
    }
};

#endif 