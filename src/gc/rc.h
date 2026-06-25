#ifndef __SARABANDE_GCRC_H
#define __SARABANDE_GCRC_H

#include "gc/gcinfo.h"

/* "Heapy" objects use reference counting to implement copy-on-write
 * semantics. They do not use this info to free objects; this (will be)
 * handled by a tracing garbage collector. So, it is okay for a
 * reference count to drop to 0 temporarily. */

void RC_retain(void *obj) {
  ((GCINFO*)obj)->refcount ++;
}

void RC_release(void *obj) {
#ifdef DEBUG
  if (((GCINFO*)obj)->refcount == 0) PANIC("reference count (of obj @ %p) cannot drop below 0!", obj);
#endif
  ((GCINFO*)obj)->refcount --;
}

#endif
