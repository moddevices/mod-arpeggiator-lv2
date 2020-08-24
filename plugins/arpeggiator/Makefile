#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX, Christopher Arndt, and Patrick Desaulniers
#

# --------------------------------------------------------------
# Installation directories

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
LV2_DIR ?= $(LIBDIR)/lv2

# --------------------------------------------------------------
# Project name, used for binaries

NAME = mod-arpeggiator

# --------------------------------------------------------------
# Plugin types to build

BUILD_LV2 ?= true

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	plugin.cpp \
	arpeggiator.cpp \
	utils.cpp \
	../../common/midiHandler.cpp \
	../../common/clock.cpp \
	../../common/pattern.cpp \

# --------------------------------------------------------------
# Do some magic

include Makefile.mk

# --------------------------------------------------------------
# Enable all selected plugin types

ifeq ($(BUILD_LV2),true)
ifeq ($(HAVE_DGL),true)
TARGETS += lv2_sep
else
TARGETS += lv2_dsp
endif
endif

all: $(TARGETS)

install: all
ifeq ($(BUILD_LV2),true)
	@install -dm755 $(DESTDIR)$(LV2_DIR) && \
		cp -rf $(TARGET_DIR)/$(NAME).lv2 $(DESTDIR)$(LV2_DIR)
endif

install-user: all
ifeq ($(BUILD_LV2),true)
	@install -dm755 $(HOME)/.lv2 && \
		cp -rf $(TARGET_DIR)/$(NAME).lv2 $(HOME)/.lv2
endif

# --------------------------------------------------------------

.PHONY: all install install-user
