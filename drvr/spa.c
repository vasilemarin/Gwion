#include <stdlib.h>
#include "vm.h"
#include "driver.h"

static sp_audio spa;

static m_bool spa_ini(VM* vm, DriverInfo* di) {
  return 1;
}

static void spa_run(VM* vm, DriverInfo* di) {
  char tmp[104];
  sp_data* sp = vm->sp;
  sprintf(tmp, "%s.spa", di->card);
  if(spa_open(vm->sp, &spa, tmp, SPA_WRITE) == SP_NOT_OK) {
    fprintf(stderr, "Error: could not open file %s.\n", di->card);
    return;
  }
  while(vm->is_running) {
    vm_run(vm);
    spa_write_buf(vm->sp, &spa, vm->sp->out, vm->sp->nchan);
    vm->sp->pos++;
    GWION_CTL
  }
}

static void spa_del(VM* vm) {
  spa_close(&spa);
}

Driver* spa_driver(VM* vm) {
  Driver* d = malloc(sizeof(Driver));
  d->ini = spa_ini;
  d->run = spa_run;
  d->del = spa_del;
  vm->wakeup = no_wakeup;
  return d;
}
