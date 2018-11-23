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

#ifndef _D2SC_PKT_HELPER_H_
#define _D2SC_PKT_HELPER_H_

#include <inttypes.h>
#include <rte_mempool.h>

struct port_info;
struct rte_mbuf;
struct tcp_hdr;
struct udp_hdr;
struct ipv4_hdr;

#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

#ifndef TCP_FLAGS
#define TCP_FLAGS
#define TCP_FLAG_FIN (1<<0)
#define TCP_FLAG_SYN (1<<1)
#define TCP_FLAG_RST (1<<2)
#define TCP_FLAG_PSH (1<<3)
#define TCP_FLAG_ACK (1<<4)
#define TCP_FLAG_URG (1<<5)
#endif
#define TCP_FLAG_ECE (1<<6)
#define TCP_FLAG_CWR (1<<7)
#define TCP_FLAG_NS  (1<<8)

#define SUPPORTS_IPV4_CHECKSUM_OFFLOAD (1<<0)
#define SUPPORTS_TCP_CHECKSUM_OFFLOAD (1<<1)
#define SUPPORTS_UDP_CHECKSUM_OFFLOAD (1<<2)

/* Returns the bitflags in the tcp header */
#define ONVM_PKT_GET_FLAGS(tcp, flags) \
	do { \
		(flags) = (((tcp)->data_off << 8) | (tcp)->tcp_flags) & 0b111111111; \
	} while (0)

/* Sets the bitflags in the tcp header */
#define ONVM_PKT_SET_FLAGS(tcp, flags) \
	do { \
		(tcp)->tcp_flags = (flags) & 0xFF; \
		(tcp)->data_off |= ((flags) >> 8) & 0x1; \
	} while (0)
	
struct ether_hdr* d2sc_pkt_ether_hdr(struct rte_mbuf* pkt);

struct ipv4_hdr * d2sc_pkt_ipv4_hdr(struct rte_mbuf *pkt);

struct tcp_hdr *d2sc_pkt_tcp_hdr(struct rte_mbuf *pkt);

struct udp_hdr *d2sc_pkt_udp_hdr(struct rte_mbuf *pkt);

int d2sc_pkt_is_ipv4(struct rte_mbuf *pkt);

int d2sc_pkt_is_tcp(struct rte_mbuf *pkt);

int d2sc_pkt_is_udp(struct rte_mbuf *pkt);

void d2sc_pkt_print(struct rte_mbuf* pkt);

void d2sc_pkt_print_ipv4(struct ipv4_hdr* hdr);

void d2sc_pkt_print_tcp(struct tcp_hdr* hdr);

void d2sc_pkt_print_udp(struct udp_hdr* hdr);

int d2sc_pkt_set_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, unsigned dst_port_id, struct port_info *ports);

int d2sc_pkt_swap_src_mac_addr(struct rte_mbuf* pkt, unsigned dst_port_id, struct port_info *ports);

int d2sc_pkt_swap_dst_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, struct port_info *ports);


#endif //	_D2SC_PKT_HELPER_H_