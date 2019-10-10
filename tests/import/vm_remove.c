#include "gwion_util.h"
#include "gwion_ast.h"
#include "oo.h"
#include "vm.h"
#include "env.h"
#include "type.h"
#include "instr.h"
#include "object.h"
#include "gwion.h"
#include "value.h"
#include "operator.h"
#include "import.h"
#include "instr.h"

SFUN(coverage_int) {
  vm_remove(shred->info->vm, *(m_int*)MEM(0));
}

GWION_IMPORT(gwion_remove_test) {
  GWI_BB(gwi_func_ini(gwi, "int", "test", coverage_int))
  GWI_BB(gwi_func_arg(gwi, "int", "i"))
  GWI_BB(gwi_func_end(gwi, 0))
  return GW_OK;
}