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
#include <rte_cycles.h>

#include "d2sc_msg.h"

#define D2SC_MAX_SC_LEN 6		// the maximum service chain length
#define MAX_NFS 16							// max number of NF instances allowed
#define MAX_NEW_NFS 8						// max number of new NFs that are for scaling
#define MAX_NTS 16							// total number of different NF types allowed
#define MAX_NFS_PER_NT 8				// max number of NF instances per NF type

#define NF_RING_SIZE 16384			//size of the NF buffer queue

#define PKT_RD_SIZE ((uint16_t)32)

#define D2SC_NF_ACT_DROP 0		// NF instance drop the packet
#define D2SC_NF_ACT_NEXT 1   // to perform the next action determined by the flow table lookup
#define D2SC_NF_ACT_TONF 2		// sent the packet to a specified NF instance
#define D2SC_NF_ACT_OUT 3		// sent the packet to the NIC port 

//flag operations that should be used on d2sc_pkt_meta
#define D2SC_CHECK_BIT(flags, n) !!((flags) & (1 << (n)))
#define D2SC_SET_BIT(flags, n) ((flags) | (1 << (n)))
#define D2SC_CLEAR_BIT(flags, n) ((flags) & (0 << (n)))

struct d2sc_pkt_meta {
	uint8_t act;
	uint16_t dst;
	uint16_t src;
	uint8_t sc_index;
	uint8_t flags;
};

/*
 * Local buffers to put packets in, used to send packets in bursts to the 
 * NFs or to the NIC
 */
struct pkt_buf {
	struct rte_mbuf *buf[PKT_RD_SIZE];
	uint16_t cnt;
};

struct tx_thread {
	unsigned first_nf_id;
	unsigned last_nf_id;
	struct pkt_buf *tx_bufs;
};

/*
 * The buffer queue used by the manager
 */ 
struct buf_queue {
	unsigned id;
	uint8_t mgr_nf;		// mgr uses buf_queue if mgr_nf = 1, else nf uses buf_queue
	union {
			struct tx_thread *tx_thread;
			struct pkt_buf *tx_buf;
	};
	struct pkt_buf *rx_bufs;
};

/*
 * The buffer queue used by the NFs
 */ 
//struct nf_bq {
//	unsigned id;
//	struct pkt_buf *tx_buf;
//	struct pkt_buf *rx_bufs;
//};

struct port_info {
	uint8_t n_ports;
	uint8_t id[RTE_MAX_ETHPORTS];
	struct ether_addr mac[RTE_MAX_ETHPORTS];
	
	/* Structures about stats of ports */
	struct {
		volatile uint64_t rx[RTE_MAX_ETHPORTS];
	} rx_stats;
	struct {
		volatile uint64_t tx[RTE_MAX_ETHPORTS];
		volatile uint64_t tx_drop[RTE_MAX_ETHPORTS];
	} tx_stats;
};

/* 
 * The structure to describe an NF
 */
struct d2sc_nf_info {
	uint16_t inst_id;
	uint16_t type_id;
	uint8_t status;
	uint16_t srv_time;		// service time of an NF 
	uint64_t max_load;		// max load of an NF, specified by the provider
	const char *name;
};

typedef int(*pkt_handler)(struct rte_mbuf *pkt, struct d2sc_pkt_meta *meta,
	__attribute__((unused)) struct d2sc_nf_info *nf_info);
typedef int(*cbk_handler)(__attribute__((unused)) struct d2sc_nf_info *nf_info);

/* 
 *The structure with states from the NFs
 */	
struct stats {
	uint64_t rx;
	uint64_t rx_drop;
	uint64_t tx;
	uint64_t tx_drop;
	uint64_t tx_buf;
	uint64_t tx_ret;
	uint64_t act_out;
	uint64_t act_tonf;
	uint64_t act_drop;
	uint64_t act_next;
	uint64_t act_buf;
};

/*
 * NF structure including all needed information in D2SC
 */
struct d2sc_nf {
	struct rte_ring *rx_q;
	struct rte_ring *tx_q;
	struct rte_ring *msg_q;
	struct d2sc_nf_info *nf_info;
	uint16_t inst_id;
	
	uint8_t ol_flag;		// NF overlaod flag
	uint8_t bk_flag;		// NF block flag, 0-normal, 1-block_signal, 2-block acually
	uint16_t pkt_rate;		// Packet arrival rate through the NF 
	uint16_t scale_num;		// The scale number of the NF
	
	struct buf_queue *nf_bq;
	uint16_t parent_nf;
	
	/* NF specific function */
	pkt_handler handler;
	cbk_handler callback;
	
	volatile struct stats stats;
};

/* 
 * The structure to describe a service chain entry
 */
struct d2sc_sc_entry {
	uint16_t dst;
	uint8_t act;
};

struct d2sc_sc {
	struct d2sc_sc_entry sc_entry[D2SC_MAX_SC_LEN];
	uint8_t sc_len;
	int ref_cnt;
};

/* define common names for structures shared between manager and NF */
#define NF_RXQ_NAME "nf_%u_RX"
#define NF_TXQ_NAME "nf_%u_TX"

#define MZ_PORT_INFO "port_info"
#define MZ_NF_INFO "nf_info"
#define MZ_NTS_INFO "nts_info"
#define MZ_NFS_PER_NT_INFO "nfs_per_nt_info"
#define MZ_NT_AVAILABLE_INFO "nt_available_info"
#define MZ_SCP_INFO "scp_info"
#define MZ_FTP_INFO "ftp_info"

#define MP_PKTMBUF_NAME "pktmbuf_mp"
#define MP_NF_INFO_NAME "nf_info_mp"   // Mempool name for the NF info
#define NF_SCALE_INFO_NAME "nf_%u_scale_info_%"PRIu64

/* common names for NF states */
#define NF_WAITTING_FOR_ID 0		// Begin in an startup process, and has no ID registered by manager yet
#define NF_STARTING 1 					// In an startup process, and already has an ID
#define NF_RUNNING 2						// Running normally
#define NF_BLOCKED 3						// Not receiving packets temporarily, but may regain in the future
#define NF_STOPPED 4						// Stopped in a shupdown process
#define NF_ID_CONFLICT 5 				// ID is conflicted with an already used ID
#define NF_NO_IDS	6							// There are no IDs for this NF

#define NF_NO_ID -1
#define D2SC_NF_HANDLE_TX 1			// True if NFs start to pass packets to each other


/***************************    helper functions   ***************************/
static inline struct d2sc_pkt_meta *d2sc_get_pkt_meta(struct rte_mbuf *pkt) {
	return (struct d2sc_pkt_meta *)&pkt->udata64;
}

static inline uint8_t d2sc_get_pkt_sc_index(struct rte_mbuf *pkt) {
	return ((struct d2sc_pkt_meta *)&pkt->udata64)->sc_index;
}

/* 
 * Get the rx queue name with an NF ID
 */
static inline const char * get_rx_queue_name(unsigned id) {
	static char buffer[sizeof(NF_RXQ_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_RXQ_NAME, id);
	return buffer;	
}

/* 
 * Get the tx queue name with an NF ID
 */
static inline const char * get_tx_queue_name(unsigned id) {
	static char buffer[sizeof(NF_TXQ_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_TXQ_NAME, id);
	return buffer;	
}

/* 
 * Get the scale info name with an NF ID
 */
static inline const char * get_scale_info_name(unsigned id) {
	static char buffer[sizeof(NF_SCALE_INFO_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_SCALE_INFO_NAME, id, rte_get_tsc_cycles());
	return buffer;
}

/*
 * Function checks if a given NF is valid, meaning if it is running
 */
static inline int d2sc_nf_is_valid(struct d2sc_nf *nf) {
	return nf && nf->nf_info && nf->nf_info->status == NF_RUNNING;
}

#define RTE_LOGTYPE_MGR RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_NFRT RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_NF RTE_LOGTYPE_USER1

#endif // _D2SC_COMMON_H_
