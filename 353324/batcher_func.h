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
        if (seg -> data <= source && source < seg -> data + seg -> size) {
            return seg;
        }
        seg = seg -> next;
    }
    return NULL;
}

#endif

#endif