NAME  = bootloader1

SUPPORT_BOOT_NUM = 2

OBJS += bootloader1.o
OBJS += ../vendor/trezor-crypto/memzero.small.o


OPTFLAGS ?= -Os

include ../Makefile.include

align: $(NAME).bin
	./firmware_align.py $(NAME).bin
