SHELL = /usr/bin/env bash -o pipefail
ifeq ($(VERBOSE),yes)
VFLAG= -v 
endif

CFLAGS  := $(CFLAGS) $(MCFLAGS)
LDFLAGS  := $(LDFLAGS) $(MCFLAGS)

CC      = $(TOOLCHAIN)-gcc 
CXX     = $(TOOLCHAIN)-gcc 
LD      = $(TOOLCHAIN)-gcc 
OBJDUMP = $(TOOLCHAIN)-objdump 
MKPROM  ?= /opt/mkprom2/mkprom2

# ------------------------------------------
# USED by testing/Makefile

BUILDCASES_TSIM_SPARC_EXTRA_NONE =
BUILDCASES_TSIM_ERC32_SPARC_ELF = \
 mvt; \
 mvt/soft;@msoft-float 

BUILDCASES_TSIM_ERC32_SPARC_RTEMS = \
 mvt; \
 mvt/soft;@msoft-float 

BUILDCASES_TSIM_ERC32_SPARC_RTEMS_EXTRA = -tsc691

BUILDCASES_TSIM_LEON2_SPARC_ELF = \
 mvt/v8;@qbsp=leon2@mcpu=leon \
 mvt/soft/v8;@qbsp=leon2@msoft-float@mcpu=leon

BUILDCASES_TSIM_LEON2_SPARC_RTEMS = \
 mvt/v8;@qbsp=at697f@mcpu=v8 \
 mvt/soft/v8;@qbsp=at697f@msoft-float@mcpu=v8

BUILDCASES_TSIM_LEON3_SPARC_ELF = \
 mvt; \
 mvt/soft;@msoft-float \
 mvt/v8;@mcpu=v8 \
 mvt/flat;@mflat \
 mvt/v8/flat;@mcpu=v8@mflat \
 mvt/soft/v8;@msoft-float@mcpu=v8 \
 mvt/soft/flat;@msoft-float@mflat \
 mvt/soft/v8/flat;@msoft-float@mcpu=v8@mflat \
 svt;@qsvt \
 svt/soft;@qsvt@msoft-float \
 svt/v8;@qsvt@mcpu=v8 \
 svt/flat;@qsvt@mflat \
 svt/v8/flat;@qsvt@mcpu=v8@mflat \
 svt/soft/v8;@qsvt@msoft-float@mcpu=v8 \
 svt/soft/flat;@qsvt@msoft-float@mflat \
 svt/soft/v8/flat;@qsvt@msoft-float@mcpu=v8@mflat \
 mvt/ut699;@mfix-ut699 \
 mvt/mfix-b2bst;@mfix-b2bst \
 mvt/v8/ut699;@mcpu=v8@mfix-ut699 \
 mvt/v8/mfix-b2bst;@mcpu=v8@mfix-b2bst \
 mvt/soft/mfix-b2bst;@msoft-float@mfix-b2bst \
 mvt/soft/v8/mfix-b2bst;@msoft-float@mcpu=v8@mfix-b2bst \
 svt/ut699;@qsvt@mfix-ut699 \
 svt/mfix-b2bst;@qsvt@mfix-b2bst \
 svt/v8/ut699;@qsvt@mcpu=v8@mfix-ut699 \
 svt/v8/mfix-b2bst;@qsvt@mcpu=v8@mfix-b2bst \
 svt/soft/mfix-b2bst;@qsvt@msoft-float@mfix-b2bst \
 svt/soft/v8/mfix-b2bst;@qsvt@msoft-float@mcpu=v8@mfix-b2bst

BUILDCASES_TSIM_LEON3_SPARC_RTEMS = \
 mvt/v8;@qbsp=gr712rc@mcpu=v8 \
 mvt/soft/v8;@qbsp=gr712rc@msoft-float@mcpu=v8 

TESTCASES_SPARC_ELF_TSIM_ERC32  =
TESTCASES_SPARC_ELF_TSIM_LEON2  = test-elf/stanford
TESTCASES_SPARC_ELF_TSIM_LEON3  = test-elf/stanford

TESTCASES_SPARC_RTEMS_TSIM_ERC32  = test-rtems/hello-erc32
TESTCASES_SPARC_RTEMS_TSIM_LEON2  = test-rtems/hello
TESTCASES_SPARC_RTEMS_TSIM_LEON3  = test-rtems/hello

MKPROMCASE_SPARC_ELF    = ram
MKPROMCASE_SPARC_RTEMS  = ram

MKPROMRAMCASE_SPARC_ELF = \
 prom; \
 prom-nocomp;@nocomp \

MKPROMRAMCASE_SPARC_RTEMS = \
 prom; \
 prom-nocomp;@nocomp \

MKPROMRAMCASE_TSIM_LEON2_SPARC_ELF = \
 prom;@leon2 \
 prom-nocomp;@leon2@nocomp \

MKPROMRAMCASE_TSIM_LEON3_SPARC_ELF = \
 prom; \
 prom-nocomp;@nocomp \

MKPROMRAMCASE_TSIM_ERC32_SPARC_ELF = \
 prom;@erc32 \
 prom-nocomp;@erc32@nocomp \


MKPROMRAMCASE_TSIM_LEON2_SPARC_RTEMS = \
 prom;@leon2 \
 prom-nocomp;@leon2@nocomp \

MKPROMRAMCASE_TSIM_LEON3_SPARC_RTEMS = \
 prom; \
 prom-nocomp;@nocomp \

MKPROMRAMCASE_TSIM_ERC32_SPARC_RTEMS = \
 prom;@erc32 \
 prom-nocomp;@erc32@nocomp \


MKPROMROMCASE_TSIM_LEON2_SPARC_RTEMS = -romres -leon2
MKPROMROMCASE_TSIM_LEON3_SPARC_RTEMS = -romres
MKPROMROMCASE_TSIM_ERC32_SPARC_RTEMS = -romres -erc32
MKPROMROMCASE_TSIM_LEON2_SPARC_ELF   = -romres -leon2
MKPROMROMCASE_TSIM_LEON3_SPARC_ELF   = -romres
MKPROMROMCASE_TSIM_ERC32_SPARC_ELF   = -romres -erc32


# ------------------------------------------
# per testcase included rule. Dependent on "target" macro of Makefile

ifneq ($(PDIR),)

RUNDIR=runs/$(TARGETSIM)/$(PDIR)/$(MDIR)

include $(PDIR)/Makefile

$(PDIR)/$(target): $(addprefix $(PDIR)/,$($(target)_objs))
	-mkdir -p $(RUNDIR)
	echo "options   : $(OPTSTR)" >$(RUNDIR)/$(target).runopt
	echo "pattern   : $(PDIR)/$($(target)_testpattern) " >>$(RUNDIR)/$(target).runopt
	$(LD) $^ $(LDFLAGS) $(LDFLAGS_$(@F)) $(EXTRAFLAGS) -o $@
	install $(PDIR)/$(target) $(RUNDIR)/$(target)
	$(OBJDUMP) -d $(RUNDIR)/$(target) >$(RUNDIR)/$(target).dis
	echo "executable: $(target)" >>$(RUNDIR)/$(target).runopt; \
	if [ "$(filter ram,$(MKPROMTYPES))" == "ram" ]; then \
		echo -n "mkprom-ram-executables:" >>$(RUNDIR)/$(target).runopt; \
		for i in $(subst ;,\;,$(MKPROMRAMTYPES)); do \
			post=`echo $$i | sed -e 's/;.*$$//'`; \
			flags=`echo $$i | sed -e 's/^[^;]*;//' -e 's/@/ -/g'`; \
			echo prom variation $$i: post=$$post flags:$$flags; \
			echo $(MKPROM) $(CFLAGS) $@ -v -freq $(FREQ) $${flags} -o $@.$${post}; \
			$(MKPROM) $(CFLAGS) $@ -v -freq $(FREQ) $${flags} -o $@.ram.$${post}; \
			install $(PDIR)/$(target).ram.$${post} $(RUNDIR)/$(target).ram.$${post}; \
			$(OBJDUMP) -d $(RUNDIR)/$(target).ram.$${post} >$(RUNDIR)/$(target).ram.$${post}.dis; \
			echo -n "$(target).ram.$${post} " >>$(RUNDIR)/$(target).runopt; \
		done; \
		echo "" >>$(RUNDIR)/$(target).runopt; \
	fi; \
	if [ "$(filter rom,$(MKPROMTYPES))" == "rom" ]; then \
		echo "$(MKPROM) $(CFLAGS) $(MKPROMROMTYPES) -freq $(FREQ) $^ -v -o $@.rom.prom"; \
		$(MKPROM) $(CFLAGS) $(MKPROMROMTYPES) -freq $(FREQ) $^ -v -o $@.rom.prom; \
		install $(PDIR)/$(target).rom.prom $(RUNDIR)/$(target).rom.prom; \
		echo "mkprom-rom-executable : $(target).rom.prom" >>$(RUNDIR)/$(target).runopt; \
	fi;

$(PDIR)/$(target)_clean:
	-rm $(addprefix $(PDIR)/,$($(target)_objs)) $(PDIR)/$(target)

$(PDIR)/$(target)_install:

test-compile: $(PDIR)/$(target)_clean $(PDIR)/$(target)  $(PDIR)/$(target)_install

endif

# ------------------------------------------

ifneq ($(TARGETSIMDIR),)
runtests:
	for i in `find runs/$(TARGETSIMDIR) -type f `; do \
		if [ -x $$i ]; then \
			dir=`dirname $$i`; \
			o=`cat $$dir/*.runopt | awk '/option/ { print $$3 } ' | sed -e 's/^[^;]*;//' `; \
			p=`cat $$dir/*.runopt | awk '/pattern/ { print $$3 } ' `; \
			o=`perl gcctosim.pl $(TARGETSIM) $$o`; \
			echo -n Test $$i...  ; \
			./runtest.sh $(VFLAG) -x $(TARGETSIM) -o "$$o" $$i ; \
			if [ -f $$p ]; then \
				if [ `perl $$p $$i.info` = "ok" ]; then \
					echo -n ok; \
				else \
					echo -n failed, read testing/$$i.info; \
				fi; \
			else \
				echo -n not verified, check $$i.info; \
			fi; \
			echo ; \
		fi; \
	done
endif

TSIM-ERC32-BIN=tsim-erc32
TSIM-LEON2-BIN=tsim-leon2
TSIM-LEON3-BIN=tsim-leon3

test-run-tsim-erc32:                  # run all tests compiled for tsim-erc32
	make TARGETSIMDIR=tsim-erc32 TARGETSIM=$(TSIM-ERC32-BIN) runtests

test-run-tsim-leon2:                   # run all tests compiled for tsim-leon2
	make TARGETSIMDIR=tsim-leon2 TARGETSIM=$(TSIM-LEON2-BIN)  runtests

test-run-tsim-leon3:                  # run all tests compiled for tsim-leon3
	make TARGETSIMDIR=tsim-leon3 TARGETSIM=$(TSIM-LEON3-BIN) runtests

# ------------------------------------------
# Compiling

testset-compile:
	@echo to get verbose output prepend VERBOSE=yes to the make call
	@for i in $(subst ;,\;,$(BUILDCASES)); do \
		dir=`echo $$i | sed -e 's/;.*$$//'`; \
		flags=`echo $$i | sed -e 's/^[^;]*;//' -e 's/@/ -/g'`; \
		echo variation $$i: dir=$$dir flags:$$flags; \
		for f in $(TESTCASES); do \
			echo " Build $$f"; \
				$(MAKE) \
				OPTSTR="$${i}" \
				MCFLAGS="$${flags}" \
				EXTRAFLAGS=$(EXTRAFLAGS) \
				MDIR="$${dir}" \
				PDIR="$${f}" \
				TOOLCHAIN=$(TOOLCHAIN) \
				TARGETSIM=$(TARGETSIM) \
				MKPROMTYPES="$(MKPROMTYPES)" \
				MKPROMRAMTYPES="$(MKPROMRAMTYPES)" \
				MKPROMROMTYPES="$(MKPROMROMTYPES)" \
				test-compile; \
		done; \
	done


test-compile-sparc-elf-tsim-erc32:
	make BUILDCASES="$(BUILDCASES_TSIM_ERC32_SPARC_ELF)"  TESTCASES=$(TESTCASES_SPARC_ELF_TSIM_ERC32)  MKPROMTYPES="$(MKPROMCASE_SPARC_ELF)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_ERC32_SPARC_ELF)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_ERC32_SPARC_ELF)" TARGETSIM=tsim-erc32 TOOLCHAIN=sparc-gaisler-elf FREQ=14 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_SPARC_EXTRA_NONE)" \
		2>&1  | tee test-compile-sparc-elf-tsim-erc32.out

test-compile-sparc-elf-tsim-leon2:
	make BUILDCASES="$(BUILDCASES_TSIM_LEON2_SPARC_ELF)"   TESTCASES=$(TESTCASES_SPARC_ELF_TSIM_LEON2)  MKPROMTYPES="$(MKPROMCASE_SPARC_ELF)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_LEON2_SPARC_ELF)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_LEON2_SPARC_ELF)" TARGETSIM=tsim-leon2 TOOLCHAIN=sparc-gaisler-elf FREQ=50 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_SPARC_EXTRA_NONE)" \
		2>&1  | tee test-compile-sparc-elf-tsim-leon.out

test-compile-sparc-elf-tsim-leon3:
	make BUILDCASES="$(BUILDCASES_TSIM_LEON3_SPARC_ELF)"  TESTCASES=$(TESTCASES_SPARC_ELF_TSIM_LEON3)  MKPROMTYPES="$(MKPROMCASE_SPARC_ELF)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_LEON3_SPARC_ELF)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_LEON3_SPARC_ELF)" TARGETSIM=tsim-leon3 TOOLCHAIN=sparc-gaisler-elf FREQ=50 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_SPARC_EXTRA_NONE)" \
		2>&1  | tee test-compile-sparc-elf-tsim-leon3.out


test-compile-sparc-rtems-tsim-erc32:
	make BUILDCASES="$(BUILDCASES_TSIM_ERC32_SPARC_RTEMS)" TESTCASES=$(TESTCASES_SPARC_RTEMS_TSIM_ERC32)  MKPROMTYPES="$(MKPROMCASE_SPARC_RTEMS)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_ERC32_SPARC_RTEMS)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_ERC32_SPARC_RTEMS)"  TARGETSIM=tsim-erc32 TOOLCHAIN=sparc-rtems FREQ=14 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_ERC32_SPARC_RTEMS_EXTRA)" \
		2>&1  | tee test-compile-sparc-rtems-tsim-erc32.out

test-compile-sparc-rtems-tsim-leon2:
	make BUILDCASES="$(BUILDCASES_TSIM_LEON2_SPARC_RTEMS)"  TESTCASES=$(TESTCASES_SPARC_RTEMS_TSIM_LEON2)  MKPROMTYPES="$(MKPROMCASE_SPARC_RTEMS)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_LEON2_SPARC_RTEMS)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_LEON2_SPARC_RTEMS)"  TARGETSIM=tsim-leon2 TOOLCHAIN=sparc-gaisler-rtems5 FREQ=50 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_SPARC_EXTRA_NONE)" \
		2>&1  | tee test-compile-sparc-rtems-tsim-leon.out

test-compile-sparc-rtems-tsim-leon3:
	make BUILDCASES="$(BUILDCASES_TSIM_LEON3_SPARC_RTEMS)" TESTCASES=$(TESTCASES_SPARC_RTEMS_TSIM_LEON3)  MKPROMTYPES="$(MKPROMCASE_SPARC_RTEMS)"  MKPROMRAMTYPES="$(MKPROMRAMCASE_TSIM_LEON3_SPARC_RTEMS)" MKPROMROMTYPES="$(MKPROMROMCASE_TSIM_LEON3_SPARC_RTEMS)" TARGETSIM=tsim-leon3 TOOLCHAIN=sparc-gaisler-rtems5 FREQ=50 testset-compile \
		EXTRAFLAGS="$(BUILDCASES_TSIM_SPARC_EXTRA_NONE)" \
		2>&1  | tee test-compile-sparc-rtems-tsim-leon3.out




testcompile-rtems: \
	test-compile-sparc-rtems-tsim-erc32 \
	test-compile-sparc-rtems-tsim-leon2 \
	test-compile-sparc-rtems-tsim-leon3

testcompile-elf: \
	test-compile-sparc-elf-tsim-leon2 \
	test-compile-sparc-elf-tsim-leon3

testcompile: clean testcompile-rtems testcompile-elf

test-leon3: testcompile \
	test-compile-sparc-elf-tsim-leon3 \
	test-compile-sparc-rtems-tsim-leon3 \
	test-run-tsim-leon3 

testall: testcompile \
	\
    test-run-tsim-erc32 \
	test-run-tsim-leon2 \
	test-run-tsim-leon3 \

clean:
	-rm -rf runs log
	-rm -f *.out
	-rm -f ./test-elf/*/*exe* ./test-rtems/*/*exe*
	-rm -f ./test-elf/*/*.o ./test-rtems/*/*.o

