#ifdef _TO_USE_BATCHER_

#ifndef _BATCHER_H_
#define _BATCHER_H_

// #define _DEBUG_FLZ_
// #define _DEBUG_FLZ_TEST_READ_
// #define _DEBUG_FLZ_TEST_WRITE_
// #define _DEBUG_FLZ_TEST_LOCK_
// #define _DEBUG_FLZ_TEST_UNDO_

#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

#include "structs.h"
#include "macros.h"
#include "Mytm.h"

static inline Segment* findSegment(const Region * region, const void* source) {
        #ifdef _DEBUG_FLZ_TEST_FIND_
        printf("Looking for %p\n", source);

        printf("region -> start -> data: %p\n", region -> start -> data);
        #endif

    if ((Word*)(region -> start -> data) <= (Word*)source
            && (Word*)source < (Word*)region -> start -> shadow)
        // && (Word*)source < (Word*)region -> ((Word*)(start -> data)) 
        //                                            + region -> start -> size)
        {
            return region -> start; 
        }

    Segment* seg = region -> allocs;
    while(seg != NULL) {
            #ifdef _DEBUG_FLZ_TEST_FIND_
            printf("Segment -> data: %p\n", seg -> data); 
            #endif

        if ((Word* )seg -> data <= (Word*)source 
             && (Word*)source < (Word*)seg -> shadow)
        {
            return seg;
        }
        seg = seg -> next;
    }

    return NULL;
}

static inline void Undo_seg(Segment* segment, const tx_t tx, const size_t step) {
        #ifdef _DEBUG_FLZ_TEST_UNDO_
        printf("Undoing segment %p\n", segment);
        #endif
    if (atomic_load(&(segment -> to_delete)) )
        return;
    if (atomic_load(&(segment -> creator)) == tx) {
        // tm_free(region, tx, segment); 
        atomic_store(&(segment -> to_delete), 1); 
        return; 
    }

    // printf("segment -> size: %lu\n", segment -> size);
    for (size_t i = 0; i < segment -> size; i += step) {
        char * control = segment -> control + i;
        if (atomic_load(control) == tx) {

            #ifdef _DEBUG_FLZ_TEST_UNDO_
            printf("j: %lu\n", i/8);
            #endif

            // undo the write
            memcpy(segment -> shadow + i, segment -> data + i, sizeof(Word) * step);
            atomic_store(control, it_is_free);
        } 
        else {
            // if we did the read
            char we_read_tx = tx + batch_size;
            atomic_compare_exchange_strong(control, &we_read_tx, it_is_free);
                #ifdef _DEBUG_FLZ_TEST_UNDO_
                if (we_read_tx == tx + batch_size) {
                    printf("!j: %lu\n", i/8);
                }
                #endif
        }
    }
}

static inline void Undo(Region * region, const tx_t tx) {
        #ifdef _DEBUG_FLZ_TEST_UNDO_
        printf("Undoing %lu\n", tx);
        printf("Undoing %lu\n", tx + batch_size);
        #endif


    Undo_seg(region -> start, tx, region -> align);
    for (Segment* segment = region -> allocs; segment != NULL; segment = segment -> next) {
        Undo_seg(segment, tx, region -> align);
    }
    tm_end((void*)region, tx);
}

static inline void Commit_seg(Region* region, Segment* seg) {
    if (atomic_load(&(seg -> to_delete))){
            #ifdef _DEBUG_FLZ_TEST_UNDO_
            printf("Undoing segment %p\n", seg);
            #endif

        // tm_free(region, seg -> creator, seg); 
        // remove from linked list
        if (seg -> previous) 
            seg -> previous -> next = seg -> next;
        else 
            region -> allocs = seg -> next;
        if (seg -> next) 
            seg -> next -> previous = seg -> previous;

        // print("freeing segment %x\n", seg);
        // free(seg -> data);
        free(seg);
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
    memset(seg -> control, 0, seg -> size * sizeof(char));
    // and it will not get reset in the following epoches
    atomic_store(&(seg -> creator), it_is_free); 
}

static inline bool try_write(Region * region, Segment* seg, tx_t tx, void* target, const size_t size) {
    ulong offset = ((uintptr_t)target - (uintptr_t)seg -> data)/sizeof(Word);

        // #ifdef _DEBUG_FLZ_TEST_UNDO_
        // printf("\ncurrent tx: %lu\n", tx);
        // printf("control of: %p is: ", target);
        // printf("%lu\n", atomic_load(seg -> control + offset));
        // // for (size_t i = 0; i < size; ++i) {
        // //     printf("%lu", atomic_load(seg -> control + offset + i));
        // // }
        // puts(""); 
        // #endif

    size_t step = region -> align; 
    for (size_t i = 0; i < size; i += step) {
        char * control = seg -> control + offset + i;
        char expected1 = it_is_free, expected2 = tx + batch_size;

        if (!(atomic_compare_exchange_strong(control, &expected1, tx) 
            || expected1 == tx 
            || atomic_compare_exchange_strong(control, &expected2, tx)))
        {
          if (i > 1)
          {
            bzero(seg -> control + offset, (i - 1) * sizeof(char));
          }

          // Someone else has already locked the word
          return false;
        }

    }


        // #ifdef _DEBUG_FLZ_TEST_UNDO_
        // printf("[after]control of: %p is: ", target);
        // for (size_t i = 0; i < size; ++i) {
        //     printf("%lu", atomic_load(seg -> control + offset + i));
        // }
        // puts(""); 
        // #endif
    return true;
}

#endif

#endif