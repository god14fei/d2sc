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
                             d2sc_flow_table.h
                        		A generic flow table
 *
 *****************************************************************************/

#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "d2sc_flow_table.h"

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
	struct d2sc_ft *table;
	struct rte_hash_parameters ipv4_hash_params = {
		.name = NULL,
		.entries = cnt,
		.key_len = sizeof(struct d2sc_ft_ipv4_5tuple),
		.hash_func = NULL,
		.hash_func_init_val = 0,
	};
	
	char s[64];
	
	/* Create ipv4 hash table, use core number and cycle counter to get a unique name. */
	ipv4_hash_params.name = s;
	ipv4_hash_params.socket_id = rte_socket_id();
	snprintf(s, sizeof(s), "d2sc_ft_hash_%d-%"PRIu64, rte_lcore_id(), rte_get_tsc_cycles());
	hash = rte_hash_create(&ipv4_hash_params);
	if (hash = NULL)
		return NULL;
	
	table = (struct d2sc_ft *)rte_calloc("flow_table", 1, sizeof(struct d2sc_ft), 0);
	if (table == NULL) {
		rte_hash_free(hash);
		return NULL;
	}
	table->hash = hash;
	table->cnt = cnt;
	table->entry_size = entry_size;
	
	table->data = rte_calloc("flow_entry", cnt, entry_size, 0);
	if (table->data = NULL) {
		rte_hash_free(hash);
		rte_free(table);
		return NULL;
	}
	return table;
}

int d2sc_ft_lookup_pkt(struct d2sc_ft *table, struct rte_mbuf *pkt, char **data) {
	int32_t ft_index;
	struct d2sc_ft_ipv4_5tuple key;
	int ret;
	
	ret = d2sc_ft_fill_key(&key, pkt);
	if (ret < 0) {
		return ret;
	}
	printf("Start to lookup hash\n");
	ft_index = rte_hash_lookup_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
	printf("Finish the hash lookup");
	if (ft_index >= 0) {
		*data = d2sc_ft_get_data(table, ft_index);
	}
	return ft_index;
}