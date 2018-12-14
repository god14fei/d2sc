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

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_ether.h>

#include "d2sc_nfrt.h"
#include "d2sc_pkt_helper.h"
#include "aes.h"

#define NF_NAME "encryptor"

/* Struct that contains information about this NF */
struct d2sc_nf_info *nf_info;

/* number of packets between each print */
static uint32_t print_delay = 1000000;

static uint32_t dest;

// True as long as the NF should keep processing packets
static uint8_t scaler_keep_running = 1;

/* AES encryption parameters */
BYTE key[1][32] = {
  {0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4}
};
BYTE iv[1][16] = {
  {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f}
};
WORD key_schedule[60]; //word Schedule

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
	printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
	int c, dst_flag = 0;

	while ((c = getopt(argc, argv, "d:p:")) != -1) {
		switch (c) {
			case 'd':
				dest = strtoul(optarg, NULL, 10);
				dst_flag = 1;
				break;
			case 'p':
				print_delay = strtoul(optarg, NULL, 10);
				break;
			case '?':
				usage(progname);
				if (optopt == 'd')
					RTE_LOG(INFO, NF, "Option -%c requires an argument.\n", optopt);
				else if (optopt == 'p')
					RTE_LOG(INFO, NF, "Option -%c requires an argument.\n", optopt);
				else if (isprint(optopt))
					RTE_LOG(INFO, NF, "Unknown option `-%c'.\n", optopt);
				else
					RTE_LOG(INFO, NF, "Unknown option character `\\x%x'.\n", optopt);
				return -1;
			default:
				usage(progname);
				return -1;
		}
	}

	if (!dst_flag) {
		RTE_LOG(INFO, NF, "AES Encrypt NF requires destination flag -d.\n");
		return -1;
	}

	return optind;
}


/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf* pkt) {
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	static uint64_t pkt_process = 0;
	struct ipv4_hdr* ip;

	pkt_process += print_delay;

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("PACKETS\n");
	printf("-----\n");
	printf("Port : %d\n", pkt->port);
	printf("Size : %d\n", pkt->pkt_len);
	printf("N°   : %"PRIu64"\n", pkt_process);
	printf("\n\n");

	ip = d2sc_pkt_ipv4_hdr(pkt);
	if (ip != NULL) {
		struct udp_hdr *udp;

		d2sc_pkt_print(pkt);
		/* Check if we have a valid UDP packet */
		udp = d2sc_pkt_udp_hdr(pkt);
		if (udp != NULL) {
			uint8_t *pkt_data;
			pkt_data = ((uint8_t *) udp) + sizeof(struct udp_hdr);
			printf("Payload : %.32s\n", pkt_data);
		}
	} else {
		printf("No IP4 header found\n");
	}
}

static int
packet_handler(struct rte_mbuf* pkt, struct d2sc_pkt_meta* meta, __attribute__((unused)) struct d2sc_nf_info *nf_info) {
	struct udp_hdr *udp;

	static uint32_t counter = 0;
	if (++counter == print_delay) {
		do_stats_display(pkt);
		counter = 0;
	}

	/* Check if we have a valid UDP packet */
	udp = d2sc_pkt_udp_hdr(pkt);
	if (udp != NULL) {
		uint8_t *	pkt_data;
		uint8_t *	eth;
		uint16_t	plen;
		uint16_t	hlen;

		/* Get at the payload */
		pkt_data = ((uint8_t *) udp) + sizeof(struct udp_hdr);
		/* Calculate length */
		eth = rte_pktmbuf_mtod(pkt, uint8_t *);
		hlen = pkt_data - eth;
		plen = pkt->pkt_len - hlen;

		/* Encrypt. */
		/* IV should change with every packet, but we don't have any
		* way to send it to the other side. */
		aes_encrypt_ctr(pkt_data, plen, pkt_data, key_schedule, 256, iv[0]);
		if (counter == 0) {
			printf("Encrypted %d bytes at offset %d (%ld)\n",
				plen, hlen, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
		}
	}

	meta->act = D2SC_NF_ACT_TONF;
	meta->dst = dest;
	return 0;
}

static int  scaled_nf_thread(void *arg) {
	RTE_LOG(INFO, NF, "Core %d: Runnning scaling thread\n", rte_lcore_id());
	
	while (scaler_keep_running) {
		// Keep checking whether this NF needs to scale
		d2sc_nfrt_check_scale_msg(nf_info);
	}
	
	return 0;
}

int main(int argc, char *argv[]) {
	int arg_offset;
	unsigned cur_lcore;

	const char *progname = argv[0];

	if ((arg_offset = d2sc_nfrt_init(argc, argv, NF_NAME, &nf_info)) < 0)
		return -1;
		
	argc -= arg_offset;
	argv += arg_offset;

	if (parse_app_args(argc, argv, progname) < 0) {
		d2sc_nfrt_stop(nf_info);
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
	}

	/* Initialise encryption engine. Key should be configurable. */
	aes_key_setup(key[0], key_schedule, 256);
	
	cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
	if (rte_eal_remote_launch(scaled_nf_thread, NULL, cur_lcore) == -EBUSY) {
		RTE_LOG(ERR, NF, "Core %d is already busy, cannot use for NF scaled thread", cur_lcore);
		return -1;
	}
	
	RTE_LOG(INFO, NF, "Core %d: Runnning initial thread\n", rte_lcore_id());
	
	d2sc_nfrt_run(nf_info, &packet_handler);
		printf("If we reach here, intial NF is ending\n");
	return 0;
}