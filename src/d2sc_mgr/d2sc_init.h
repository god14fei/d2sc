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

                                 d2sc_init.h
                                 
       Header for the initialisation function and global variables and
       data structures.
       
******************************************************************************/

#ifndef _D2SC_INIT_H_
#define _D2SC_INIT_H_

/********************************DPDK library*********************************/

#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif

/*****************************Internal library********************************/

#include "d2sc_mgr/d2sc_parse.h"
#include "d2sc_mgr/d2sc_stats.h"
#include "d2sc_sc.h"
#include "d2sc_includes.h"
#include "d2sc_common.h"

#include "d2sc_flow_table.h"
#include "d2sc_flow_steer.h"

/***********************************Macros************************************/

#define MBUFS_PER_NF 1536
#define MBUFS_PER_PORT 1536
#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define NF_INFO_SIZE sizeof(struct d2sc_nf_info)
#define NF_INFO_CACHE_SIZE 8

#define NF_MSG_SIZE sizeof(struct d2sc_nf_msg)
#define NF_MSG_CACHE_SIZE 8

#define RTE_RX_DESC_DEFAULT 512
#define RTE_TX_DESC_DEFAULT 512
#define NF_MSG_RING_SIZE 128

#define NO_FLAGS 0

#define D2SC_NUM_RX_THREADS 1

/*************************External global variables***************************/

/* ring queue for NF to Manager message */
extern struct rte_ring *new_msg_ring;

/* ring queue for Manager to NF scale message */
struct rte_ring *scale_msg_ring;

/* the shared port information: port numbers, rx and tx stats, etc. */
extern struct port_info *ports;

extern struct rte_mempool *pktmbuf_mp;
extern struct rte_mempool *nf_msg_mp;
extern uint16_t num_nfs;
extern uint16_t num_nts;
extern uint16_t default_nt;
extern uint16_t num_rx_threads;
extern uint16_t **nts;
extern uint16_t *nfs_per_nt_num;
extern uint16_t *nfs_per_nt_available;
extern unsigned num_sockets;
extern struct d2sc_sc *default_sc;
extern struct d2sc_ft *ft;

extern D2SC_STATS_OUTPUT stats_dst;
extern uint16_t global_stats_sleep_time;

/**********************************Functions**********************************/

/*
 * Function that initialize all data structures, memory mapping and global
 * variables.
 *
 * Input  : the number of arguments (following C conventions)
 *          an array of the arguments as strings
 * Output : an error code
 *
 */
int init(int argc, char *argv[]);

#endif // _D2SC_INIT_H_
