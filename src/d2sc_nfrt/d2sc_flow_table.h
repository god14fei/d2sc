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

#ifndef _D2SC_FLOW_TABLE_H_
#define _D2SC_FLOW_TABLE_H_

#include <string.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_thash.h>
#include "d2sc_pkt_helper.h"
#include "d2sc_common.h"

extern uint8_t rss_symmetric_key[40];

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC	rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC	rte_jhash
#endif

struct d2sc_ft {
	struct rte_hash *hash;
	char *data;
	int cnt;
	int entry_size;
};

struct d2sc_ft_ipv4_5tuple {
	uint32_t src_addr;
	uint32_t dst_addr;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
};

union ipv4_5tuple_host {
	struct {
		uint8_t  pad0;
		uint8_t  proto;
		uint16_t virt_port;
		uint32_t ip_src;
		uint32_t ip_dst;
		uint16_t port_src;
		uint16_t port_dst;
	};
	__m128i xmm;
};


struct d2sc_ft *d2sc_ft_create(int cnt, int entry_size);

int d2sc_ft_lookup_pkt(struct d2sc_ft *table, struct rte_mbuf *pkt, char **data);


/*******************************Helper function*******************************/

static inline int d2sc_ft_fill_key(struct d2sc_ft_ipv4_5tuple *key, rte_mbuf *pkt) {
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;
	struct udp_hdr *udp_hdr;
	
	if (unlikely(!d2sc_pkt_is_ipv4(pkt))) {
		return -EPROTONOSUPPORT;
	}
	ipv4_hdr = d2sc_pkt_ipv4_hdr(pkt);
	memset(key, 0, sizeof(struct d2sc_ft_ipv4_5tuple));
	key->proto = ipv4_hdr->nex_proto_id;
	key->src_addr = ipv4_hdr->src_addr;
	key->dst_addr = ipv4_hdr->dst_addr;
	if (key->proto == IP_PROTOCOL_TCP) {
		tcp_hdr = d2sc_pkt_tcp_hdr(pkt);
		key->src_port = tcp_hdr->src_port;
		key->dst_port = tcp_hdr->dst_port;
	} else if (key->proto == IP_PROTOCOL_UDP) {
		udp_hdr = d2sc_pkt_udp_hdr(pkt);
		key->src_port = udp_hdr->src_port;
		key->dst_port = udp_hdr->dst_port;		
	} else {
		key->src_port = 0;
		key->dst_port = 0;
	}
	return 0;
}

static inline char *d2sc_ft_get_data(struct d2sc_ft *table, int32_t index) {
	return &table->data[index*table->entry_size];
}


#endif //	_D2SC_FLOW_TABLE_H_