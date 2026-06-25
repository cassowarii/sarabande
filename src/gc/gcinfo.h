#ifndef __SARABANDE_GCINFO_H__
#define __SARABANDE_GCINFO_H__

#include "global.h"

enum GCFLAG {
  GC_FLAG_COLORA = 1ULL,
  GC_FLAG_COLORB = 2ULL,
  GC_FLAG_COLORC = GC_FLAG_COLORA | GC_FLAG_COLORB,
  GC_FLAG_PINNED = 4ULL,
};

typedef struct GCINFO {
  u32 gcflags;
  u32 refcount;
} GCINFO;

#endif
