#ifndef __SARABANDE_VARIABLE_H__
#define __SARABANDE_VARIABLE_H__

typedef struct sbVar {
  hVal value;
} sbVar;

typedef sbVar *hVar;

hVal sbVar_get_value(hVar v);

hVal *sbVar_get_value_ptr(hVar v);

void sbVar_set_value(hVar v, const hVal *value);

hVal sbVar_get_lvalue_ref(hVar v);

hVal sbVar_get_rvalue_ref(hVar v);

void sbVar_set_attached_ref(hVal *attach_to, hVal *value);

hVal *sbVar_get_attached_ref(const hVal *attached_to);

void sbVar_move_to_heap(hVar v);

hVar sbVar_deref(const hVal *v);

sbVar sbVar_retain(hVar v);

void sbVar_release(hVar v);

#endif
