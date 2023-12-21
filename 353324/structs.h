#ifndef _STRUCTS_H_
#define _STRUCTS_H_

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

// Internal headers
#include "Mytm.h"

#include "macros.h"
#include "shared-lock.h"

// Constants and types
static const tx_t read_only_tx  = UINTPTR_MAX - 1;
static const tx_t read_write_tx = UINTPTR_MAX - 2;
static const tx_t to_delete = UINTPTR_MAX - 3;
static const tx_t it_is_free    = 0; //UINTPTR_MAX - 4;
static const ulong batch_size = 2; 
// static const ulong batch_size = 1; 

// typedef char tx_t; // The type of a transaction identifier
typedef _Atomic(tx_t) atomic_tx;

struct Batcher_str{
    /// @brief ts to assign to the next requring thread
    atomic_tx timestamp;
    /// @brief next thread to process
    atomic_tx next; 
    /// @brief which epoch this batcher is in
    atomic_ulong cnt_epoch;
    /// @brief Number of threads in this epoch
    atomic_ulong cnt_thread;
    /// @brief Number of slots remaining for writing threads in this epoch
    atomic_ulong res_writes;
    /// @brief indicate there is a writing thread in this epoch
    atomic_bool is_writing;

    // TBD
};
typedef struct Batcher_str Batcher; 
// ==============================
// Batcher Functions
static inline ulong get_epoch(const Batcher* batcher) { return atomic_load(&(batcher -> cnt_epoch)); }


struct Word_str {
    void* data1;
    void* data2; 
    //SOMETHING control; 

}; 
// typedef struct Word_str Word;
typedef _Atomic(uint8_t) Word;

struct Segment_str {
    // Batcher batcher;
    Word* data; 
    Word* shadow; 
    char* control;
    size_t size; 
    /// @brief actually it's the creator of this segment
    atomic_tx creator; 
    atomic_bool to_delete; 
    struct Segment_str* next;
    struct Segment_str* previous; 
    // TBD
}; 
typedef struct Segment_str Segment; 

struct Region_str {
    Segment* start; 
    // void* start;
    Segment* allocs;
    size_t size;
    size_t align;

    Batcher *batcher;
    struct shared_lock_t lock;
    // TBD
};
typedef struct Region_str Region;



#endif