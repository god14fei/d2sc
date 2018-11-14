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


#include <errno.h>
#include <inttypes.h>
#include "d2sc_common.h"
#include "d2sc_sc.h"



/*********************************Interfaces**********************************/

uint16_t d2sc_sc_nt_to_nf_map(uint16_t type_id, struct rte_mbuf *pkt) {
	if (!nts || !nfs_per_nt_num) {
		rte_exit(EXIT_FAILURE, "Failed to retrieve NF map information\n");
	}
	uint16_t num_nfs_available = nfs_per_nt_num[type_id];
	
	if (num_nfs_available == 0)
		return 0;
		
	if (pkt == NULL)
		return 0;
		
	uint16_t inst_index = pkt->hash.rss % num_nfs_available;
	uint16_t inst_id = nts[type_id][inst_index];
	
	return inst_id;
}

int d2sc_sc_merge_entry(struct d2sc_sc *sc, uint8_t act, uint16_t dst) {
	int sc_len = sc->sc_len;
	
	if (unlikely(sc_len > D2SC_MAX_SC_LEN)) 
		return ENOSPC;
		
	/* the first entry is reserved */
	sc_len++;
	(sc->sc_len)++;
	sc->sc_entry[sc_len].act = act;
	sc->sc_entry[sc_len].dst = dst;
	
	return 0;
}

int d2sc_sc_set_entry(struct d2sc_sc *sc, uint8_t index, uint8_t act, uint16_t dst) {
	if (unlikely(index > sc->sc_len)) 
		return -1;
		
	sc->sc_entry[index].act = act;
	sc->sc_entry[index].dst = dst;
	
	return 0;		
}

void d2sc_sc_print(struct d2sc_sc *sc) {
	uint8_t i;
	for (i = 0; i < sc->sc_len; i++) {
		printf("curent_index: %"PRIu8", action: %"PRIu8", destination: %"PRIu16"\n",
			i, sc->sc_entry[i].act, sc->sc_entry[i].dst);
	}
	printf("\n");
}


static inline uint8_t d2sc_sc_next_act(struct d2sc_sc *sc, struct rte_mbuf *pkt) {
	uint8_t cur_index = d2sc_get_pkt_sc_index(pkt);
	
	if (unlikely(cur_index >= sc->sc_len))
		return D2SC_NF_ACT_DROP;
		
	return sc->sc_entry[cur_index+1].act;
}

static inline uint16_t d2sc_sc_next_dst(struct d2sc_sc *sc, struct rte_mbuf *pkt) {
	uint8_t cur_index = d2sc_get_pkt_sc_index(pkt);
	
	if (unlikely(cur_index >= sc->sc_len))
		return 0;
	
	return sc->sc_entry[cur_index+1].dst;
}

struct d2sc_sc * d2sc_sc_crate(void) {
	struct d2sc_sc *sc;
	sc = rte_calloc("D2SC_service_chain", 1, sizeof(struct d2sc_sc), 0);
	if (sc == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot allocate memory for service chain\n");
	}
	
	return sc;
}