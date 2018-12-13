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
                                   d2sc_stats.c
      C file containing all function implementations related to statistis
      display in the manager.

******************************************************************************/

#include <unistd.h>
#include <time.h>
#include <stdio.h>

#include "d2sc_mgr.h"
#include "d2sc_stats.h"
#include "d2sc_nf.h"


/************************Internal Functions Prototypes************************/

static void d2sc_stats_display_ports(unsigned stime);

static void d2sc_stats_display_nfs(unsigned stime);

static void d2sc_stats_clear_terminal(void);

static void d2sc_stats_truncate(void);

static const char *d2sc_stats_print_MAC(uint8_t port);

static void d2sc_stats_flush(void);


/*********************Stats Output Streams************************************/

static FILE *stats_out;


/****************************Interfaces***************************************/

void d2sc_stats_set_output(D2SC_STATS_OUTPUT stats_dst) {
	if (stats_dst != D2SC_STATS_NONE) {
		switch (stats_dst) {
			case D2SC_STATS_STDOUT:
				stats_out = stdout;
				break;
			case D2SC_STATS_STDERR:
				stats_out = stderr;
				break;
			default:
				rte_exit(-1, "Error handling stats output file\n");
				break;
		}
	}
}


void d2sc_stats_clear_all_nfs(void) {
	unsigned i;
	
	for (i=0; i < MAX_NFS; i++) {
		nfs[i].stats.rx = 0;
		nfs[i].stats.rx_drop = 0;
		nfs[i].stats.act_drop = 0;
		nfs[i].stats.act_tonf = 0;
		nfs[i].stats.act_next = 0;
		nfs[i].stats.act_out = 0;
	}
}

void d2sc_stats_clear_nf(uint16_t id) {
		nfs[id].stats.rx = 0;
		nfs[id].stats.rx_drop = 0;
		nfs[id].stats.act_drop = 0;
		nfs[id].stats.act_tonf = 0;
		nfs[id].stats.act_next = 0;
		nfs[id].stats.act_out = 0;
}

void d2sc_stats_display_all(unsigned stime) {
	if (stats_out == stdout) {
		d2sc_stats_clear_terminal();
	} else {
		d2sc_stats_truncate();
	}
	
	d2sc_stats_display_ports(stime);
	d2sc_stats_display_nfs(stime);
	
	d2sc_stats_flush();
}

/********************************Internal Functions***************************/

static void d2sc_stats_display_ports(unsigned stime) {
	unsigned i = 0;
	uint64_t rx_pkts = 0;
	uint64_t tx_pkts = 0;
	uint64_t rx_pps = 0;
	uint64_t tx_pps = 0;
	
	/* Arrays to store last TX/RX pakcet number to calculate rate */
	static uint64_t rx_last[RTE_MAX_ETHPORTS];
	static uint64_t tx_last[RTE_MAX_ETHPORTS];
	
	fprintf(stats_out, "PORTS\n");
	fprintf(stats_out, "-----\n");
	for (i = 0; i < ports->n_ports; i++) {
		fprintf(stats_out, "Port %u: '%s'\t", (unsigned)ports->id[i], d2sc_stats_print_MAC(ports->id[i]));
	}
	fprintf(stats_out, "\n\n");
	for (i = 0; i < ports->n_ports; i++) {
		rx_pkts = ports->rx_stats.rx[ports->id[i]];
		tx_pkts = ports->tx_stats.tx[ports->id[i]];
		
		rx_pps = (rx_pkts - rx_last[i]) / stime;
		tx_pps = (tx_pkts - tx_last[i]) / stime;
		
		fprintf(stats_out, "Port %u - rx: %13"PRIu64"  (%13"PRIu64" pps)\t"
		"tx: %13"PRIu64"  (%13"PRIu64" pps)\n", (unsigned)ports->id[i], rx_pkts, rx_pps, tx_pkts, tx_pps);
		
		rx_last[i] = rx_pkts;
		tx_last[i] = tx_pkts;
	}
}

static void d2sc_stats_display_nfs(unsigned stime) {
	unsigned i = 0;
	
	/* Arrays to store last RX/TX pakcet number for NFs to calculate packet rate */
	static uint64_t rx_last[MAX_NFS];
	static uint64_t tx_last[MAX_NFS];
	
	/* Arrays to store last RX/TX pkts drop for NFs to calculate drop rate */
	static uint64_t rx_drop_last[MAX_NFS];
	static uint64_t tx_drop_last[MAX_NFS];
	
	fprintf(stats_out, "\nNFS\n");
	fprintf(stats_out, "------\n");
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
			
		const uint64_t rx = nfs[i].stats.rx;
		const uint64_t rx_drop = nfs[i].stats.rx_drop;
		const uint64_t tx = nfs[i].stats.tx;
		const uint64_t tx_drop = nfs[i].stats.tx_drop;
		const uint64_t act_drop = nfs[i].stats.act_drop;
		const uint64_t act_next = nfs[i].stats.act_next;
		const uint64_t act_tonf = nfs[i].stats.act_tonf;
		const uint64_t act_out = nfs[i].stats.act_out;
		const uint64_t tx_buf = nfs[i].stats.tx_buf;
		const uint64_t tx_ret = nfs[i].stats.tx_ret;
		
		const uint64_t rx_pps = (rx - rx_last[i]) / stime;
		const uint64_t tx_pps = (tx - tx_last[i]) / stime;
		const uint64_t rx_drop_rate = (rx_drop - rx_drop_last[i]) / stime;
		const uint64_t tx_drop_rate = (tx_drop - tx_drop_last[i]) / stime;
		
		fprintf(stats_out, "NF %2u - rx: %13"PRIu64" rx_drop: %13"PRIu64" next: %13"PRIu64" drop: %13"PRIu64" ret: %13"PRIu64"\n"
			"	tx: %13"PRIu64" tx_drop: %13"PRIu64" out:  %13"PRIu64" tonf: %13"PRIu64" buf: %13"PRIu64" \n"
			" \trx_pps: %9"PRIu64" rx_drop_rate: %8"PRIu64" tx_pps:   %9"PRIu64" tx_drop_rate:  %9"PRIu64"\n",
			nfs[i].nf_info->inst_id, rx, rx_drop, act_next, act_drop, tx_ret,
			tx, tx_drop, act_out, act_tonf, tx_buf,
			rx_pps, rx_drop_rate, tx_pps, tx_drop_rate);
			
		rx_last[i] = rx;
		tx_last[i] = tx;
		rx_drop_last[i] = rx_drop;
		tx_drop_last[i] = tx_drop;
		
		nfs[i].pkt_rate = ceil(rx_pps / 100000);	// Reserve the instantaneous packet rate (0.1Mpps) in order to compute the load	 
	}
	
	fprintf(stats_out, "\n");
}


/***************************Helper functions**********************************/


static void d2sc_stats_clear_terminal(void) {
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	fprintf(stats_out, "%s%s", clr, topLeft);
}

static void d2sc_stats_truncate(void) {
	if (stats_out == stdout || stats_out == stderr) {
		return;
	}
}

static const char *d2sc_stats_print_MAC(uint8_t port) {
	static const char err_address[] = "00:00:00:00:00:00";
	static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

	if (unlikely(port >= RTE_MAX_ETHPORTS))
		return err_address;

	if (unlikely(addresses[port][0] == '\0')) {
		struct ether_addr mac;
		rte_eth_macaddr_get(port, &mac);
		snprintf(addresses[port], sizeof(addresses[port]),
			"%02x:%02x:%02x:%02x:%02x:%02x\n",
			mac.addr_bytes[0], mac.addr_bytes[1],
			mac.addr_bytes[2], mac.addr_bytes[3],
			mac.addr_bytes[4], mac.addr_bytes[5]);
	}

	return addresses[port];
}

static void d2sc_stats_flush(void) {
	if (stats_out == stdout || stats_out == stderr) {
		return;
	}
	
	fflush(stats_out);
}
