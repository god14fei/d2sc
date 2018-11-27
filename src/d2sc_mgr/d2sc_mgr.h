/*********************************************************************
 *                            D2SC
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
                                   d2sc_mgr.h
      Header file containing all shared headers and data structures

******************************************************************************/

#ifndef _D2SC_MGR_H_
#define _D2SC_MGR_H_


/******************************Standard C library*****************************/

#include <netinet/ip.h>
#include <stdbool.h>
#include <math.h>


/********************************DPDK library*********************************/

#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>


/******************************Internal headers*******************************/

#include "d2sc_mgr/d2sc_parse.h"
#include "d2sc_mgr/d2sc_init.h"
#include "d2sc_mgr/d2sc_scale.h"
#include "d2sc_includes.h"
#include "d2sc_sc.h"
#include "d2sc_flow_table.h"
#include "d2sc_flow_steer.h"
#include "d2sc_pkt_process.h"


/***********************************Macros************************************/

#define TO_PORT 0
#define TO_NF 1


/***************************Shared global variables***************************/

/* ID to be assigned to the next NF instance that starts */
extern uint16_t next_inst_id;

#endif // _D2SC_MGR_H_