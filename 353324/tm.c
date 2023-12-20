/**
 * @file   tm.c
 * @author Qingyi HE <qingyi.he@epfl.ch>
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

#define _TO_USE_BATCHER_ 
// #define _DEBUG_FLZ_ 

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

// Internal headers
#include <tm.h>

#include "structs.h"
#include "batcher_func.h"
#include "macros.h"
#include "shared-lock.h"

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/

shared_t tm_create(size_t unused(size), size_t unused(align)) {

    #ifdef _DEBUG_FLZ_
    // printf("STARTING my CREATE\n\n");
    #endif 

    align = align < sizeof(void*) ? sizeof(void*) : align;

    if (unlikely(size % align != 0))
        return invalid_shared;

    Region* region = (Region*)malloc(sizeof(Region));
    if (unlikely(!region)) {
        return invalid_shared;
    }


    // alloc the start
    if (unlikely(posix_memalign((void**)&region -> start, align, sizeof(Segment) 
                                                     + sizeof(atomic_tx) * size
                                                     + sizeof(Word) * size * 2) != 0)) 
    {
        free(region);
        return invalid_shared;
    }

    memset(region -> start, 0, sizeof(Segment) 
                   + sizeof(atomic_tx) * size 
                   + sizeof(Word) * size * 2 );

    region -> start -> data    =      (Word*)((uintptr_t)region -> start + sizeof(Segment));
    region -> start -> shadow  =      (Word*)((uintptr_t)region -> start -> data + sizeof(Word) * size);
    region -> start -> control = (atomic_tx*)((uintptr_t)region -> start -> shadow + sizeof(Word) * size);
    // memset(seg -> data, 0, size * sizeof(Word));

    // add creator and size
    atomic_store(&(region -> start -> creator), it_is_free);
    region -> start -> size = size;

    // if (unlikely(posix_memalign(&(region -> start), align, size) != 0)) {
    //     free(region);
    //     return invalid_shared;
    // }
    // memset(region -> start, 0, size);



    region -> align = align;
    region -> size = size;
    region -> allocs = NULL; 

    if (!shared_lock_init(&(region->lock))) {
        free(region->start);
        free(region);
        return invalid_shared;
    }

    // TODO: create Batcher
    region -> batcher = (Batcher*)malloc(sizeof(Batcher));
    atomic_store(&(region -> batcher -> timestamp), 0);
    atomic_store(&(region -> batcher -> next), 0);
    atomic_store(&(region -> batcher -> cnt_thread), 0);
    atomic_store(&(region -> batcher -> cnt_epoch), 0);
    atomic_store(&(region -> batcher -> is_writing), false);
    atomic_store(&(region -> batcher -> res_writes), batch_size);

    #ifdef _DEBUG_FLZ_
    printf("END CREATE for MY\n");
    #endif
    return region; 
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    // TODO: tm_destroy(shared_t)

    #ifdef _DEBUG_FLZ_
    printf("STARTING my DESTROY\n\n");
    #endif 
    Region *region = (Region*)shared;

    while(region -> allocs != NULL) {
        Segment* tmp = region -> allocs;
        region -> allocs = region -> allocs -> next;
        // free(tmp -> data);
        free(tmp);
    }

    // ==============================
    shared_lock_cleanup(&(region->lock));
    // ==============================

    free(region -> batcher);
    free(region -> start);
    free(region);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t unused(shared)) {
    // TODO: tm_start(shared_t)
    // return NULL;
    Region *region = (Region*)shared;
    return region -> start -> data;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t unused(shared)) {
    Region *region = (Region*)shared;
    return region -> size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t unused(shared)) {
    Region *region = (Region*)shared;
    return region -> align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro){
        #ifdef _DEBUG_FLZ_
        if (is_ro) {
            printf("RORORORORORO!!!!!"); 
            // return ;
        }

        printf("======\ntm_begin\n");
        #endif

    Region *region = (Region*)shared;

    #ifdef _TO_USE_BATCHER_
    Batcher *batcher = region -> batcher;
    #endif


    if (is_ro) {
        
        #ifdef _TO_USE_BATCHER_

        tx_t process_idx = atomic_fetch_add(&(batcher->timestamp), 1);

            #ifdef _DEBUG_FLZ_
                printf("tm_begin: is_ro, process: %lu\n", process_idx);
            #endif

        while (process_idx != atomic_load(&(batcher->next)))
            sched_yield();

        atomic_fetch_add(&(batcher->cnt_thread), 1);
        atomic_fetch_add(&(batcher->next), 1);

        return read_only_tx;
        #endif

        // ==============================
        // ==== reference implementation
        if (unlikely(!shared_lock_acquire_shared(&(region ->lock)))){
            printf("tm_begin: is_ro: failed\n");
            return invalid_tx;
        }
        return read_only_tx;
    } else {

        #ifdef _TO_USE_BATCHER_

        while(true) {
            tx_t process_idx = atomic_fetch_add(&(batcher->timestamp), 1);

                #ifdef _DEBUG_FLZ_
                printf("tm_begin: is_rw, process: %lu\n", process_idx);
                #endif


            while (process_idx != atomic_load(&(batcher->next)))
                sched_yield();

            if (atomic_load(&(batcher->res_writes)) != 0) 
            {
                // printf("tm_begin: is_rw, process: %lu\n\t\t: res_writes: %lu\n", process_idx, atomic_load(&(batcher->res_writes)));
                atomic_fetch_add(&(batcher->res_writes), -1)
                ;
                // );
                break; 
            }

            // skip and wait for next epoch, process with new idx
            atomic_fetch_add(&(batcher->next), 1);

            ulong this_epoch = get_epoch(batcher);
            while (this_epoch == get_epoch(batcher))
                sched_yield();
            
            process_idx = atomic_fetch_add(&(batcher->timestamp), 1);
        } 
        
        ulong tx_idx = atomic_fetch_add(&(batcher->cnt_thread), 1) + 1;

        atomic_store(&(batcher->is_writing), true);
        atomic_fetch_add(&(batcher->next), 1);

        return tx_idx; 

        #endif

        // ==============================
        // ==== reference implementation

        if (unlikely(!shared_lock_acquire(&(region ->lock)))){
            printf("tm_begin: is_rw: failed\n");
            return invalid_tx;
        }
        return read_write_tx;
    }
    // ==============================

    return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
        #ifdef _DEBUG_FLZ_
        printf("tm_end\n======\n");
        #endif

    Region* region = (Region*)shared;

    #ifdef _TO_USE_BATCHER_

    Batcher *batcher = region -> batcher;
    ulong process_idx = atomic_fetch_add(&(batcher->timestamp), 1);

        #ifdef _DEBUG_FLZ_
        printf("===\ntm_end: process_idx: %lu\n", process_idx);
        printf("tm_end: next: %lu\n", atomic_load(&(batcher->next)));
        #endif

    while (process_idx != atomic_load(&(batcher->next)))
        sched_yield();

    if (atomic_fetch_add(&(batcher->cnt_thread), -1) == 1
        && atomic_load(&(batcher -> is_writing))
        ) {
                #ifdef _DEBUG_FLZ_
                printf("ITS THE END OF EPOCH: %lu\n", atomic_load(&(batcher->cnt_epoch)));
                #endif

            // if this epoch contains some writes
            Commit_seg(region, region -> start); 
            for (Segment* seg = region -> allocs; seg != NULL; seg = seg -> next) {
                Commit_seg(region, seg);
            }
            // and start a new epoch
            atomic_store(&(batcher->res_writes), batch_size);
            atomic_store(&(batcher->is_writing), false);
// 
            atomic_fetch_add(&(batcher->cnt_epoch), 1);
    } else {
        if (tx != read_only_tx) {
            atomic_fetch_add(&(batcher->next), 1);
    
            ulong this_epoch = get_epoch(batcher);
            while (this_epoch == get_epoch(batcher))
                sched_yield();

            return true;
        } 
    }
    atomic_fetch_add(&(batcher->next), 1);
    return true; 

    // if (atomic_fetch_add(&(batcher->cnt_thread), -1) == 1) {
    //         #ifdef _DEBUG_FLZ_
    //             printf("last of this epoch\n"); 
    //         #endif
    //     // if at the end of the epoch, do cleanup
    //     if (atomic_load(&(batcher->is_writing))) {
    //         // if this epoch contains some writes
    //         for (Segment* seg = region -> allocs; seg != NULL; seg = seg -> next) {
    //             if (atomic_load(&(seg -> to_delete))){
    //                 tm_free(region, seg -> creator, seg); 
    //                 continue; 
    //             }
    //             // commit all writes
    //             // from shadow to data
    //             memcpy(seg -> data, seg -> shadow, seg -> size * sizeof(Word));
    //             // and reset control
    //             memset(seg -> control, 0, seg -> size * sizeof(tx_t));
    //             // and it will not get reset in the following epoches
    //             atomic_store(&(seg -> creator), it_is_free); 
    //         }
    //         // and start a new epoch
    //         atomic_store(&(batcher->res_writes), batch_size);
    //         atomic_store(&(batcher->is_writing), false);

    //         atomic_fetch_add(&(batcher->cnt_epoch), 1);
    //     } else {
        //     process this readonly staff; 
        // }

    //     atomic_fetch_add(&(batcher->next), 1);

    //     return true;
    // } else {
    //     // not the end of epoch
    //     if (tx == read_only_tx) {
    //         // if read-only, just return
    //         // noneed to block
    //         atomic_fetch_add(&(batcher->next), 1);
    //         return true; 
    //     } else {
    //         // if is writing
    //         // wait until the end of epoch 
    //         // (after commit)
    //         // to return
    //         atomic_fetch_add(&(batcher->next), 1);
    //         ulong this_epoch = get_epoch(batcher);
    //         while (this_epoch == get_epoch(batcher))
    //             sched_yield();
    //         return true;
    //     } 
    // }

    #endif

    // ==============================
    // ==== reference implementation
    if (tx == read_only_tx) {
        shared_lock_release_shared(&(region->lock));
    } else {
        shared_lock_release(&(region->lock));
    }
    return true;

    return false;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
        #ifdef _DEBUG_FLZ_
        printf("start tm_read\n");
        #endif

    #ifdef _TO_USE_BATCHER_
    Region *region = (Region*)shared;

        // #ifdef _DEBUG_FLZ_
        // printf("tm_read: %p -> %p\n", source, target);
        // #endif

    if (tx == read_only_tx) {
        memcpy(target, source, size);
        return true;
    }

    Segment* seg = findSegment(region, source);
    if (seg == NULL) {
        // printf("tm_read: seg is NULL\n");
        Undo(region, tx); 
        return false;
    }

    size_t cnt_word = size / sizeof(Word);
    size_t offset = ((uintptr_t)source - (uintptr_t)seg -> data)/sizeof(Word);
    
        #ifdef _DEBUG_FLZ_TEST_READ_
        printf("tm_read: %p -> %p, offset: %lu, cnt_word: %lu\n", source, target, offset, cnt_word);
        printf("base address of data: %p\n", seg -> data);
        printf("base address of shadow: %p\n", seg -> shadow);
        #endif


    for (size_t i = 0; i < cnt_word; ++i) {
        atomic_tx * control = seg -> control + offset + i;
        tx_t expected = it_is_free;
        if (tx == atomic_load(control)) {
            memcpy(((Word*) target) + i , 
                    seg -> shadow + offset + i, 
                    sizeof(Word));
        } else {
            if (atomic_compare_exchange_strong(control, &expected, tx + batch_size)
                || expected == tx + batch_size
                ) {
                    memcpy(((Word*) target) + i , 
                            seg -> data + offset + i, 
                            sizeof(Word));
            } else {
                Undo(region, tx);
                return false;
            }
        }
    }
    
    return true; 

    #endif
    // ==============================
    // ==== reference implementation
    memcpy(target, source, size);
    // printf("end tm_read\n");
    return true;
    // ==============================
    return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
// static int countttt = 0;

bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // ++countttt; 
    // if (countttt > 10)
    //     return false;

    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
        // #ifdef _DEBUG_FLZ_
        // printf("tm_writing %lu byte\n", size);
        // #endif

    #ifdef _TO_USE_BATCHER_

    Region *region = (Region*)shared;
    Segment *seg = findSegment(region, target);
    if (seg == NULL || atomic_load(&(seg -> to_delete))) {
        // printf("tm_write: seg is NULL\n");
        Undo(region, tx); 
        return false;
    }
    if (!try_write(region, seg, tx, target, size)) {
        // printf("tm_write: lock_write failed\n");
        Undo(region, tx); 
        return false;
    }

        #ifdef _DEBUG_FLZ_TEST_WRITE_
        printf("tm_write: %p -> (data)%p, (shadow)%p \n", source, target, 
                                                          ((Word*) target) + (seg -> size) * sizeof(Word));
        #endif

    // ulong offset = ((uintptr_t)target - (uintptr_t)seg -> data)/sizeof(Word);
    // memcpy(seg -> shadow + offset,
    //         source, 
    //         size * sizeof(Word));
    memcpy(((Word*) target) + (seg -> size) * sizeof(Word), 
                            // to the shadow
            source,
            size * sizeof(Word));
    return true; 

    #endif
    // ==============================
    // ==== reference implementation
    // printf("start tm_write %x -> %x \n", source, target);
    memcpy(target, source, size);
    // printf("end tm_write\n");
    return true;
    // ==============================
    return false;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t tx, size_t size, void** target) {
    // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
    #ifdef _DEBUG_FLZ_
    printf("start tm_alloc\n");
    #endif


    Region *region = (Region*)shared;
    size_t align = region -> align;

    // allocate a new segment
    // Words are appended to the end of the segment
    Segment* seg; 
    if (unlikely(posix_memalign((void**)&seg, align, sizeof(Segment) 
                                                     + sizeof(atomic_tx) * size
                                                     + sizeof(Word) * size * 2) != 0)) 
    {
        return nomem_alloc;
    }
    memset(seg, 0, sizeof(Segment) 
                   + sizeof(atomic_tx) * size 
                   + sizeof(Word) * size * 2 );

    seg -> data = (Word*)((uintptr_t)seg + sizeof(Segment));
    seg -> shadow = (Word*)((uintptr_t)seg -> data + sizeof(Word) * size);
    seg -> control = (atomic_tx*)((uintptr_t)seg -> shadow + sizeof(Word) * size);
    // memset(seg -> data, 0, size * sizeof(Word));

    // add creator and size
    atomic_store(&(seg -> creator), tx);
    seg -> size = size;
    
    // add to linked list
    seg -> previous = NULL;
    seg -> next = region -> allocs;
    if (seg -> next) seg -> next -> previous = seg;
    region -> allocs = seg;

    *target = seg -> data;
    // if (seg -> data == NULL)
    //     printf("failed to allocate\n");
    #ifdef _DEBUG_FLZ_
    printf("end tm_alloc\n");
    #endif

    return success_alloc;

    // return abort_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t unused(tx), void* target) {
    // TODO: tm_free(shared_t, tx_t, void*)
    // printf("start tm_free: %x\n", target);
    Region *region = (Region*)shared;
    Segment* seg = (Segment*)((uintptr_t)target - sizeof(Segment));

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

    return true; 
    // ==============================

    return false;
}
