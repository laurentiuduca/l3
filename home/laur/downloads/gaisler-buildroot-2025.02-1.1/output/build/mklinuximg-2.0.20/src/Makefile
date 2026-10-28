
### Override from make command line
# BUILD DIRECTORY
O:=.
# Linux Kernel Virtual Image
LINUX_IMAGE:=linux-2.6/arch/sparc/boot/image

SRCDIR:=$(shell pwd)
TOPDIR:=$(SRCDIR)/..
# The include order is so that the user can override the defaults
MCFLAGS:= $(CFLAGS) -I$(O)/include -I$(TOPDIR)/include -O2 \
	-static -ffreestanding \
	-fno-zero-initialized-in-bss -fno-stack-protector \
	-g -Wall

# Can be overridden
MCPU=leon3
ifneq ($(strip $(MCPU)),-)
	MCFLAGS += -mcpu=$(MCPU)
endif

MCFLAGS += $(CFLAGS_EXTRA)

CROSS_COMPILE=sparc-linux-

CC:=$(CROSS_COMPILE)gcc
LD:=$(CROSS_COMPILE)ld
OBJCOPY:=$(CROSS_COMPILE)objcopy

all:
	$(CC) --version
	$(CC) -c $(MCFLAGS) $(SRCDIR)/startup.c -o $(O)/startup.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/startup_ambapp.c -o $(O)/startup_ambapp.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/startup_ambapp_names.c -o $(O)/startup_ambapp_names.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom.c -o $(O)/prom.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_amba.c -o $(O)/prom_amba.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_amba_custom.c -o $(O)/prom_amba_custom.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_math.c -o $(O)/prom_math.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_no.c -o $(O)/prom_no.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_string.c -o $(O)/prom_string.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_debug.c -o $(O)/prom_debug.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/prom_watchdog.c -o $(O)/prom_watchdog.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/boot.c -o $(O)/boot.o
	$(CC) -c $(MCFLAGS) $(SRCDIR)/head.S -o $(O)/head.o
	$(OBJCOPY) -O binary -R .note -R .comment -S ${LINUX_IMAGE} $(O)/image_linux.bin
	$(LD) -z noexecstack -r -b binary $(O)/image_linux.bin -o $(O)/image_linux -g
	$(CC) -E -P -undef -nostdinc $(MCFLAGS) $(SRCDIR)/mklinuximg_boot.lds.S \
			-o $(O)/mklinuximg.lds
	$(LD) -X -T $(O)/mklinuximg.lds -o $(O)/image \
		$(O)/image_linux $(O)/boot*.o $(O)/head*.o \
		$(O)/startup*.o $(O)/prom*.o

clean:
	rm -f $(O)/*.o $(O)/mklinuximg.lds \
	      $(O)/image_linux $(O)/image_linux.bin \
	      $(O)/scanxml
