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

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

// Internal headers
#include <tm.h>

#include "macros.h"
#include "shared-lock.h"

// Constants and types
static const tx_t read_only_tx  = UINTPTR_MAX - 10;
static const tx_t read_write_tx = UINTPTR_MAX - 11;

typedef _Atomic(tx_t) atomic_tx;

struct Batcher_str{
    atomic_ulong last;
    atomic_ulong next; 
    atomic_ulong cnt_thread;
    atomic_ulong cnt_epoch;

    // TBD
};
typedef struct Batcher_str Batcher; 
// atomic_ulong get_epoch(Batcher* batcher) { return atomic_load(&(batcher -> cnt_epoch)); }

struct Word_str {
    void* data1;
    void* data2; 
    //SOMETHING control; 

    // atomic_tx owner; 
}; 
typedef struct Word_str Word;
// typedef uint8_t Word;

struct Segment_str {
    // Batcher batcher;
    Word* data; 
    size_t size; 
    atomic_tx owner; 
    struct Segment_str* next;
    struct Segment_str* previous; 
    // TBD
}; 
typedef struct Segment_str Segment; 

struct Region_str {
    void* start;
    Segment* allocs;
    size_t size;
    size_t align;

    // Batcher batcher;
    struct shared_lock_t lock;
    // TBD
};
typedef struct Region_str Region;


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t unused(size), size_t unused(align)) {
    // printf("STARTING my CREATE\n\n");
    align = align < sizeof(void*) ? sizeof(void*) : align;

    if (unlikely(size % align != 0))
        return invalid_shared;

    Region* region = (Region*)malloc(sizeof(Region));
    if (unlikely(!region)) {
        return invalid_shared;
    }


    if (unlikely(posix_memalign(&(region -> start), align, size) != 0)) {
        free(region);
        return invalid_shared;
    }
    memset(region -> start, 0, size);
    region -> align = align;
    region -> size = size;
    region -> allocs = NULL; 

    // TODO: create Batcher
    if (!shared_lock_init(&(region->lock))) {
        free(region->start);
        free(region);
        return invalid_shared;
    }

    printf("END CREATE for MY\n");
    return region; 
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    // TODO: tm_destroy(shared_t)
    printf("STARTING my DESTROY\n\n");
    Region *region = (Region*)shared;

    while(region -> allocs != NULL) {
        Segment* tmp = region -> allocs;
        region -> allocs = region -> allocs -> next;
        // free(tmp -> data);
        free(tmp);
    }

    shared_lock_cleanup(&(region->lock));
    // ==============================

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
    return region -> start;
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
tx_t tm_begin(shared_t unused(shared), bool unused(is_ro)) {
    // printf("======\ntm_begin\n");
    // TODO: tm_begin(shared_t)
    Region *region = (Region*)shared;
    if (is_ro) {
        // printf("tm_begin: is_ro\n");
        // Note: "unlikely" is a macro that helps branch prediction.
        // It tells the compiler (GCC) that the condition is unlikely to be true
        // and to optimize the code with this additional knowledge.
        // It of course penalizes executions in which the condition turns up to
        // be true.
        if (unlikely(!shared_lock_acquire_shared(&(region ->lock)))){
            printf("tm_begin: is_ro: failed\n");
            return invalid_tx;
        }
        return read_only_tx;
    } else {
        // printf("tm_begin: is_rw\n");
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
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
    // TODO: tm_end(shared_t, tx_t)
    Region* region = (Region*)shared;
    if (tx == read_only_tx) {
        shared_lock_release_shared(&(region->lock));
    } else {
        shared_lock_release(&(region->lock));
    }
    return true;
    // ==============================


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
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) {
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
    // printf("start tm_read\n");
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
bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) {
    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
    if (source == NULL) printf("source is NULL\n");
    if (target == NULL) printf("target is NULL\n");
    if (source == NULL || target == NULL) return false;

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
    // printf("start tm_alloc\n");
    Region *region = (Region*)shared;
    size_t align = region -> align;

    // allocate a new segment
    // Words are appended to the end of the segment
    Segment* seg; 
    if (unlikely(posix_memalign((void**)&seg, align, sizeof(Segment) + sizeof(Word) * size) != 0)) {
        return nomem_alloc;
    }

    memset(seg, 0, sizeof(Segment) + sizeof(Word) * size);
    seg -> data = (Word*)((uintptr_t)seg + sizeof(Segment));
    memset(seg -> data, 0, size * sizeof(Word));

    // add owner and size
    atomic_store(&(seg -> owner), tx);
    seg -> size = size;
    
    // add to linked list
    seg -> previous = NULL;
    seg -> next = region -> allocs;
    if (seg -> next) seg -> next -> previous = seg;
    region -> allocs = seg;

    *target = seg -> data;
    if (seg -> data == NULL)
        printf("failed to allocate\n");
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
