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


#ifndef _D2SC_FLOW_STEER_H_
#define _D2SC_FLOW_STEER_H_

#include "d2sc_common.h"
#include "d2sc_flow_table.h"

extern struct d2sc_ft *ft;
extern struct d2sc_ft **ft_p;

struct d2sc_flow_entry {
	struct d2sc_ft_ipv4_5tuple *key;
	struct d2sc_sc *sc;
	uint64_t ref_cnt;
	uint64_t idle_timeout;
	uint64_t hard_timeout;
	uint64_t pkt_cnt;
	uint64_t byte_cnt;
};


int d2sc_fs_init(void);

int d2sc_fs_get_entry(rte_mbuf *pkt, struct d2sc_flow_entry **flow_entry);



#endif //	_D2SC_FLOW_STEER_H_