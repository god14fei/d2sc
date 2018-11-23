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
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
LIB = libd2sc.a

# all source are stored in SRCS-y
SRCS-y := d2sc_pkt_process.c d2sc_sc.c d2sc_pkt_helper.c d2sc_flow_steer.c d2sc_flow_table.c d2sc_nfrt.c

CFLGAS += -g

include $(RTE_SDK)/mk/rte.extlib.mk