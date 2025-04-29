#include "cache.h"
#include <stdexcept>
#include <iostream> // For debugging

Cache::Cache(int core_id, unsigned int s, unsigned int E, unsigned int b, Bus* shared_bus, Stats* statistics) :
    id(core_id),
    num_sets(s == 0 ? 1 : (1 << s)), // Handle s=0 for fully associative (1 set)
    associativity(E),
    block_size(1 << b),
    block_bits(b),
    set_bits(s),
    sets(num_sets, CacheSet(E)), // Initialize sets vector
    bus(shared_bus),
    stats(statistics)
{
    if (bus == nullptr || stats == nullptr) {
        throw std::invalid_argument("Cache requires valid Bus and Stats pointers.");
    }
    if (E == 0 || block_size == 0) {
         throw std::invalid_argument("Associativity and Block Size must be non-zero.");
    }
     bus->registerCache(this); // Register with the bus
}

// --- Address calculation helpers ---
addr_t Cache::getTag(addr_t address) const {
    if ((set_bits + block_bits) >= 32) return 0; // Avoid overshift
    return address >> (set_bits + block_bits);
}

unsigned int Cache::getIndex(addr_t address) const {
    if (set_bits == 0) return 0; // Fully associative case
    addr_t mask = (1 << set_bits) - 1;
    return (address >> block_bits) & mask;
}

addr_t Cache::getBlockAddress(addr_t address) const {
     if (block_bits >= 32) return 0; // Avoid potential issues with mask
     addr_t mask = ~((1ULL << block_bits) - 1);
     return address & mask;
}

addr_t Cache::reconstructAddress(addr_t tag, unsigned int index) const {
     return (tag << (set_bits + block_bits)) | (index << block_bits);
}

// --- Core facing access method ---
bool Cache::access(addr_t address, Operation op, cycle_t current_cycle) {
    if (stalled) {
         // Should not be called by core if stalled, but safety check
         std::cerr << "Warning: Core " << id << " accessed cache while stalled!" << std::endl;
         return false; // Indicate still stalled / miss ongoing
    }

    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    stats->recordAccess(id, op); // Record access attempt

    int way_index = sets[index].findLine(tag);

    if (way_index != -1) { // Potential Hit - Line exists
        CacheLine& line = sets[index].getLine(way_index);
        MESIState current_state = line.state;

        if (op == Operation::READ) {
            // Read Hit on S, E, or M state
            sets[index].updateLRU(way_index, current_cycle); // Update LRU on hit
            // State does not change (M remains M, E remains E, S remains S)
            return true; // HIT
        } else { // WRITE Access
            if (current_state == MESIState::MODIFIED) {
                // Write Hit on M state
                sets[index].updateLRU(way_index, current_cycle);
                return true; // HIT (already exclusive and dirty)
            } else if (current_state == MESIState::EXCLUSIVE) {
                // Write Hit on E state -> transition to M
                line.state = MESIState::MODIFIED;
                sets[index].updateLRU(way_index, current_cycle);
                return true; // HIT (silently transition E->M)
            } else { // current_state == MESIState::SHARED
                // Write Hit requires upgrade S -> M
                // std::cout << "DEBUG Core " << id << " Cycle " << current_cycle << ": Write HIT to S state Addr=" << std::hex << address << std::dec << ". Calling handleMiss for upgrade." << std::endl; // ADDED
                stats->recordMiss(id);
                stalled = true;
                handleMiss(address, index, tag, op, current_cycle);
                return false;
            }
        }
    } else { // Definite MISS - Line not present (State I)
        // std::cout << "DEBUG Core " << id << " Cycle " << current_cycle << ": " << (op==Operation::WRITE?"Write":"Read") << " MISS (State I) Addr=" << std::hex << address << std::dec << ". Calling handleMiss." << std::endl; // ADDED
        stats->recordMiss(id);
        stalled = true;
        handleMiss(address, index, tag, op, current_cycle);
        return false; // MISS
    }
}


// --- Internal Miss Handling Logic ---
// --- Internal Miss Handling Logic (Corrected) ---
void Cache::handleMiss(addr_t address, unsigned int index, addr_t tag, Operation op, cycle_t current_cycle) {
    addr_t block_addr = getBlockAddress(address);

    // Check if already handling a miss for this block
    if (pending_requests.count(block_addr)) {
        return; // Already waiting
    }

    // --- Handle Write Hit to Shared State (S->M Upgrade) ---
    int existing_way = sets[index].findLine(tag);
    if (op == Operation::WRITE && existing_way != -1 && sets[index].getLine(existing_way).state == MESIState::SHARED) {
        // This is a Coherence Miss requiring BusUpgr

        PendingRequest pending;
        pending.original_op = op;
        pending.target_way = existing_way; // Target the existing line
        pending.request_init_cycle = current_cycle;
        pending_requests[block_addr] = pending;
        // std::cout << "DEBUG Core " << id << " Cycle " << current_cycle << ": handleMiss issuing BusUpgr for Addr=" << std::hex << block_addr << std::dec << std::endl;
        // Issue BusUpgr
        BusRequest bus_req;
        bus_req.requestingCoreId = id;
        bus_req.type = BusTransaction::BusUpgr;
        bus_req.address = block_addr;
        bus_req.request_cycle = current_cycle;
        bus->addRequest(bus_req);

        // Core is stalled, waiting for BusUpgr completion signal
        return; // Exit after handling S->M upgrade
    }

    // --- Handle Read Miss (I->S or I->E) OR Write Miss (I->M) ---
    // Both require allocating a block and fetching data via BusRd or BusRdX

    int target_way = -1; // Way index where the new block will be placed
    allocateBlock(block_addr, index, tag, target_way, current_cycle); // Finds/evicts way, handles WB initiation

    if (target_way == -1) {
        std::cerr << "Error: Core " << id << " could not allocate block for miss addr " << std::hex << address << std::dec << "!" << std::endl;
        stalled = false; // Try to recover? Needs careful thought.
        return;
    }

    // Create the *single* pending request entry for this miss
    PendingRequest pending;
    pending.original_op = op;
    pending.target_way = target_way; // Store where the data should go
    pending.request_init_cycle = current_cycle;
    pending_requests[block_addr] = pending; // Track the miss

    // Issue the *single* appropriate bus request for the data fetch
    BusRequest bus_req;
    bus_req.requestingCoreId = id;
    // Use BusRdX for Write Miss (I->M), BusRd for Read Miss (I->S/E)
    bus_req.type = (op == Operation::READ) ? BusTransaction::BusRd : BusTransaction::BusRdX;
    // std::cout << "DEBUG Core " << id << " Cycle " << current_cycle << ": handleMiss issuing " << (bus_req.type == BusTransaction::BusRd ? "BusRd" : "BusRdX") << " for Addr=" << std::hex << block_addr << std::dec << std::endl; // ADDED
    bus_req.address = block_addr;
    bus_req.request_cycle = current_cycle;
    bus->addRequest(bus_req);

    // Core is stalled waiting for BusRd/BusRdX completion (stalled = true was set in Cache::access)
    // No return needed here, function ends.
}


// --- Block Allocation and Eviction ---
// Finds a way (invalid or LRU victim), initiates WB if needed, returns chosen way index.
void Cache::allocateBlock(addr_t block_addr, unsigned int index, addr_t tag, int& way_index, cycle_t current_cycle) {
    way_index = sets[index].findInvalidLine(); // Prefer invalid lines

    if (way_index == -1) { // No invalid lines, must evict
        way_index = sets[index].getLRUVictim();
        CacheLine& victim_line = sets[index].getLine(way_index);

        if (victim_line.isValid()) { // Should always be true if findInvalidLine failed
            stats->recordEviction(id); // Record eviction of a valid block

            if (victim_line.state == MESIState::MODIFIED) {
                // Initiate Writeback for the Modified victim block
                addr_t victim_addr = reconstructAddress(victim_line.tag, index);
                 initiateWriteback(victim_addr, index, way_index, current_cycle);
                 // The writeback request is now queued on the bus.
            }
        }
    }
z
    // Mark the chosen way as invalid temporarily until data arrives
    // Set tag now so reconstructAddress works if needed for WB, but state is key.
    CacheLine& target_line = sets[index].getLine(way_index);
    target_line.state = MESIState::INVALID; // Mark as invalid (or a temporary pending state)
    target_line.tag = tag; // Pre-set the tag for the incoming block
    target_line.lastUsedCycle = current_cycle; // Update LRU for the allocated spot
}


// --- Writeback Initiation ---
void Cache::initiateWriteback(addr_t victim_address, unsigned int victim_set_index, int victim_way_index, cycle_t current_cycle) {
    stats->recordWriteback(id); // Record WB initiation

    BusRequest wb_req;
    wb_req.requestingCoreId = id;
    wb_req.type = BusTransaction::Writeback;
    wb_req.address = victim_address; // Use the actual victim block address
    wb_req.request_cycle = current_cycle;
    bus->addRequest(wb_req);

    // Optional: Mark the line with a transient state like "Invalid-PendingWriteback"?
    // For simplicity, MESI INVALID state is sufficient as long as WB is queued.
    // sets[victim_set_index].getLine(victim_way_index).state = MESIState::INVALID; // Done by caller (allocateBlock)
}


// --- Snooping Logic ---
SnoopResult Cache::snoopRequest(BusTransaction transaction, addr_t address, cycle_t current_cycle) {
    SnoopResult result;
    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    int way_index = sets[index].findLine(tag);

    if (way_index != -1) { // Line exists in this cache
        CacheLine& line = sets[index].getLine(way_index);
        MESIState current_state = line.state;

        switch (transaction) {
            case BusTransaction::BusRd: // Other core reading
                if (current_state == MESIState::MODIFIED) {
                    // Provide data, transition M -> S

                    addr_t my_block_addr = reconstructAddress(line.tag, index);
                    initiateWriteback(my_block_addr, index, way_index, current_cycle); // Initiate WB for M block
                    line.state = MESIState::SHARED;
                    result.data_supplied = true;
                    result.was_dirty = true; // Indicate data came from M state
                    // Implicit writeback happens conceptually; bus traffic for WB handled separately if needed.
                    // stats->recordWriteback(id); // Don't record WB here, only on actual eviction WB.
                } else if (current_state == MESIState::EXCLUSIVE) {
                    // Provide data, transition E -> S
                    line.state = MESIState::SHARED;
                    result.data_supplied = true;
                }
                // If Shared: Remain S, don't supply data (memory supplies or M/E cache does)
                // If Invalid: Remain I
                break;

            case BusTransaction::BusRdX: // Other core writing
                if (current_state == MESIState::MODIFIED) {
                    result.data_supplied = true; // Provide data (last copy)
                    result.was_dirty = true;
                    line.state = MESIState::INVALID; // Invalidate self
                    // std::cout << "Core " << id << ": Invalidating line on BusRdX-M" << std::endl;
                    stats->recordInvalidation();
                } else if (current_state == MESIState::EXCLUSIVE) {
                    result.data_supplied = true; // Provide data
                    line.state = MESIState::INVALID;
                    // std::cout << "Core " << id << ": Invalidating line on BusRdX-E" << std::endl;
                    stats->recordInvalidation();
                } else if (current_state == MESIState::SHARED) {
                    // Don't supply data
                    line.state = MESIState::INVALID;
                    stats->recordInvalidation();
                    // std::cout << "Core " << id << ": Invalidating line on BusRdX-S" << std::endl;
                }
                // If Invalid: Remain I
                break;

            case BusTransaction::BusUpgr: // Other core upgrading S -> M
                 if (current_state == MESIState::SHARED) {
                     line.state = MESIState::INVALID;
                     stats->recordInvalidation();
                    //    << "Core " << id << ": Invalidating line on BusUpgr" << std::endl;
                 }
                 // Ignore if M, E, I. (Shouldn't receive BusUpgr if M/E)
                 break;

            case BusTransaction::Writeback: // Observing another core's WB
                // No state change needed based on MESI for observing WB.
                break;
             case BusTransaction::NoTransaction:
                 // Do nothing
                 break;
        }
        // LRU NOT updated on snoop hits.
    }

    // Set is_shared flag in result (needed by bus to determine final state)
    // If the line still exists and is not Invalid after the transition above, it contributes to sharing
    if (way_index != -1 && sets[index].getLine(way_index).isValid()) {
         result.is_shared = true; // This cache still holds a valid copy (S or potentially M if BusRdX failed?)
         // However, Bus logic should determine overall shared status based on all snoop results.
    }


    return result;
}

// Helper used by Bus to determine final state on BusRd completion
bool Cache::isBlockShared(addr_t address) {
    addr_t block_addr = getBlockAddress(address);
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);

    int way_index = sets[index].findLine(tag);
    if (way_index != -1) {
        // If line exists and is NOT Invalid, it's considered shared for this purpose
        return sets[index].getLine(way_index).isValid();
    }
    return false;
}


// --- Bus Completion Handling ---
void Cache::handleBusCompletion(const BusRequest& completed_request, cycle_t current_cycle) {

    // Handle Writeback completion - Just acknowledge, no state change needed usually
    if (completed_request.type == BusTransaction::Writeback) {
         // Find if this WB corresponds to a pending miss that caused it
         // Not strictly necessary, WB completes independently.
         // Log potentially: std::cout << "Core " << id << ": WB complete addr " << std::hex << completed_request.address << std::dec << std::endl;
         // Remove from pending_requests if tracked? No, WB is separate.
         return; // WB completion doesn't un-stall the core waiting for data.
    }

    addr_t block_addr = completed_request.address;

    // Find the pending request entry for this block address
    auto pending_it = pending_requests.find(block_addr);
    if (pending_it == pending_requests.end()) {
        // Not waiting for this block, or already handled. Could be a completed WB.
        // std::cerr << "Warning: Core " << id << " received unexpected bus completion for addr " << std::hex << block_addr << std::dec << std::endl;
        return;
    }

    PendingRequest& pending = pending_it->second;
    unsigned int index = getIndex(block_addr);
    addr_t tag = getTag(block_addr);
    int way_index = pending.target_way; // Get the allocated way

    if (way_index < 0 || way_index >= associativity) {
        std::cerr << "Error: Invalid target way index in pending request for Core " << id << ", Addr " << std::hex << block_addr << std::dec << std::endl;
        pending_requests.erase(pending_it); // Clean up invalid pending request
        stalled = false; // Try to unstall
        return;
    }

    CacheLine& line = sets[index].getLine(way_index);

    // Determine final MESI state based on the completed transaction type
    // and whether the bus indicated the block was shared (via snoop results).
    // The Bus needs to pass this info. Let's assume Bus::tick calls this *after*
    // combining snoop results. We need access to that combined result.
    // --> Redesign: Bus::tick should pass the combined SnoopResult to handleBusCompletion.
    // --> Current Design: Infer based on request type. This is less accurate for BusRd.
    
    bool shared_after_snoop = false; // Placeholder - Bus should determine this
    switch (completed_request.type) {
        case BusTransaction::BusRd:
            // Infer E vs S: If Bus logic determined no other sharers, it's E. Otherwise S.
            // Simplification: Assume S unless we implement the check in Bus.
            // Assume Bus passes combined snoop result somehow or we check locally:
             // Let's query the bus or rely on snoop result passed in (requires API change)
             // Quick check: Did *we* snoop anyone having it? (Not available here easily)
             // Fallback: Assume Shared unless it's the first copy. Assume S for now.
             line.state = MESIState::SHARED; // Could be EXCLUSIVE! Needs Bus input.
             line.tag = tag;
             break;

        case BusTransaction::BusRdX:
            line.state = MESIState::MODIFIED; // Comes in Modified state after RdX
            line.tag = tag;
            break;

        case BusTransaction::BusUpgr:
             // Should have been S, now becomes M
             if (line.tag == tag) { // Ensure it's the correct line
                 line.state = MESIState::MODIFIED;
             } else {
                 std::cerr << "Error: BusUpgr completion for incorrect tag?" << std::endl;
             }
            break;
        default:
            // Should not be called for other types like WB here.
            break;
    }

    // Update LRU for the newly filled/upgraded line
    sets[index].updateLRU(way_index, current_cycle);

    // Remove the completed request from pending map
    pending_requests.erase(pending_it);

    // Un-stall the core *only if* no other misses are pending for this cache
    // In this simple model, we only track one miss at a time effectively causing stalls.
    // Check if map is empty. If using MSHRs later, check specific core stall flag.
    if (pending_requests.empty()) {
        stalled = false;
    }
    // else: Still stalled waiting for other pending requests (if MSHR model used)
    // In current model, stalled should become false.

}