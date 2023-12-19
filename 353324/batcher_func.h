#ifdef _TO_USE_BATCHER_

#ifndef _BATCHER_H_
#define _BATCHER_H_

#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

#include "structs.h"
#include "macros.h"
#include <tm.h>

static inline Segment* findSegment(const Region * region, const void* source) {
    Segment* seg = region -> allocs;
    while(seg != NULL) {
        if (((Word*)seg -> data) <= ((Word*)source )
            && ((Word*)source) < (seg -> data) + seg -> size) {
            return seg;
        }
        seg = seg -> next;
    }
    return NULL;
}

static inline void* Undo(const Region * region, const tx_t tx) {
    for (Segment* segment = region -> allocs; segment != NULL; segment = segment -> next) {
        if (atomic_load(&(segment -> to_delete)) )
            continue;
        if (atomic_load(&(segment -> creator)) == tx) {
            // tm_free(region, tx, segment); 
            atomic_store(&(segment -> to_delete), 1); 
            continue; 
        }
        for (size_t i = 0; i < segment -> size; ++i) {
            atomic_tx * control = segment -> control + i;
            if (atomic_load(control) == tx) {
                atomic_store(control, it_is_free);
                // undo the write
                memcpy(segment -> shadow + i, segment -> data + i, sizeof(Word));
            } 
            else {
                // if we did the read
                tx_t we_read_tx = tx + batch_size;
                atomic_compare_exchange_strong(control, &we_read_tx, it_is_free);
            }
        }
    }
    tm_end(region, tx);
}

static inline bool try_write(const Region *region, const Segment* seg, tx_t tx, const void* target, const size_t size) {
    ulong offset = ((uintptr_t)target - (uintptr_t)seg -> data)/sizeof(Word);
    for (size_t i = 0; i < size; ++i) {
        atomic_tx * control = seg -> control + offset + i;
        if (atomic_load(control) != it_is_free 
            && atomic_load(control) != tx 
            && atomic_load(control) != tx + batch_size
            ) {
            return false;
        }
    }
    for (size_t i = 0; i < size; ++i) {
        atomic_tx * control = seg -> control + offset + i;
        atomic_store(control, tx);
    }
    return true;
}

#endif

#endif