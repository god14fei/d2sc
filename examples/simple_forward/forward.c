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

#include "d2sc_nfrt.h"
#include "d2sc_pkt_helper.h"

#define NF_NAME "simple_forward"
#define MAX_LOAD_SF 660

/* Struct that contains information about this NF */
struct d2sc_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 1000000;


static uint32_t destination;

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
				destination = strtoul(optarg, NULL, 10);
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
		RTE_LOG(INFO, NF, "Simple Forward NF requires destination flag -d.\n");
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
		d2sc_pkt_print(pkt);
	} else {
		 printf("No IP4 header found\n");
	}
}

static int
packet_handler(struct rte_mbuf *pkt, struct d2sc_pkt_meta *meta, __attribute__((unused)) struct d2sc_nf_info *nf_info) {
	static uint32_t counter = 0;
	if (++counter == print_delay) {
		do_stats_display(pkt);
		counter = 0;
	}

	meta->act = D2SC_NF_ACT_TONF;
	meta->dst = destination;
	return 0;
}


int main(int argc, char *argv[]) {
	int arg_offset;

	const char *progname = argv[0];

	if ((arg_offset = d2sc_nfrt_init(argc, argv, NF_NAME, &nf_info, MAX_LOAD_SF)) < 0)
		return -1;
		
	argc -= arg_offset;
	argv += arg_offset;

	if (parse_app_args(argc, argv, progname) < 0) {
		d2sc_nfrt_stop(nf_info);
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
	}
	
	RTE_LOG(INFO, NF, "Core %d: Runnning initial thread\n", rte_lcore_id());
	d2sc_nfrt_run(nf_info, &packet_handler);
	
	printf("If we reach here, initial NF is ending\n");
	return 0;
}