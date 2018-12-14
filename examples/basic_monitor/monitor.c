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
#include <signal.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_cycles.h>

#include "d2sc_nfrt.h"
#include "d2sc_pkt_helper.h"

#define NF_NAME "basic_monitor"
//#define MAX_LOAD 500

/* Struct that contains information about this NF */
struct d2sc_nf_info *nf_info;
//struct d2sc_nf_info *scaled_nf_info;

// buffer used for NFs that handle TX. May not be used
//struct buf_queue *nf_bq;
//struct buf_queue *scaled_nf_bq;

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint32_t total_packets = 0;
static uint64_t last_cycle;
static uint64_t cur_cycles;

/* shared data structure containing host port info */
extern struct port_info *ports;

// True as long as the NF should keep processing packets
static uint8_t scaler_keep_running = 1;

/*
 * Print a usage message
 */
static void usage(const char *progname) {
	printf("Usage: %s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int parse_app_args(int argc, char *argv[], const char *progname) {
	int c;

	while ((c = getopt (argc, argv, "p:")) != -1) {
		switch (c) {
			case 'p':
				print_delay = strtoul(optarg, NULL, 10);
				RTE_LOG(INFO, NF, "print_delay = %d\n", print_delay);
				break;
			case '?':
				usage(progname);
				if (optopt == 'p')
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
	printf("Hash : %u\n", pkt->hash.rss);
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
callback_handler(__attribute__((unused)) struct d2sc_nf_info *nf_info) {
	cur_cycles = rte_get_tsc_cycles();

	if (((cur_cycles - last_cycle) / rte_get_timer_hz()) > 5) {
		printf("Total packets received: %" PRIu32 "\n", total_packets);
		last_cycle = cur_cycles;
	}

	return 0;
}

static int
packet_handler(struct rte_mbuf* pkt, struct d2sc_pkt_meta* meta, __attribute__((unused)) struct d2sc_nf_info *nf_info) {
	static uint32_t counter = 0;
	total_packets++;
	if (++counter == print_delay) {
		do_stats_display(pkt);
		counter = 0;
	}

	meta->act = D2SC_NF_ACT_OUT;
	meta->dst = pkt->port;

	if (d2sc_pkt_swap_src_mac_addr(pkt, meta->dst, ports) != 0) {
		RTE_LOG(INFO, NF, "ERROR: Failed to swap src mac with dst mac!\n");
	}
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
	
	cur_lcore = rte_lcore_id();
	
	if ((arg_offset = d2sc_nfrt_init(argc, argv, NF_NAME, &nf_info)) < 0)
		return -1;
	argc -= arg_offset;
	argv += arg_offset;
	
	if (parse_app_args(argc, argv, progname) < 0) {
		d2sc_nfrt_stop(nf_info);
		rte_exit(EXIT_FAILURE, "Invalid commandline arguments\n");
	}
	
	cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
	if (rte_eal_remote_launch(scaled_nf_thread, NULL, cur_lcore) == -EBUSY) {
		RTE_LOG(ERR, NF, "Core %d is already busy, cannot use for NF scaled thread", cur_lcore);
		return -1;
	}
	
	RTE_LOG(INFO, NF, "Core %d: Runnning initial thread\n", rte_lcore_id());
	
	cur_cycles = rte_get_tsc_cycles();
	last_cycle = rte_get_tsc_cycles();
	
	d2sc_nfrt_run_callback(nf_info, &packet_handler, &callback_handler);
	
	// Stop the scaling thread
	scaler_keep_running = 0;
	
	printf("If we reach here, initial NF is ending\n");
	
	return 0;
}