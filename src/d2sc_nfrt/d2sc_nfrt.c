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


/************************Standard C and DPDK library**************************/


#include <getopt.h>
#include <signal.h>

#include <rte_cycles.h>
#include <rte_power.h>

/*****************************Internal headers********************************/

#include "d2sc_nfrt.h"
#include "d2sc_includes.h"
#include "d2sc_sc.h"
#include "d2sc_mgr/d2sc_scale.h"


/**********************************Macros*************************************/


#define D2SC_NO_CALLBACK NULL
#define MAX_LOAD 500
#define MAX_CHECK_ITERS 105000000 // equal to 5s


/******************************Global Variables*******************************/

// Shared ports information
struct port_info *ports;

// ring used for NFs Sending msg to the manager
static struct rte_ring *new_msg_queue;

// ring sued for manager sending scale msg to nfs
static struct rte_ring *scale_msg_queue;


// buffer used for NFs that handle TX. May not be used
//extern struct buf_queue *nf_bq;
//extern struct buf_queue *scaled_nf_bq;

// Shared data from manger, through shared memzone. We update statistics here
struct d2sc_nf *nfs;

// Shared data from manager, has NF map information used for NF side TX
uint16_t **nts;
uint16_t *nfs_per_nt_num;
uint16_t *nfs_per_nt_available;

//// Shared data for NF info
//extern struct d2sc_nf_info *nf_info;
//extern struct d2sc_nf_info *scaled_nf_info;

// Shared mempool for all NFs info
static struct rte_mempool *nf_info_mp;

// Shared mempool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// User-given NF ID (if not given, default to manager assigned)
static uint16_t init_inst_id = NF_NO_ID;

// User supplied NF type ID
static uint16_t type_id = -1;

// True as long as the NF should keep processing packets
static uint8_t keep_running = 1;

// Shared data for default service chain
struct d2sc_sc *default_sc;


/***********************Internal Functions Prototypes*************************/

static int d2sc_nfrt_parse_args(int argc, char *argv[]);

static void d2sc_nfrt_usage(const char *progname);

static void d2sc_nfrt_handle_signal(int sig);

static struct d2sc_nf_info *d2sc_nfrt_info_init(const char *name);

static void d2sc_nfrt_nf_bq_init(struct d2sc_nf *nf);

static inline uint16_t 
d2sc_nfrt_dequeue_pkts(void **pkts, struct d2sc_nf_info *info, pkt_handler handler);

static inline void 
d2sc_nfrt_dequeue_new_msgs(struct d2sc_nf *nf);

static inline int d2sc_nfrt_nf_srv_time(struct d2sc_nf_info *info);

static int d2sc_nfrt_start_child(void *arg);

static void d2sc_nfrt_set_bk_flag(void);

/************************************API**************************************/

static int d2sc_nfrt_init_premises(int argc, char *argv[]) {
	const struct rte_memzone *mz_nf;
	const struct rte_memzone *mz_port;
	const struct rte_memzone *mz_scp;
	const struct rte_memzone *mz_nts;
	const struct rte_memzone *mz_nfs_per_nt;
	const struct rte_memzone *mz_nt_available;
	struct rte_mempool *mp_pkt;
	struct d2sc_sc **scp;
	
	int ret_eal;
	
	if ((ret_eal = rte_eal_init(argc, argv)) < 0)
		return -1;
	
	/* Lookup mempool for nf_info struct */
	nf_info_mp = rte_mempool_lookup(MP_NF_INFO_NAME);
	if (nf_info_mp == NULL)
		rte_exit(EXIT_FAILURE, "No NF info mempool found\n");
	
	/* Lookup mempool for NF messages */
	nf_msg_pool = rte_mempool_lookup(MP_NF_MSG_NAME);
	if (nf_msg_pool == NULL)
		rte_exit(EXIT_FAILURE, "No NF msg mempool found\n");
	
	mp_pkt = rte_mempool_lookup(MP_PKTMBUF_NAME);
	if (mp_pkt == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");
	
	/* Lookup memzone for NF structures*/
	mz_nf = rte_memzone_lookup(MZ_NF_INFO);
	if (mz_nf == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get NF structures memzone\n");
	nfs = mz_nf->addr;
	
	mz_nts = rte_memzone_lookup(MZ_NTS_INFO);
	if (mz_nts == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get NF type memzone\n");
	nts = mz_nts->addr;
	
	mz_nfs_per_nt = rte_memzone_lookup(MZ_NFS_PER_NT_INFO);
	if (mz_nfs_per_nt == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get NFs per NF type memzone\n");
	nfs_per_nt_num = mz_nfs_per_nt->addr;
		
	mz_nt_available = rte_memzone_lookup(MZ_NT_AVAILABLE_INFO);
	if (mz_nt_available == NULL)
		rte_exit(EXIT_FAILURE, "Cannt get NF type available memzone\n");
	nfs_per_nt_available = mz_nt_available->addr;
	
	mz_port = rte_memzone_lookup(MZ_PORT_INFO);	
	if (mz_port == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get port memzone\n");
	ports = mz_port->addr;
		
	mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
	if (mz_scp == NULL)
		rte_exit(EXIT_FAILURE, "Cannt get service chain pointer structure\n");
	scp = mz_scp->addr;
	default_sc = *scp;	
	
	d2sc_sc_print(default_sc);
	
	new_msg_queue = rte_ring_lookup(MGR_MSG_Q_NAME);
	if (new_msg_queue == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get new msg ring\n");
		
	scale_msg_queue = rte_ring_lookup(MGR_SCALE_Q_NAME);
	if (scale_msg_queue == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get scale msg ring\n");
		
	return ret_eal;
}

static int d2sc_nfrt_start_nf(struct d2sc_nf_info *nf_info) {
	struct d2sc_nf_msg *start_msg;
		
	/* Put this NF's info struct into nf msg pool for Manager to process startup */
	if (rte_mempool_get(nf_msg_pool, (void **)(&start_msg)) != 0) {
		rte_mempool_put(nf_info_mp, nf_info);	// give back memory to mempool obtained in NF init function
		rte_exit(EXIT_FAILURE, "Cannot create starup msg\n");
	}
	
	start_msg->msg_type = MSG_NF_STARTING;
	start_msg->msg_data = nf_info;
	
	if (rte_ring_enqueue(new_msg_queue, start_msg) < 0) {
		rte_mempool_put(nf_info_mp, nf_info);
		rte_mempool_put(nf_msg_pool, start_msg);
		rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager\n");
	}
	
	/* Wait for an NF ID to be assigned by the manager */
	RTE_LOG(INFO, NFRT, "Waiting for manager to assign an ID...\n");
	for (; nf_info->status == (uint16_t)NF_WAITTING_FOR_ID; ) {
		sleep(1);
	}
	
	/* This NF is trying to declare an ID already in use. */
	if (nf_info->status == NF_ID_CONFLICT) {
		rte_mempool_put(nf_info_mp, nf_info);
		rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
	} else if (nf_info->status == NF_NO_IDS) {
		rte_mempool_put(nf_info_mp, nf_info);
		rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
	} else if (nf_info->status != NF_STARTING) {
		rte_mempool_put(nf_info_mp, nf_info);
		rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
	}
	RTE_LOG(INFO, NFRT, "Using Instance ID %d\n", nf_info->inst_id);
	RTE_LOG(INFO, NFRT, "Using Type ID %d\n", nf_info->type_id);
	
	/* Initialize empty NF's buffer queue */
	d2sc_nfrt_nf_bq_init(&nfs[nf_info->inst_id]);
	
	/* Set the parent id to zero */
	nfs[nf_info->inst_id].parent_nf = 0;
		
	/* Tell the manger, this NF is ready to receive packets */
	keep_running = 1;
	
	RTE_LOG(INFO, NFRT, "Finished process init.\n");
	
	return 0;
}


int d2sc_nfrt_init(int argc, char *argv[], const char *nf_name, struct d2sc_nf_info **nf_info_p) {
	struct d2sc_nf_info *nf_info;
	int ret_eal, ret_parse, ret_final;
	
	/* Init the nfrt premises, including dpdk and shared memory init */
	ret_eal = d2sc_nfrt_init_premises(argc, argv);
	
	argc -= ret_eal;
	argv += ret_eal;
	
	/* Reset getopt global variables opterr and optind to their default values */
	opterr = 0; optind = 1;
	
	if ((ret_parse = d2sc_nfrt_parse_args(argc, argv)) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
		
	/*
	 * Calculate the offset that the nf will use to modify argc and argv for its
	 * getopt call. This is the sum of the number of arguments parsed by
	 * rte_eal_init and parse_nflib_args. This will be decremented by 1 to assure
	 * getopt is looking at the correct index since optind is incremented by 1 each
	 * time "--" is parsed.
	 * This is the value that will be returned if initialization succeeds.
	 */
	ret_final = (ret_eal + ret_parse) - 1;
	
	/* Reset getopt global variables opterr and optind to their default values */
	opterr = 0; optind = 1;
	
	/* Initialize the info struct */
	nf_info = d2sc_nfrt_info_init(nf_name);
	*nf_info_p = nf_info;
	
	d2sc_nfrt_start_nf(nf_info);
	
	return ret_final;
}


int d2sc_nfrt_run_callback(struct d2sc_nf_info *info, pkt_handler handler, 
cbk_handler callback) {
	struct rte_mbuf *pkts[PKT_RD_SIZE];
	struct d2sc_nf *nf;
	int ret;
	uint16_t nb_pkts, i;
	unsigned core;
	static uint32_t cur_freq;
	static uint64_t last_cycle;
	static uint64_t cur_cycles;
	
	static uint8_t srv_time_flag = 0;
	static uint64_t rx_idle_cnt = 0;
	
	nf = &nfs[info->inst_id];
	nf->handler = handler;
	nf->callback = callback;
	
	core = rte_lcore_id();
	cur_freq = rte_power_get_freq(core);
	printf("current frequency index is %lu\n", cur_freq);
	
	printf("\n NF %d handling packets\n", info->inst_id);
	
	/* Listen for ^C and docker stop so we can exit gracefully */
	signal(SIGINT, d2sc_nfrt_handle_signal);
	signal(SIGTERM, d2sc_nfrt_handle_signal);
	
	printf("Sending NF_READY message to manager...\n");
	ret = d2sc_nfrt_nf_ready(info);
	if (ret != 0) rte_exit(EXIT_FAILURE, "Unable to send ready message to manager\n");
		
	printf("[Press Ctrl-C to quit ...]\n");
	last_cycle = rte_get_tsc_cycles();
	for (; keep_running; ) {
		while (nf->bk_flag != 2) {
			// Set the frequency to normal
			if (rte_power_get_freq(core) != cur_freq) {
				rte_power_set_freq(core, cur_freq);
			}
			
			nb_pkts = d2sc_nfrt_dequeue_pkts((void **)pkts, info, handler);
			if (likely(nb_pkts > 0)) {
				d2sc_pkt_process_tx_batch(nf->nf_bq, pkts, nb_pkts, nf);
			} else {
				rx_idle_cnt++;
				if (rx_idle_cnt > MAX_CHECK_ITERS && nf->parent_nf != 0) {
					// block the child NF
					nf->bk_flag = 1;
					rx_idle_cnt = 0;
				}
			}
			
			/* Flush the packet buffers */
			d2sc_pkt_enqueue_tx_ring(nf->nf_bq->tx_buf, info->inst_id);
			d2sc_pkt_flush_all_bqs(nf->nf_bq);
		
			d2sc_nfrt_dequeue_new_msgs(nf);
		
			if (callback != D2SC_NO_CALLBACK) {
				keep_running = !(*callback)(info) && keep_running;
			}
			
			if (srv_time_flag == 0) {
				// Compute the service time 
				cur_cycles = rte_get_tsc_cycles();
				info->srv_time = (cur_cycles - last_cycle) * 1000000 / rte_get_timer_hz();
				//last_cycle = cur_cycles;
				printf("Computed service time = %u us\n", info->srv_time);
				printf("rte timer hz is %llu\n", rte_get_timer_hz());
				/* Send nf srv_time msg to manager */
				ret = d2sc_nfrt_nf_srv_time(info);
				if (ret != 0) {
					rte_exit(EXIT_FAILURE, "Unable to send nf service time msg to manager\n");
				}
				srv_time_flag = 1;
			}
			
			// Check scale messages when NF is running
			d2sc_nfrt_check_scale_msg(nf);
		}
		// Keep checking scale messages when NF is blocked
		d2sc_nfrt_check_scale_msg(nf);
		
		// Avoid too much CPU consumption when NF is blocked
		ret = rte_power_freq_min(core);
		if (ret <= 0) {
			RTE_LOG(INFO, NFRT, "Fail to change frequency of core %u in block status\n", core);
		} 
	}
	
	/* Wait for child nfs and other non-block nfs to quit */
	for (i = 0; i < MAX_NFS; i++) {
		while (nfs[i].nf_info != NULL && nfs[i].parent_nf == info->inst_id)
			sleep(1);
			
		while (nfs[i].nf_info != NULL && nfs[i].bk_flag != 2)
			sleep(1);
	}		
	
	// Stop the power management for the core running child NF
	if (nf->parent_nf == 0) {
		rte_power_exit(core);
		if (ret) 
			rte_exit(EXIT_FAILURE, "Power management exit failed on %u\n", core);
	} 

	// Stop and free
	d2sc_nfrt_stop(info);
	
	return 0;
}


int d2sc_nfrt_nf_ready(struct d2sc_nf_info *info) {
	struct d2sc_nf_msg *msg;
	int ret;
	
	/* Put this NF's info struct into msg queue for manager to process startup */
	ret = rte_mempool_get(nf_msg_pool, (void **)(&msg));
	if (ret != 0) return ret;
		
	msg->msg_type = MSG_NF_READY;
	msg->msg_data = info;
	ret = rte_ring_enqueue(new_msg_queue, msg);
	if (ret < 0) {
		rte_mempool_put(nf_msg_pool, msg);
		return ret;
	}
	return 0;
}


int d2sc_nfrt_handle_new_msg(struct d2sc_nf_msg *msg, struct d2sc_nf *nf) {
	switch (msg->msg_type) {
		case MSG_STOP:
			RTE_LOG(INFO, NFRT, "Shutting down...\n");
			nf->bk_flag = 2;
			keep_running = 0;
			break;
		case MSG_NOOP:
		default:
			break;
	}
	return 0;
}


void d2sc_nfrt_check_scale_msg(struct d2sc_nf *nf) {
	int i;
	struct d2sc_scale_msg *msg;	
	struct d2sc_scale_info *scale_info;
	struct rte_ring *scale_q;
	
	scale_q = nf->scale_q;
	
	if (likely(rte_ring_count(scale_q) == 0))
		return;

	msg = NULL;	
	rte_ring_dequeue(scale_q, (void**)(&msg));
		
	switch (msg->scale_sig) {
		case SCALE_UP:
			scale_info = (struct d2sc_scale_info *) msg->scale_data;
			printf("nf %u needs scale num %u\n", nf->inst_id, nf->scale_num);
			if(scale_info->inst_id == nf->inst_id && nf->scale_num != 0) {
				d2sc_nfrt_scale_nfs(nf->nf_info, scale_info->scale_num);
			}
			break;
		case SCALE_BLOCK:
			printf("nf %u get scale block message\n", nf->inst_id);
			scale_info = (struct d2sc_scale_info *)msg->scale_data;
			if (scale_info->inst_id == nf->inst_id && nf->bk_flag == 1) {
				d2sc_nfrt_scale_block(nf->nf_info);
			}
			break;
		case SCALE_RUN:
			printf("nf %u get scale run message\n", nf->inst_id);
			scale_info = (struct d2sc_scale_info *)msg->scale_data;
			if (scale_info->inst_id == nf->inst_id && nf->bk_flag == 2) {
				d2sc_nfrt_scale_run(nf->nf_info);
			}
			break;
		case SCALE_NO:
		default:
			break;		
	}		
	rte_mempool_put(nf_msg_pool, (void *)msg);
}


void d2sc_nfrt_stop(struct d2sc_nf_info *info) {
	struct d2sc_nf_msg *stop_msg;
	info->status = NF_STOPPED;
	
	/* Put this NF's info struct back into the queue for manager to ack stop */
	if (new_msg_queue == NULL) {
		rte_mempool_put(nf_info_mp, info);	//give back memory
		rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for stopping");
	}
	if (rte_mempool_get(nf_msg_pool, (void **)(&stop_msg)) != 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_exit(EXIT_FAILURE, "Cannot create stop msg");
	}
	
	stop_msg->msg_type = MSG_NF_STOPPING;
	stop_msg->msg_data = info;
	
	if (rte_ring_enqueue(new_msg_queue, stop_msg) < 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_mempool_put(nf_msg_pool, stop_msg);
		rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager for stopping");
	}
}

void d2sc_nfrt_scale_block(struct d2sc_nf_info *info) {
	struct d2sc_nf_msg *block_msg;
	info->status = NF_BLOCKED;
	
	if (new_msg_queue == NULL) {
		rte_mempool_put(nf_info_mp, info);
		rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for scale blocking");
	}
	if (rte_mempool_get(nf_msg_pool, (void **)(&block_msg)) != 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_exit(EXIT_FAILURE, "Cannot create scale block msg");
	}
	
	block_msg->msg_type = MSG_NF_BLOCKING;
	block_msg->msg_data = info;
	
	if (rte_ring_enqueue(new_msg_queue, block_msg) < 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_mempool_put(nf_msg_pool, block_msg);
		rte_exit(EXIT_FAILURE, "Cannot send nf_info to the manager for scale blocking");
	}
		
}

void d2sc_nfrt_scale_run(struct d2sc_nf_info *info) {
	struct d2sc_nf_msg *run_msg;
	
	if (info->status != NF_BLOCKED)
		return;
	
	info->status = NF_RUNNING;
	
	if (new_msg_queue == NULL) {
		rte_mempool_put(nf_info_mp, info);
		rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for scale running");
	}
	if (rte_mempool_get(nf_msg_pool, (void **)(&run_msg)) != 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_exit(EXIT_FAILURE, "Cannot create scale run msg");
	}
	
	run_msg->msg_type = MSG_NF_RUNNING;
	run_msg->msg_data = info;
	
	if (rte_ring_enqueue(new_msg_queue, run_msg) < 0) {
		rte_mempool_put(nf_info_mp, info);
		rte_mempool_put(nf_msg_pool, run_msg);
		rte_exit(EXIT_FAILURE, "Cannot send nf_info to the manager for scale running");
	}
		
}


int d2sc_nfrt_run(struct d2sc_nf_info *info,
	int(*pkt_handler)(struct rte_mbuf *pkt, struct d2sc_pkt_meta *act, __attribute__((unused)) struct d2sc_nf_info *nf_info)) {
	return d2sc_nfrt_run_callback(info, pkt_handler, D2SC_NO_CALLBACK);
}


int d2sc_nfrt_ret_pkt(struct rte_mbuf *pkt, struct d2sc_nf_info *info) {
	if (unlikely(rte_ring_enqueue(nfs[info->inst_id].tx_q, pkt) == -ENOBUFS)) {
		rte_pktmbuf_free(pkt);
		nfs[info->inst_id].stats.tx_drop++;
		return -ENOBUFS;
	}
	else nfs[info->inst_id].stats.tx_ret++;
	return 0;
}


struct d2sc_sc *d2sc_nfrt_get_default_sc(void) {
	return default_sc;
}


int d2sc_nfrt_scale_nfs(struct d2sc_nf_info *nf_info, uint16_t num_nfs) {
	unsigned cur_lcore, nfs_lcore;
	unsigned core;
	uint16_t i;
	enum rte_lcore_state_t state;
	int ret;
	uint16_t d_nfs = num_nfs;
	
	cur_lcore = rte_lcore_id();
	nfs_lcore = rte_lcore_count() - 1;
	
	RTE_LOG(INFO, NFRT, "Currently running on core %u\n", cur_lcore);
	for (core = 0, i = 0; core < nfs_lcore && i < num_nfs; core++, i++ ) {
		/* Find the next available lcore to use */
		cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
		state = rte_eal_get_lcore_state(cur_lcore);
		if (state != RUNNING) {
			// init power management lib for the core
			ret = rte_power_init(cur_lcore);
			if (ret) {
				rte_exit(EXIT_FAILURE, "Power management library initialization failed on core %d\n", cur_lcore);
			}
			ret = rte_eal_remote_launch(&d2sc_nfrt_start_child, nf_info, cur_lcore);
			if (ret == -EBUSY) {
				RTE_LOG(INFO, NFRT, "Core %u is busy, skipping...\n", core);
				continue;
			}
			d_nfs--;
		}
	}
	if (d_nfs == 0) {
		RTE_LOG(INFO, NFRT, "All child NFs are to scale successfully\n");
		return 0;
	} else {
		RTE_LOG(INFO, NFRT, "No cores available to scale, remaining %u child NFs\n", d_nfs);
		return -1;
	}
}


/******************************Internal Functions*********************************/


static int d2sc_nfrt_parse_args(int argc, char *argv[]) {
	const char *progname = argv[0];
	int c;

	opterr = 0;
	while ((c = getopt (argc, argv, "n:t:")) != -1)
		switch (c) {
			case 'n':
				init_inst_id = (uint16_t) strtoul(optarg, NULL, 10);
				break;
			case 't':
				type_id = (uint16_t) strtoul(optarg, NULL, 10);
				// NF type id 0 is reserved
				if (type_id == 0) type_id = -1;
				break;
			case '?':
				d2sc_nfrt_usage(progname);
				if (optopt == 'n')
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return -1;
			default:
				return -1;
		}

		if (type_id == (uint16_t)-1) {
			/* NF type ID is required */
			fprintf(stderr, "You must provide a nonzero type ID with -t\n");
			return -1;
		}
		return optind;
}

static void d2sc_nfrt_usage(const char *progname) {
	printf("Usage: %s [EAL args] -- "
		"[-n <instance_id>]"
		"[-t <type_id>]\n\n", progname);
}

static void d2sc_nfrt_handle_signal(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		d2sc_nfrt_set_bk_flag();
		keep_running = 0;
	}
}

static struct d2sc_nf_info *d2sc_nfrt_info_init(const char *name) {
	void *mp_data;
	struct d2sc_nf_info *info;
	
	if (rte_mempool_get(nf_info_mp, &mp_data) < 0)
		rte_exit(EXIT_FAILURE, "Failed to get NF info mempool");
		
	if (mp_data == NULL)
		rte_exit(EXIT_FAILURE, "NF info struct is not allocated");
		
	info = (struct d2sc_nf_info *)mp_data;
	info->inst_id = init_inst_id;
	info->type_id = type_id;
	info->status = NF_WAITTING_FOR_ID;
	info->max_load = MAX_LOAD;
	info->name = name;
	return info;
}

static void d2sc_nfrt_nf_bq_init(struct d2sc_nf *nf) {
	nf->nf_bq = calloc(1, sizeof(struct buf_queue));
	nf->nf_bq->mgr_nf = 0;
	nf->nf_bq->tx_buf = calloc(1, sizeof(struct pkt_buf));
	nf->nf_bq->id = init_inst_id;
	nf->nf_bq->rx_bufs = calloc(MAX_NFS, sizeof(struct pkt_buf));
}

static inline uint16_t 
d2sc_nfrt_dequeue_pkts(void **pkts, struct d2sc_nf_info *info, pkt_handler handler) {
	struct d2sc_pkt_meta *meta;
	uint16_t i, nb_pkts;
	struct pkt_buf tx_buf;
	int ret_act;
	
	/* Dequeue all packets in ring up to max possible */
	nb_pkts = rte_ring_dequeue_burst(nfs[info->inst_id].rx_q, pkts, PKT_RD_SIZE, NULL);
	if (unlikely(nb_pkts == 0)) 
		return 0;
		
	tx_buf.cnt = 0;
	
	/* Given each packet to the user processing function */
	for (i = 0; i < nb_pkts; i++) {
		meta = d2sc_get_pkt_meta((struct rte_mbuf *)pkts[i]);
		ret_act = (*handler)((struct rte_mbuf *)pkts[i], meta, info);
		/* NF returns 0 to give back packets, returns 1 to TX buffer */
		if (likely(ret_act == 0)) {
			tx_buf.buf[tx_buf.cnt++] = pkts[i];
		} else {
			nfs[info->inst_id].stats.tx_buf++;
		}
	}
	if (D2SC_NF_HANDLE_TX) {
		return nb_pkts;
	}
	
	d2sc_pkt_enqueue_tx_ring(&tx_buf, info->inst_id);
	return 0;
}

static inline void 
d2sc_nfrt_dequeue_new_msgs(struct d2sc_nf *nf) {
	struct d2sc_nf_msg *msg;
	struct rte_ring *msg_ring;
	
	msg_ring = nf->msg_q;
	
	/* Check whether this NF has any msgs from the manager */
	if (likely(rte_ring_count(msg_ring) == 0)) {
		return;
	}
	msg = NULL;
	rte_ring_dequeue(msg_ring, (void **)(&msg));
	d2sc_nfrt_handle_new_msg(msg, nf);
	rte_mempool_put(nf_msg_pool, (void *)msg);
}

static inline int d2sc_nfrt_nf_srv_time(struct d2sc_nf_info *info) {
	struct d2sc_nf_msg *srv_time_msg;
	int ret;
	
	ret = rte_mempool_get(nf_msg_pool, (void **)(&srv_time_msg));
	if (ret != 0) return ret;
	
	srv_time_msg->msg_type = MSG_NF_SRV_TIME;
	srv_time_msg->msg_data = info;
	ret = rte_ring_enqueue(new_msg_queue, srv_time_msg);
	if (ret < 0) {
		rte_mempool_put(nf_msg_pool, srv_time_msg);
		return ret;
	}
	return 0;
}

static int d2sc_nfrt_start_child(void *arg) {
	struct d2sc_nf *parent_nf;
	struct d2sc_nf *child_nf;
	struct d2sc_nf_info *child_info;
	struct d2sc_nf_info *nf_info = (struct d2sc_nf_info *) arg;
	
	parent_nf = &nfs[nf_info->inst_id];
	parent_nf->scale_num--;
	child_info = d2sc_nfrt_info_init(nf_info->name);
	// Child NF inherits service time
	child_info->srv_time = nf_info->srv_time;
	
	d2sc_nfrt_start_nf(child_info);
	child_nf = &nfs[child_info->inst_id];
	child_nf->parent_nf = parent_nf->inst_id;
	// Child NF inherits specific function from parent NF
	child_nf->handler = parent_nf->handler;
	child_nf->callback = parent_nf->callback;
	
	RTE_LOG(INFO, NF, "Core %d: Runnning child NF %u thread\n", rte_lcore_id(), child_info->inst_id);
	d2sc_nfrt_run_callback(child_info, child_nf->handler, child_nf->callback);
	
	printf("If we reach, child NF %u is ending\n", child_info->inst_id);
	return 0;
}

static void d2sc_nfrt_set_bk_flag(void) {
	uint16_t i;
	
	for (i = 0; i < MAX_NFS; i++) {
		if (nfs[i].nf_info == NULL)
			continue;
			
		nfs[i].bk_flag = 2;
	}
}

