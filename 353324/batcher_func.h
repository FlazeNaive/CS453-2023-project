#ifdef _TO_USE_BATCHER_

#ifndef _BATCHER_H_
#define _BATCHER_H_

#define _DEBUG_FLZ_


#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

#include "structs.h"
#include "macros.h"
#include <tm.h>

static inline Segment* findSegment(const Region * region, const void* source) {
    // printf("Looking for %p\n", source);


    Segment* seg = region -> allocs;

    if ((Word*)region -> start <= (Word*)source
        && (Word*)source < (Word*)region -> start + region -> start -> size) {
            return region -> start; 
        }
    while(seg != NULL) {
            #ifdef _DEBUG_FLZ_
            printf("Segment -> data: %p\n", seg -> data); 
            #endif
        if (((Word*)seg -> data) <= ((Word*)source )
            && ((Word*)source) < (seg -> data) + seg -> size) {
            return seg;
        }
        seg = seg -> next;
    }
    return NULL;
}

static inline void Undo_seg(Segment* segment, const tx_t tx) {
    if (atomic_load(&(segment -> to_delete)) )
        return;
    if (atomic_load(&(segment -> creator)) == tx) {
        // tm_free(region, tx, segment); 
        atomic_store(&(segment -> to_delete), 1); 
        return; 
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

static inline void Undo(Region * region, const tx_t tx) {
    // printf("Undoing %lu\n", tx);
    // printf("Undoing %lu\n", -tx);
    Undo_seg(region -> start, tx);
    for (Segment* segment = region -> allocs; segment != NULL; segment = segment -> next) {
        Undo_seg(segment, tx);
    }
    tm_end((void*)region, tx);
}

static inline void Commit_seg(Region* region, Segment* seg) {
    if (atomic_load(&(seg -> to_delete))){
        tm_free(region, seg -> creator, seg); 
        return; 
    }
    // commit all writes
    // from shadow to data


        // #ifdef _DEBUG_FLZ_
        // printf("\n\nsegment address: %p\n", seg);
        // printf("size(Segment Header): %lu\n", sizeof(Segment));
        // printf("segment -> size: %lu\n", seg -> size);
        // printf("size(Word): %lu\n", sizeof(Word));
        // printf("Commiting %p -> %p\n", seg->shadow, seg -> data);
        // #endif


    memcpy(seg -> data, seg -> shadow, seg -> size * sizeof(Word));
    // and reset control
    memset(seg -> control, 0, seg -> size * sizeof(tx_t));
    // and it will not get reset in the following epoches
    atomic_store(&(seg -> creator), it_is_free); 
}

static inline bool try_write(Region * unused(region), Segment* seg, tx_t tx, void* target, const size_t size) {
    ulong offset = ((uintptr_t)target - (uintptr_t)seg -> data)/sizeof(Word);

        #ifdef _DEBUG_FLZ_TEST_LOCK_
        printf("current tx: %lu\n", tx);
        printf("control of: %p is: ", target);
        for (size_t i = 0; i < size; ++i) {
            printf("%lu", atomic_load(seg -> control + offset + i));
        }
        puts(""); 
        #endif

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

        #ifdef _DEBUG_FLZ_TEST_LOCK_
        printf("[after]control of: %p is: ", target);
        for (size_t i = 0; i < size; ++i) {
            printf("%lu", atomic_load(seg -> control + offset + i));
        }
        puts(""); 
        #endif
    return true;
}

#endif

#endif