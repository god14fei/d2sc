/*********************************************************************
 *                           D2SC
 *              https://github.com/god14fei/d2sc
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2018-2019 Huazhong University of Science and Technology
 *
 *   All rights reserved.
 *
 ********************************************************************/
 
/******************************************************************************
                               onvm_includes.h
         Header file containing all shared headers and data structures
 *
 *****************************************************************************/
 
 #ifndef _D2SC_INCLUDES_H_
 #define _D2SC_INCLUDES_H_
 
 
/******************************Standard C library*****************************/
 
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>



/********************************DPDK library*********************************/
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_atomic.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

/******************************Internal headers*******************************/


#include "d2sc_common.h"

#endif // _D2SC_INCLUDES_H_