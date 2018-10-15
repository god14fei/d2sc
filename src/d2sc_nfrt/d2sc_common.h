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
                               d2sc_common.h
                       Shared data between host and NFs
 *
 *****************************************************************************/


#ifndef _D2SC_COMMON_H_
#define _D2SC_COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ether.h>

#include "d2sc_msg_common.h"

#define D2SC_MAX_SC_LENGTH 6		// the maximum service chain length
#define MAX_NFS 16							// total numer of NF instances allowed
#define MAX_NTS 16							// total numer of different NF types allowed
#define MAX_NFS_PER_NT 8				// max numer of NF instances per NF type

#define NF_BQ_SIZE 16384			//size of the NF buffer queue

#define PKT_RD_SIZE ((unit16_t)32)

#define D2SC_NF_ACT_DROP 0		// NF instance drop the packet
#define D2SC_NF_ACT_NEXT 1   // to perform the next action determined by the flow table lookup
#define D2SC_NF_ACT_TONF 2		// sent the packet to a specified NF instance
#define D2SC_NF_ACT_OUT 3		// sent the packet to the NIC port 

//flag operations that should be used on d2sc_pkt_meta
#define D2SC_CHECK_BIT(flags, n) !!((flags) & (1 << (n)))
#define D2SC_SET_BIT(flags, n) ((flags) | (1 << (n)))
#define D2SC_CLEAR_BIT(flags, n) ((flags) & (0 << (n)))

struct d2sc_pkt_meta {
	unit8_t act;
	unit16_t dst;
	unit16_t src;
	unit8_t sc_index;
	unit8_t flags;
};

/*
 * Local buffers to put packets in, used to send packets in bursts to the 
 * NFs or to the NIC
 */
struct pkt_buf {
	struct rte_mbuf *buf[PKT_RD_SIZE];
	unit16_t cnt;
};

struct tx_thread {
	unsigned first_nf_id;
	unsigned last_nf_id;
	struct pkt_buf *tx_bufs;
};

/*
 * The buffer queue used by the manager
 */ 
struct mgr_bq {
	unsigned id;
	struct tx_thread *tx_thread;
	struct pkt_buf *rx_buf;
};

/*
 * The buffer queue used by the NFs
 */ 
struct nfs_bq {
	unsigned id;
	struct pkt_buf *tx_buf;
	struct pkt_buf *rx_buf;
};

struct rx_stats {
	unit64_t rx[RTE_MAX_ETHPORTS];
};

struct tx_stats {
	unit64_t tx[RTE_MAX_ETHPORTS];
	unit64_t tx_drop[RTE_MAX_ETHPORTS];
};

struct port_info {
	unit8_t n_ports;
	unit8_t id[RTE_MAX_ETHPORTS];
	struct ether_addr max[RTE_MAX_ETHPORTS];
	volatile struct tx_stats rx_stats;
	volatile struct rx_stats rx_stats;
};

/* 
 * The structure to describe an NF
 */
struct d2sc_nf_info {
	unit16_t inst_id;
	unit16_t type_id;
	unit8_t stat;
	const char *name;
};

/* 
 *The structure with states from the NFs
 */	
struct stats {
	unit64_t rx;
	unit64_t rx_drop;
	unit64_t tx;
	unit64_t tx_drop;
	unit64_t tx_buf;
	unit64_t tx_ret;
	unit64_t act_out;
	unit64_t act_tonf;
	unit64_t act_drop;
	unit64_t act_next;
	unit64_t act_buf;
};

/*
 * NF structure including all needed information in D2SC
 */
struct d2sc_nf {
	struct rte_ring *rx_q;
	struct rte_ring *tx_q;
	struct rte_ring *msg_q
	struct d2sc_nf_info *nf_info;
	unit8_t inst_id;
	
	volatile struct stats stats;
};

/* 
 * The structure to describe a service chain entry
 */
struct d2sc_sc_entry {
	unit16_t dst;
	unit8_t act;
};

struct d2sc_sc {
	struct d2sc_sc_entry sc_entry[D2SC_MAX_SC_LENGTH];
	unit8_t sc_len;
	int ref_cnt;
};

/* define common names for structures shared between manager and NF */
#define NF_RXQ_NAME "nf_%u_RX"
#define NF_TXQ_NAME "nf_%u_TX"
#define PKTMBUF_POOL_NAME "pktmbuf_pool"
#define PORT_MZ_INFO "port_info"
#define NF_MZ_INFO "nf_info"
#define NT_MZ_INFO "nt_info"
#define NF_PER_NT_MZ_INFO "nf_per_nt_info"
#define SCP_MZ_INFO "scp_info"
#define FTP_MZ_INFO "ftp_info"

#define MGR_MSG_Q_NAME "mgr_msg_q"
#define NF_MSG_Q_NAME "nf_%u_msg_q"
#define NF_INFO_MP_NAME "nf_info_mp"
#define NF_MSG_MP_NAME "nf_msg_mp"

