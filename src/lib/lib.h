#ifndef __SARABANDE_LIB_H__
#define __SARABANDE_LIB_H__

#define METHOD(f, min, max) (sbLibMethod) { .behavior = f, .is_property = FALSE, .min_args = min, .max_args = max }
#define PROPERTY(f) (sbLibMethod) { .behavior = f, .is_property = TRUE }

typedef struct sbLibMethod {
  void (*behavior)(hVm, hVal*, usize);
  flag is_property : 1;
  i16 min_args : 15;
  i16 max_args;
} sbLibMethod;

void sbLib_resolve_method(hVm vm);

void sbLib_resolve_property(hVm vm);

void sbLib_resolve_global(hVm vm);

void sbLib_sys_init();

void sbLib_sys_deinit();

#endif
