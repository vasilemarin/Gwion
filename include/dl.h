#ifndef DL_H
#define DL_H

#include <stdlib.h>
#include <complex.h>
#include "defs.h"
#include "vm.h"
#include "map_private.h"

typedef struct DL_Return {
  union {
    m_uint    v_uint;
    m_float   v_float;
    m_complex v_complex;
    m_vec3 	  v_vec3;
    m_vec4 	  v_vec4;
    M_Object  v_object;
  } d;
} DL_Return;

void dl_return_push(const DL_Return retval, VM_Shred shred, int kind);

typedef void (*f_xtor)(M_Object o, VM_Shred sh);
typedef void (*f_mfun)(M_Object o, DL_Return * RETURN, VM_Shred sh);
typedef void (*f_sfun)(DL_Return * RETURN, VM_Shred sh);

typedef struct {
  m_str name;
  m_str type;
} DL_Value;

typedef struct {
  m_str    name;
  m_str    type;
  m_uint   addr;
  m_uint   narg;
  DL_Value args[6];
} DL_Func;


void dl_func_init(DL_Func* fun, const m_str t, const m_str n, const m_uint addr);
void dl_func_add_arg(DL_Func* a, const m_str t, const m_str  n);
#endif
