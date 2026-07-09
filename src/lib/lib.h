#ifndef __SARABANDE_LIB_H__
#define __SARABANDE_LIB_H__

typedef void (*sbLibMethod)(hVm, hV*, usize);

void sbLib_resolve_method(hVm vm);

void sbLib_sys_init();

void sbLib_sys_deinit();

#endif
