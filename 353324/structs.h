#ifndef _STRUCTS_H_
#define _STRUCTS_H_

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

// Internal headers
#include <tm.h>

#include "macros.h"
#include "shared-lock.h"

// Constants and types
static const tx_t read_only_tx  = UINTPTR_MAX - 1;
static const tx_t read_write_tx = UINTPTR_MAX - 2;
static const ulong batch_size = 8; 

typedef _Atomic(tx_t) atomic_tx;

struct Batcher_str{
    atomic_tx timestamp;
    atomic_tx next; 
    atomic_ulong cnt_epoch;
    atomic_ulong cnt_thread;
    atomic_ulong res_writes;

    // TBD
};
typedef struct Batcher_str Batcher; 
// ==============================
// Batcher Functions
atomic_ulong get_epoch(Batcher* batcher) { return atomic_load(&(batcher -> cnt_epoch)); }


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

    Batcher batcher;
    struct shared_lock_t lock;
    // TBD
};
typedef struct Region_str Region;



#endif