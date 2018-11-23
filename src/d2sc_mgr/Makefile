#
#                           D2SC
#             https://github.com/god14fei/d2sc
#
#   BSD LICENSE
#
#   Copyright(c)
#            2018-2019 Huazhong University of Science and Technology
#
#   All rights reserved.
#
#

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV),"linuxapp")
$(error This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# binary name
APP = d2sc_mgr

#all source are stored in SRCS-y
SRCS-y :=main.c d2sc_init.c d2sc_parse.c d2sc_stats.c d2sc_nf.c d2sc_scale.c

INC := d2sc_mgr.h d2sc_init.h d2sc_parse.h d2sc_stats.h d2sc_nf.h d2sc_scale.h

CFLAGS += -g
CFLAGS += -I$(SRCDIR)/../ -I$(SRCDIR)/../d2sc_nfrt/
LDFLAGS += $(SRCDIR)../d2sc_nfrt/$(RTE_TARGET)/libd2sc.a

# for newer gcc, e.g. 4.4, no-strict-aliasing may not be necessary
# and so the next line can be removed in those cases.
# EXTRA_CFLAGS += -fno-strict-aliasing

include $(RTE_SDK)/mk/rte.extapp.mk