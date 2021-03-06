GWION_PACKAGE=gwion

ifeq (,$(wildcard util/config.mk))
$(shell cp util/config.mk.orig util/config.mk)
endif
ifeq (,$(wildcard config.mk))
$(shell cp config.mk.orig config.mk)
endif
include util/config.mk
include config.mk

SEVERITY ?= 10
GWION_TEST_DIR ?= /tmp

src := $(wildcard src/*.c)
src += $(wildcard src/*/*.c)

test_dir_all := $(wildcard tests/*)
test_ignore = tests/plug test/driver tests/module
test_dir := $(filter-out $(test_ignore), $(test_dir_all))
test_dir += examples


CFLAGS += -Iutil/include -Iast/include -Ilibcmdapp/src -D_GNU_SOURCE

# add commit hash to version for now
CFLAGS += -DGWION_VERSION=\"$(shell git log -n1 --format="%h")\"

ifeq (${DEBUG_STACK}, 1)
CFLAGS += -DDEBUG_STACK
endif

ifneq (${BUILD_ON_WINDOWS}, 1)
LDFLAGS += -ldl -lpthread
endif

src_obj := $(src:.c=.o)
gcda := $(src:.c=.gcda)
gcno := $(src:.c=.gcno)
lib_obj := $(filter-out src/main.o, ${src_obj})

CFLAGS  += -Iinclude

ifeq (${BUILD_ON_WINDOWS}, 1)
ifeq (${CC}, gcc)
#LDFLAGS += -Wl,--export-all-symbols -Wl,--enable-auto-import -Wl,--out-implib=lib${PRG}.dll.a
LDFLAGS += -Wl,--enable-auto-import
LDFLAGS += -lm
endif
else
LDFLAGS += -rdynamic
LDFLAGS += -lm
endif

ifeq ($(shell uname), Linux)
LDFLAGS += -lrt
endif

CFLAGS += -DGWION_BUILTIN

_GWLIBS = util/libgwion_util.a ast/libgwion_ast.a libcmdapp/libcmdapp.a lib${PRG}.a
GWLIBS = lib${PRG}.a libcmdapp/libcmdapp.a ast/libgwion_ast.a util/libgwion_util.a
_LDFLAGS = ${GWLIBS} ${LDFLAGS}

all: options-show ${_GWLIBS} src/main.o
	$(info link ${PRG})
	@${CC} src/main.o -o ${PRG} ${_LDFLAGS} ${LIBS}

options-show:
	@$(call _options)

lib${PRG}.a: ${lib_obj}
	@${AR} ${AR_OPT}

util/libgwion_util.a:
	@+GWION_PACKAGE= ${MAKE} -s -C util

util: util/libgwion_util.a
	@(info build util)

ast/libgwion_ast.a:
	@+GWION_PACKAGE= ${MAKE} -s -C ast

libcmdapp/libcmdapp.a:
	@+${MAKE} -s -C libcmdapp static

ast: ast/libgwion_ast.a
	@(info build ast)

afl: gwion-fuzz

gwion-fuzz:
	@touch src/parse/{scan*.c,check.c} src/emit/emit.c src/main.c
	@+PRG=gwion-fuzz CC=afl-clang-fast CFLAGS=-D__FUZZING__ ${MAKE}
	@touch src/parse/{scan*.c,check.c} src/emit/emit.c src/main.c

clean_core:
	@rm -f core.* *vgcore.*

.PHONY: .afl

clean: clean_core
	$(info cleaning ...)
	@rm -f src/*.o src/*/*.o gwion lib${PRG}.a ${gcno} ${gcda}

install: ${PRG} translation-install
	$(info installing ${GWION_PACKAGE} in ${PREFIX})
	@mkdir -p ${DESTDIR}/${PREFIX}/{bin,lib,include,share}
	@install ${PRG} ${DESTDIR}/${PREFIX}/bin
	@install lib${PRG}.a ${DESTDIR}/${PREFIX}/lib
	@PREFIX=${PREFIX} sed 's#PREFIX#${PREFIX}#g' scripts/gwion-config > gwion-config
	@install gwion-config ${DESTDIR}/${PREFIX}/bin/gwion-config
	@install scripts/gwion-pkg ${DESTDIR}/${PREFIX}/bin/gwion-pkg
	@rm gwion-config
	@mkdir -p ${DESTDIR}/${PREFIX}/include/gwion
	@cp -r include/* ${DESTDIR}/${PREFIX}/include/gwion
	@${MAKE} -s -C util install
	@${MAKE} -s -C ast install

uninstall: translation-uninstall
	$(info uninstalling ${GWION_PACKAGE} from ${PREFIX})
	@rm ${DESTDIR}/${PREFIX}/bin/${PRG}
	@rm ${DESTDIR}/${PREFIX}/bin/gwion-config
	@rm ${DESTDIR}/${PREFIX}/bin/gwion-pkg
	@rm ${DESTDIR}/${PREFIX}/lib/lib${PRG}.a
	@rm ${DESTDIR}/${PREFIX}/include/gwion/*.h
	@rmdir --ignore-fail-on-non-empty ${DESTDIR}/${PREFIX}/include/gwion

test:
	@bash scripts/test.sh ${test_dir}

include $(wildcard .d/*.d)
include util/locale.mk
