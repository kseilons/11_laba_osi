CC := gcc-11
obj-m += xwrand.o
CFLAGS_fibmod.o += -std=gnu99

V ?= 2

KBUILDDIR ?= /lib/modules/$(shell uname -r)/build


MAKEFLAGS += $(if $(value V),,--no-print-directory)

.PHONY: modules clean tidy

modules: 
	$(MAKE) -C "$(KBUILDDIR)" M="$(PWD)" V=$(V) modules

tidy:
	$(MAKE) -C "$(KBUILDDIR)" M="$(PWD)" V=$(V) clean

