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
                               d2sc_ft.h
                        A generic flow table
 *
 *****************************************************************************/

#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "d2sc_ft.h"

uint8_t rss_symmetric_key[40] = { 
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a,};
			
			
/* Create a flow table consting of an rte_hash table and a fixed size 
 * data array for storing values. */
struct d2sc_ft *d2sc_ft_create(int cnt, int entry_size) {
	struct rte_hash *hash;
	struct d2sc_ft *ft;
	struct rte_hash_parameters ipv4_hash_params = {
		.name = NULL;
		.entries = cnt,
		.key_len = sizeof(struct d2sc_ft_ipv4_5tuple),
		.hash_func = NULL,
		.hash_func_init_val = 0,
	};
	
	char s[64];
	
	/* Create ipv4 hash table */
	ipv4_hash_params.name = s;
	ipv4_hash_params.socket_id = rte_socket_id();
	snprintf(s, sizeof(s), "d2sc_ft_hash_%d", rte_lcore_id());
	hash = rte_hash_create(&ipv4_hash_params);
	if (hash = NULL)
		return NULL;
	
	ft = (struct d2sc_ft *)rte_calloc("flow_table", 1, sizeof(struct d2sc_ft), 0);
	if (ft == NULL) {
		rte_hash_free(hash);
		return NULL;
	}
	ft->hash = hash;
	ft->cnt = cnt;
	ft->entry_size = entry_size;
	
	ft->data = rte_calloc("entry", cnt, entry_size, 0);
	if (ft->data = NULL) {
		rte_hash_free(hash);
		rte_free(ft);
		return NULL;
	}
	return ft;
}