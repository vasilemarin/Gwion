#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "instr.h"
#include "memoize.h"
#include "gwion.h"
#include "object.h"
#include "array.h"
#include "operator.h"
#include "import.h"

ANN void free_code_instr(const Vector v, const Gwion gwion) {
  for(m_uint i = vector_size(v) + 1; --i;) {
    const Instr instr = (Instr)vector_at(v, i - 1);
    const f_freearg f = (f_freearg)(map_get(&gwion->data->freearg, instr->opcode) ?:
       map_get(&gwion->data->freearg, (vtype)instr->execute));
    if(f)
      f(instr, gwion);
    mp_free(gwion->mp, Instr, instr);
  }
}

ANN void free_vmcode(VM_Code a, Gwion gwion) {
  if(a->memoize)
    memoize_end(gwion->mp, a->memoize);
  if(!a->builtin) {
    _mp_free(gwion->mp, vector_size(a->instr) * BYTECODE_SZ, a->bytecode);
    if(likely(!a->callback))
      free_code_instr(a->instr, gwion);
    free_vector(gwion->mp, a->instr);
  }
  if(a->closure)
    free_closure(a->closure, gwion);
  free_mstr(gwion->mp, a->name);
  mp_free(gwion->mp , VM_Code, a);
}

static inline uint isgoto(const unsigned opcode) {
  return opcode == eGoto ||
      opcode == eBranchEqInt || opcode == eBranchNeqInt ||
      opcode == eBranchEqFloat || opcode == eBranchNeqFloat;
}

static inline void setpc(const m_bit *data, const m_uint i) {
  *(unsigned*)(data+1) = i + 1;
}

ANN static m_bit* tobytecode(MemPool p, const VM_Code code) {
   const Vector v = code->instr;
  const m_uint sz = vector_size(v);
  m_bit *ptr = _mp_malloc(p, sz * BYTECODE_SZ);
  struct Vector_ nop;
  vector_init(&nop);
  for(m_uint i= 0; i < sz; ++i) {
    m_bit *const data = ptr + i*BYTECODE_SZ;
    const Instr instr = (Instr)vector_at(v, i);
    if(instr->opcode < eOP_MAX) {
      if(instr->opcode == eRegMove) {
        m_int move = (m_int)instr->m_val;
        m_uint j = 0;
        Instr next;
        while((next = (Instr)vector_at(v, i + j +1)) && next->opcode == eRegMove) {
          ++j; vector_add(&nop, i+j);
          move += (m_int)next->m_val;
          next->opcode = eNoOp;
        }
        if((instr->m_val = move)) {
          memcpy(data, instr, BYTECODE_SZ);
          setpc(data, i);
        } else {
          vector_add(&nop, i);
          instr->opcode = eNoOp;
        }
        i += j;
        continue;
      } else if(instr->opcode == eUnroll2) {
        const Instr unroll = (Instr)instr->m_val;
        const m_uint pc = vector_find(v, (m_uint)unroll);
        m_uint reduce_pre = 0, reduce = 0;
        for(m_uint j = 0; j < vector_size(&nop); ++j) {
          const m_uint at = vector_at(&nop, j);
          if(at < pc)
            ++reduce_pre;
          if(at >= pc) {
            if(at > (pc + unroll->m_val2))
              break;
            ++reduce;
          }
        }
        m_bit *const unroll_data = ptr + (pc-reduce_pre)*BYTECODE_SZ;
        unroll->m_val2 -= reduce;
        *(m_uint*)(unroll_data + SZ_INT *2) -= reduce;
        instr->opcode = eNoOp;
        vector_add(&nop, i);
        continue;
      }
      if(instr->opcode == eGoto && instr->m_val  == i+1) {
        instr->opcode = eNoOp;
        vector_add(&nop, i);
      } else if(instr->opcode != eNoOp)
        memcpy(data, instr, BYTECODE_SZ);
      else
        vector_add(&nop, i);
    } else {
      *(m_bit*)(data) = instr->opcode;
      *(Instr*)(data + SZ_INT) = instr;
      *(f_instr*)(data + SZ_INT*2) = instr->execute;
    }
    setpc(data, i);
  }
  if(!vector_size(&nop)) {
    vector_release(&nop);
    return ptr;
  }
  m_bit *const final = _mp_malloc(p, sz * BYTECODE_SZ); // could use smaller size
  for(m_uint i= 0, j = 0; i < sz; ++i) {
    const Instr instr = (Instr)vector_at(v, i);
    unsigned opcode = instr->opcode;
    if(opcode != eNoOp) {
      m_bit *const base = ptr   + i*BYTECODE_SZ,
            *const data = final + j*BYTECODE_SZ;
      memcpy(data, base, BYTECODE_SZ);
      if(isgoto(opcode)) {
        m_uint pc = 0;
        for(pc = 0; pc < vector_size(&nop); ++pc) {
          if(instr->m_val <= vector_at(&nop, pc))
            break;
        }
        *(m_uint*)(data + SZ_INT) = instr->m_val > pc ? instr->m_val - pc : 0;
      }
      setpc(data, j);
      ++j;
    }
  }
  vector_release(&nop);
  mp_free2(p, sz*BYTECODE_SZ, ptr);
  return final;
}

VM_Code new_vmcode(MemPool p, const Vector instr, const m_uint stack_depth,
      const int builtin, const m_str name) {
  VM_Code code           = mp_calloc(p, VM_Code);
  code->name             = mstrdup(p, name);
  if(instr) {
    code->instr            = vector_copy(p, instr);
    code->bytecode = tobytecode(p, code);
  }
  code->stack_depth      = stack_depth;
  code->builtin = builtin;
  code->ref = 1;
  return code;
}

// TODO: handle native code
// TODO: do not re-create if code exists
VM_Code vmcode_callback(MemPool mp, VM_Code base) {
  char name[strlen(base->name) + 11];
  sprintf(name, "%s(callback)", base->name);
  const Instr instr = (Instr)vector_back(base->instr);
  instr->opcode = eEOC;
  VM_Code code = new_vmcode(mp, base->instr, base->stack_depth, base->builtin, name);
  code->callback = 1;
  instr->opcode = eFuncReturn;
  return code;
}
