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

#ifndef _D2SC_SC_H_
#define _D2SC_SC_H_

#include <inttypes.h>
#include <rte_mbuf.h>
#include "d2sc_common.h"

/***************************Extern Global variables***************************/

extern struct d2sc_nf *nfs;
extern uint16_t **nts;
extern uint16_t *nfs_per_nt_num;

/********************************Interfaces***********************************/

void d2sc_sc_nt_to_nf_map(uint16_t type_id, struct rte_mbuf *pkt);

int d2sc_sc_merge_entry(struct d2sc_sc *sc, uint8_t act, uint16_t dst);

int d2sc_sc_set_entry(struct d2sc_sc *sc, uint8_t index, uint8_t act, uint16_t dst);

void d2sc_sc_print(struct d2sc_sc *sc);

static inline uint8_t d2sc_sc_next_act(struct d2sc_sc *sc, struct rte_mbuf *pkt);

static inline uint16_t d2sc_sc_next_dst(struct d2sc_sc *sc, struct rte_mbuf *pkt);

/* get service chain */
//struct d2sc_sc * d2sc_sc_get(void);

/* create service chain */
struct d2sc_sc * d2sc_sc_crate(void);


#endif // _D2SC_SC_H_