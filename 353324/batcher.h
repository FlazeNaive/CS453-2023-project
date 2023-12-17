#ifndef _BATCHER_H_
#define _BATCHER_H_

#include <string.h>
#include <stdatomic.h>
#include <stdio.h>


#include "macros.h"
#include <tm.h>

struct Batcher_str{
    atomic_ulong last;
    atomic_ulong next; 
    atomic_ulong cnt_thread;
    atomic_ulong cnt_epoch;

    // TBD
};
typedef struct Batcher_str Batcher; 
// atomic_ulong get_epoch(Batcher* batcher) { return atomic_load(&(batcher -> cnt_epoch)); }

#endif