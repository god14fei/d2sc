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

RTE_TARGET ?= $(RTE_TARGET)

# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = monitor

# all source are stored in SRCS-y
SRCS-y := monitor.c

# D2SC path
D2SC= $(SRCDIR)/../../src

CFLAGS += -g

CFLAGS += -I$(D2SC) -I$(D2SC)/d2sc_nfrt -I$(D2SC)/d2sc_mgr
LDFLAGS += $(D2SC)/d2sc_nfrt/$(RTE_TARGET)/libd2sc.a

include $(RTE_SDK)/mk/rte.extapp.mk