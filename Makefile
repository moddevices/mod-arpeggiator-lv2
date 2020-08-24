#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX, Christopher Arndt, and Patrick Desaulniers
#

include dpf/Makefile.base.mk

all: libs plugins gen

# --------------------------------------------------------------

submodules:
	git submodule update --init --recursive

libs:

plugins: libs
	$(MAKE) all -C plugins/arpeggiator

ifneq ($(CROSS_COMPILING),true)
gen: plugins dpf/utils/lv2_ttl_generator
	#@$(CURDIR)/dpf/utils/generate-ttl.sh
	cp -r static-lv2-data/mod-arpeggiator.lv2/* bin/mod-arpeggiator.lv2/
ifeq ($(MACOS),true)
	@$(CURDIR)/dpf/utils/generate-vst-bundles.sh
endif
dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator
else
gen: plugins dpf/utils/lv2_ttl_generator.exe
	#@$(CURDIR)/dpf/utils/generate-ttl.sh
	cp -r static-lv2-data/mod-arpeggiator.lv2/* bin/mod-arpeggiator.lv2/

dpf/utils/lv2_ttl_generator.exe:
	$(MAKE) -C dpf/utils/lv2-ttl-generator WINDOWS=true
endif

# --------------------------------------------------------------

clean:
	$(MAKE) clean -C dpf/utils/lv2-ttl-generator
	$(MAKE) clean -C plugins/arpeggiator
	rm common/*.o common/*.d
	rm -rf bin build

install: all
	$(MAKE) install -C plugins/arpeggiator

install-user: all
	$(MAKE) install-user -C plugins/arpeggiator

# --------------------------------------------------------------

.PHONY: all clean install install-user plugins submodule
