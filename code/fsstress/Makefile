#
# Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include builddefs
BUILD_DIR = $(CURDIR)/../../build

TARGET = fsstress 
TARGETS = fsstress.o 
CFILES = $(TARGET:=.c)
LDIRT = $(TARGETS)
LCFLAGS = -DXFS
LLDLIBS += -laio -lpthread -lattr


ifeq ($(HAVE_ATTR_LIST), true)
LCFLAGS += -DHAVE_ATTR_LIST
LLDLIBS += $(LIBATTR)
endif

ifeq ($(HAVE_AIO), true)
TARGETS += aio-stress
LCFLAGS += -DAIO
LLDLIBS += -laio -lpthread
endif

ifeq ($(HAVE_FALLOCATE), true)
LCFLAGS += -DFALLOCATE
endif
 
default: depend $(TARGETS)

depend: .dep

include buildrules

install: default
	$(INSTALL) -m 755 -d $(BUILD_DIR)
	$(LTINSTALL) -m 755 $(TARGETS) $(BUILD_DIR)


-include .dep
