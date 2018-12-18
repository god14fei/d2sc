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

#ifndef _D2SC_NFRT_H_
#define _D2SC_NFRT_H_

#include <rte_mbuf.h>
#include "d2sc_common.h"
#include "d2sc_pkt_process.h"

/************************************API**************************************/

static int d2sc_nfrt_init_premises(int argc, char *argv[]);


static int d2sc_nfrt_start_nf(struct d2sc_nf_info *nf_info);


int d2sc_nfrt_init(int argc, char *argv[], const char *nf_name, struct d2sc_nf_info **nf_info_p);


int d2sc_nfrt_run_callback(struct d2sc_nf_info *info, 
	int(*pkt_handler)(struct rte_mbuf *pkt, struct d2sc_pkt_meta *meta, __attribute__((unused)) struct d2sc_nf_info *nf_info), 
	int(*cbk_handler)(__attribute__((unused)) struct d2sc_nf_info *nf_info));


int d2sc_nfrt_run(struct d2sc_nf_info *info,
	int(*pkt_handler)(struct rte_mbuf *pkt, struct d2sc_pkt_meta *act, __attribute__((unused)) struct d2sc_nf_info *nf_info));


int d2sc_nfrt_ret_pkt(struct rte_mbuf *pkt, struct d2sc_nf_info *info);


int d2sc_nfrt_nf_ready(struct d2sc_nf_info *info);


int d2sc_nfrt_handle_new_msg(struct d2sc_nf_msg *msg, struct d2sc_nf *nf);


void d2sc_nfrt_check_scale_msg(struct d2sc_nf *nf);


void d2sc_nfrt_stop(struct d2sc_nf_info *info);


void d2sc_nfrt_scale_block(struct d2sc_nf_info *info);


void d2sc_nfrt_scale_run(struct d2sc_nf_info *info);


// struct rte_ring *d2sc_nfrt_get_tx_ring(struct d2sc_nf_info *info);


// struct rte_ring *d2sc_nfrt_get_rx_ring(struct d2sc_nf_info *info);


// struct d2sc_nf *d2sc_nfrt_get_nf(uint16_t id);


struct d2sc_sc *d2sc_nfrt_get_default_sc(void);


int d2sc_nfrt_scale_nfs(struct d2sc_nf_info *nf_info, uint16_t num_nfs);


#endif	// _D2SC_NFRT_H_
