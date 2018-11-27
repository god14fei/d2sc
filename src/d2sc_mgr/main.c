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
                                   main.c
     File containing the main function of the manager and all its worker
     threads.

******************************************************************************/

#include <signal.h>

#include "d2sc_mgr.h"
#include "d2sc_stats.h"
#include "d2sc_nfrt/d2sc_pkt_process.h"
#include "d2sc_nf.h"


/****************************Internal Declarations****************************/

#define MAX_SHUTDOWN_ITERS 10

/* True as long as the main thread loop should keep running */
static uint8_t main_keep_running = 1;

/* Shut down the TX/RX threads second so that the stats display can print, so 
 * use a separate vaviable
 */
static uint8_t worker_keep_running = 1;

static void handle_signal (int sig);

/*******************************Worker threads********************************/

/*
 * RX to receive packets from the NIC and distribute them to the default nf
 */
static int rx_thread_main(void *arg) {
	uint16_t i, nb_rx;
	struct rte_mbuf *rx_pkts[PKT_RD_SIZE];
	struct buf_queue *mgr_bq = (struct buf_queue *)arg;
	
	RTE_LOG(INFO, MGR, "Core %d: Running RX thread for RX queue %d\n", 
		rte_lcore_id(), mgr_bq->id);
		
	for (; worker_keep_running; ) {
		/* Read ports */
		for (i = 0; i < ports->n_ports; i++) {
			nb_rx = rte_eth_rx_burst(ports->id[i], mgr_bq->id, rx_pkts, PKT_RD_SIZE);
			ports->rx_stats.rx[ports->id[i]] += nb_rx;
			
			/* Process the NIC packets read */
			if (likely(nb_rx > 0)) {
				if (!num_nfs) {
					d2sc_pkt_drop_batch(rx_pkts, nb_rx);
				} else {
					d2sc_pkt_process_rx_batch(mgr_bq, rx_pkts, nb_rx);
				}
			}
		}
	}
	
	RTE_LOG(INFO, MGR, "Core %d: RX thread done!\n", rte_lcore_id());
	
	return 0;
}


static int tx_thread_main(void *arg) {
	struct d2sc_nf *nf;
	uint16_t i, nb_tx;
	struct rte_mbuf *tx_pkts[PKT_RD_SIZE];
	struct buf_queue *mgr_bq = (struct buf_queue *)arg;
	
	if (mgr_bq->tx_thread->first_nf_id == mgr_bq->tx_thread->last_nf_id - 1) {
		RTE_LOG(INFO, MGR, "Core %d: Running TX thread for NF %d\n",
			rte_lcore_id(), mgr_bq->tx_thread->first_nf_id);
	} else if (mgr_bq->tx_thread->first_nf_id < mgr_bq->tx_thread->last_nf_id) {
		RTE_LOG(INFO, MGR, "Core %d: Running TX thread for NFs %d to %d\n",
			rte_lcore_id(), mgr_bq->tx_thread->first_nf_id, mgr_bq->tx_thread->last_nf_id-1);		
	}
	
	for (; worker_keep_running;) {
		/* Read packets from the NF's tx queue and process them as needed */
		for (i = mgr_bq->tx_thread->first_nf_id; i < mgr_bq->tx_thread->last_nf_id; i++) {
			nf = &nfs[i];
			if (!d2sc_nf_is_valid(nf))
				continue;
			
			/* Dequeue all packets in ring up to max possible */
			nb_tx = rte_ring_dequeue_burst(nf->tx_q, (void **) tx_pkts, PKT_RD_SIZE, NULL);
			
			/* Now process the packets read from NF */
			if (likely(nb_tx > 0)) {
				d2sc_pkt_process_tx_batch(mgr_bq, tx_pkts, nb_tx, nf);
			}
		}
		
		/* Send a burst to every port */
		d2sc_pkt_flush_all_ports(mgr_bq);
		
		/* Send a burst to every NF */
		d2sc_pkt_flush_all_bqs(mgr_bq);
	}
	
	RTE_LOG(INFO, MGR, "Core %d: TX thread done!\n", rte_lcore_id());
	
	return 0;
	
}


/*
 * Scale thread periodically monitor whether an NF type needs to perform scaling.
 */
static int scale_thread_main(void *arg) {
	uint16_t i;
	const unsigned scale_iter = SCALE_SLEEP_TIME;
	
	RTE_LOG(INFO, MGR, "Core %d: Running scale thread\n", rte_lcore_id());
	
	for (; worker_keep_running && sleep(scale_iter) <= scale_iter;) {
		for (i = 0; i < num_nts; i++) {
			if (nfs_per_nt_num[i] == 0)
				continue;
				
			if (nfs_per_nt_available[i] == 0) {
				d2sc_scale_check_block(i);
			}
		}
		
		d2sc_scale_check_overload();
		d2sc_scale_up_signal();
		if (up_signal == 1) {
			for (i = 0; i < num_nts; i++) {
				if (nfs_per_nt_num[i] == 0)
					continue;
				
				if (nfs_per_nt_available[i] ==0) {	
					RTE_LOG(INFO, MGR, "Core %d: Notifying NF type %"PRIu16" to scale up\n", rte_lcore_id(), i);
					d2sc_scale_up_execute(i);
				}
			}
		}
		
		// Check the NF block signal
		d2sc_scale_block_signal();
		for (i = 0; i < MAX_NFS; i++) {
			if (!d2sc_nf_is_valid(&nfs[i])) {
				continue;
			}
			
			if (nfs[i].bk_flag == 1) {
				RTE_LOG(INFO, MGR, "Core %d: Notifying NF %"PRIu16" to scale block\n", rte_lcore_id(), i);
				d2sc_scale_block_execute(i, SCALE_BLOCK);
			}
		}
	}
	
	RTE_LOG(INFO, MGR, "Core %d: Scale thread done\n", rte_lcore_id());	
		
	return 0;
}


/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static void master_thread_main(void) {
	uint16_t i;
	int shutdown_iter_cnt;
	const unsigned sleep_time = global_stats_sleep_time;
	
	RTE_LOG(INFO, MGR, "Core %d: Runnning master thread\n", rte_lcore_id());
	
	/* Initial pause so above print is seen */
	sleep(2);
	
	/* Loop forever if main_keep_running == 1, since sleep always return 0 or <= sleep_time */
	while (main_keep_running && sleep(sleep_time) <= sleep_time) {
		d2sc_nf_check_status();
		if (stats_dst != D2SC_STATS_NONE)
			d2sc_stats_display_all(sleep_time);
	}
	
	RTE_LOG(INFO, MGR, "Core %d: Initiating shutdown sequence\n", rte_lcore_id());
	
	/* Stop aall RX and TX threads */
	worker_keep_running = 0;
	
	/* Tell all NFs to stop */
	for (i = 0; i < MAX_NFS; i++) {
		if (nfs[i].nf_info == NULL) {
			continue;
		}
		RTE_LOG(INFO, MGR, "Core %d: Notifying NF %"PRIu16" to shut down\n", rte_lcore_id(), i);
		d2sc_nf_send_msg(i, MSG_STOP, NULL); 
	}
	
	/* Wait to process all exits */
	for (shutdown_iter_cnt = 0; shutdown_iter_cnt < MAX_SHUTDOWN_ITERS && num_nfs > 0; shutdown_iter_cnt++) {
		d2sc_nf_check_status();
		RTE_LOG(INFO, MGR, "Core %d: Waiting for %"PRIu16" NFs to exit\n", rte_lcore_id(), num_nfs);
		sleep(sleep_time);
	}
	
	if (num_nfs > 0) {
		RTE_LOG(INFO, MGR, "Core %d: Up to %"PRIu16" NFs may still running and must be killed manually\n", 
		rte_lcore_id(), num_nfs);
	}
	
	RTE_LOG(INFO, MGR, "Core %d: Master thread done\n", rte_lcore_id());	
}


static void handle_signal(int sig) {
	if (sig == SIGINT || sig == SIGTERM) {
		main_keep_running = 0;
	}
}


/*******************************Main function*********************************/

int main(int argc, char *argv[]) {
	unsigned cur_lcore, rx_lcores, tx_lcores;
	unsigned nfs_per_tx;
	unsigned i;
	
	next_inst_id = 1;		/* Reserve ID 0 for internal manager things */
	
	if (init(argc, argv) < 0)
		return -1;
	RTE_LOG(INFO, MGR, "Process Init Finished.\n");
	
	/* clear statistics */
	d2sc_stats_clear_all_nfs();
	
	/* Reserve n cores for: 1 stats, 1 final TX out, 1 scaling, and num_tx_threads for RX */
	cur_lcore = rte_lcore_id();
	rx_lcores = num_rx_threads;
	tx_lcores = rte_lcore_count() - rx_lcores - 2;
	
	/* Offset cur_lcore to start assigning TX cores */
	cur_lcore += (rx_lcores - 1);
	
	RTE_LOG(INFO, MGR, "%d cores available in total\n", rte_lcore_count());
	RTE_LOG(INFO, MGR, "%d cores available for handling manager RX queues\n", rx_lcores);
	RTE_LOG(INFO, MGR, "%d cores available for handling TX queues\n", tx_lcores);
	RTE_LOG(INFO, MGR, "%d cores available for handling stats\n", 1);
	
	/* Evenly assign NFs to TX threads */
	
	/* 
	 *
	 */
	nfs_per_tx = ceil((float)MAX_NFS/tx_lcores);
	
	/* We start the system with 0 NFs active */
	num_nfs = 0;
	
	/* Listen for ^C and docker stop so we can exit gracefully */
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	
	for (i = 0; i < tx_lcores; i++) {
		struct buf_queue *mgr_bq = calloc(1, sizeof(struct buf_queue));
		mgr_bq->id = i;
		mgr_bq->mgr_nf = 1;
		mgr_bq->tx_thread = calloc(1, sizeof(struct tx_thread));
		mgr_bq->tx_thread->tx_bufs = calloc(RTE_MAX_ETHPORTS, sizeof(struct pkt_buf));
		mgr_bq->tx_thread->first_nf_id = RTE_MIN(i * nfs_per_tx + 1, (unsigned)MAX_NFS);
		mgr_bq->tx_thread->last_nf_id = RTE_MIN((i+1) * nfs_per_tx + 1, (unsigned)MAX_NFS);
		mgr_bq->rx_bufs = calloc(MAX_NFS, sizeof(struct pkt_buf));
		cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
		if (rte_eal_remote_launch(tx_thread_main, (void *)mgr_bq, cur_lcore) == -EBUSY) {
			RTE_LOG(ERR, MGR, "Core %d is already busy, cannot use for NF %d TX\n", 
					cur_lcore, mgr_bq->tx_thread->first_nf_id);
			return -1;
		}
	}
	
	/* Launch RX thread main function for each RX queue on cores */
	for (i = 0; i < rx_lcores; i++) {
		struct buf_queue *mgr_bq = calloc(1, sizeof(struct buf_queue));
		mgr_bq->id = i;
		mgr_bq->mgr_nf = 1;
		mgr_bq->tx_thread = NULL;
		mgr_bq->rx_bufs = calloc(MAX_NFS, sizeof(struct pkt_buf));
		cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
		if (rte_eal_remote_launch(rx_thread_main, (void *)mgr_bq, cur_lcore) == -EBUSY) {
			RTE_LOG(ERR, MGR, "Core %d is already busy, cannot use for RX queue id %d\n", 
					cur_lcore, mgr_bq->id);
			return -1;
		}
	}
	
	cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
	if (rte_eal_remote_launch(scale_thread_main, NULL, cur_lcore) == -EBUSY) {
		RTE_LOG(ERR, MGR, "Core %d is already busy, cannot use for NF scaling", cur_lcore);
		return -1;
	}
	
	/* Master thread handles statistics and NF management */
	master_thread_main();
		
	return 0;	
}