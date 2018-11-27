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

/*****************************Internal headers********************************/

#include "d2sc_nfrt.h"
#include "d2sc_includes.h"
#include "d2sc_sc.h"
#include "d2sc_mgr/d2sc_scale.h"


/**********************************Macros*************************************/


#define D2SC_NO_CALLBACK NULL
#define MAX_LOAD 500

typedef int(*pkt_handler)(struct rte_mbuf *pkt, struct d2sc_pkt_meta *meta);
typedef int(*callback_handler)(void);


/******************************Global Variables*******************************/

// Shared ports information
struct port_info *ports;

// ring used for NFs Sending msg to the manager
static struct rte_ring *new_msg_queue;

// ring sued for manager sending scale msg to nfs
static struct rte_ring *scale_msg_queue;

// rings used to pass pkts between NFRT and Manager
//static struct rte_ring *tx_ring, *rx_ring;
//static struct rte_ring *scaled_tx_ring, *scaled_rx_ring;

// ring used for manager sending msg to NFs
//static struct rte_ring *nf_msg_ring;
//static struct rte_ring *scaled_nf_msg_ring;

// buffer used for NFs that handle TX. May not be used
extern struct buf_queue *nf_bq;
extern struct buf_queue *scaled_nf_bq;

// Shared data from manger, through shared memzone. We update statistics here
struct d2sc_nf *nfs;

// Shared data from manager, has NF map information used for NF side TX
uint16_t **nts;
uint16_t *nfs_per_nt_num;
uint16_t *nfs_pet_nt_available;

// Shared data for NF info
extern struct d2sc_nf_info *nf_info;
extern struct d2sc_nf_info *scaled_nf_info;

// Shared mempool for all NFs info
static struct rte_mempool *nf_info_mp;

// Shared mempool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// User-given NF ID (if not given, default to manager assigned)
static uint16_t init_inst_id = NF_NO_ID;

// User supplied NF type ID
static uint16_t type_id = -1;

// True as long as the NF should keep processing packets
extern uint8_t keep_running;

// True as long as the NF is not blocked
static uint8_t non_blocking = 1;

// Shared data for default service chain
struct d2sc_sc *default_sc;


/***********************Internal Functions Prototypes*************************/

static int d2sc_nfrt_parse_args(int argc, char *argv[]);

static void d2sc_nfrt_usage(const char *progname);

//static void d2sc_nfrt_handle_signal(int sig);

static struct d2sc_nf_info *d2sc_nfrt_info_init(const char *name);

static void d2sc_nfrt_nf_bq_init(struct buf_queue *bq);

static inline uint16_t 
d2sc_nfrt_dequeue_pkts(void **pkts, struct d2sc_nf_info *info, pkt_handler handler);

static inline void 
d2sc_nfrt_dequeue_new_msgs(struct rte_ring *msg_ring);

static inline int d2sc_nfrt_nf_srv_time(struct d2sc_nf_info *info);


/************************************API**************************************/

int d2sc_nfrt_init(int argc, char *argv[], const char *nf_name) {
	const struct rte_memzone *mz_nf;
	const struct rte_memzone *mz_port;
	const struct rte_memzone *mz_scp;
	const struct rte_memzone *mz_nts;
	const struct rte_memzone *mz_nfs_per_nt;
	const struct rte_memzone *mz_nt_available;
	struct rte_mempool *mp_pkt;
	struct d2sc_sc **scp;
	struct d2sc_nf_msg *start_msg;
	int ret_eal, ret_parse, ret_final;
	
	if ((ret_eal = rte_eal_init(argc, argv)) < 0)
		return -1;
		
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
	
	/* Lookup mempool for nf_info struct */
	nf_info_mp = rte_mempool_lookup(MP_NF_INFO_NAME);
	if (nf_info_mp == NULL)
		rte_exit(EXIT_FAILURE, "No NF info mempool found\n");
	
	/* Lookup mempool for NF messages */
	nf_msg_pool = rte_mempool_lookup(MP_NF_MSG_NAME);
	if (nf_msg_pool == NULL)
		rte_exit(EXIT_FAILURE, "No NF msg mempool found\n");
		
	/* Initialize the info struct */
	nf_info = d2sc_nfrt_info_init(nf_name);
	
	/* Initialize empty NF's buffer queue */
	d2sc_nfrt_nf_bq_init(nf_bq);
	
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
	if (mz_nt_available = NULL)
		rte_exit(EXIT_FAILURE, "Cannt get NF type available memzone\n");
	nfs_pet_nt_available = mz_nt_available->addr;
	
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
	
	/* Now, map rx and tx rings into NF space */
//	rx_ring = rte_ring_lookup(get_rx_queue_name(nf_info->inst_id));
//	if (rx_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get RX ring - is manager process running?\n");
//	tx_ring = rte_ring_lookup(get_tx_queue_name(nf_info->inst_id));
//	if (tx_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get TX ring - is manager process running?\n");
//	nf_msg_ring = rte_ring_lookup(get_msg_queue_name(nf_info->inst_id));
//	if (nf_msg_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get NF msg ring\n");
		
	/* Tell the manger, this NF is ready to receive packets */
	keep_running = 1;
	
	RTE_LOG(INFO, NFRT, "Finished process init.\n");
	return ret_final;
}

int d2sc_nfrt_scale_init(const char *nf_name) {
	struct d2sc_nf_msg *start_msg;
	
	/* Initialize the info struct */
	scaled_nf_info = d2sc_nfrt_info_init(nf_name);
	
	/* Initialize empty scaled NF's buffer queue */
	d2sc_nfrt_nf_bq_init(scaled_nf_bq);
	
	d2sc_sc_print(default_sc);
	
	/* Put this NF's info struct into nf msg pool for Manager to process startup */
	if (rte_mempool_get(nf_msg_pool, (void **)(&start_msg)) != 0) {
		rte_mempool_put(nf_info_mp, scaled_nf_info);	// give back memory to mempool obtained in NF init function
		rte_exit(EXIT_FAILURE, "Cannot create starup msg\n");
	}
	
	start_msg->msg_type = MSG_NF_STARTING;
	start_msg->msg_data = scaled_nf_info;
	
	/* Wait for an NF ID to be assigned by the manager */
	RTE_LOG(INFO, NFRT, "Waiting for manager to assign an ID...\n");
	for (; scaled_nf_info->status == (uint16_t)NF_WAITTING_FOR_ID; ) {
		sleep(1);
	}
	
	/* This NF is trying to declare an ID already in use. */
	if (scaled_nf_info->status == NF_ID_CONFLICT) {
		rte_mempool_put(nf_info_mp, scaled_nf_info);
		rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
	} else if (scaled_nf_info->status == NF_NO_IDS) {
		rte_mempool_put(nf_info_mp, scaled_nf_info);
		rte_exit(NF_NO_IDS, "There are no ids available for this scaling NF\n");
	} else if (scaled_nf_info->status != NF_STARTING) {
		rte_mempool_put(nf_info_mp, scaled_nf_info);
		rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
	}
	RTE_LOG(INFO, NFRT, "Using Instance ID %d\n", scaled_nf_info->inst_id);
	RTE_LOG(INFO, NFRT, "Using Type ID %d\n", scaled_nf_info->type_id);
	
//	/* Now, map rx and tx rings into NF space */
//	scaled_rx_ring = rte_ring_lookup(get_rx_queue_name(scaled_nf_info->inst_id));
//	if (scaled_rx_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get scaled RX ring - is manager process running?\n");
//	scaled_tx_ring = rte_ring_lookup(get_tx_queue_name(scaled_nf_info->inst_id));
//	if (scaled_tx_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get scalded TX ring - is manager process running?\n");
//	scaled_nf_msg_ring = rte_ring_lookup(get_msg_queue_name(scaled_nf_info->inst_id));
//	if (scaled_nf_msg_ring == NULL) 
//		rte_exit(EXIT_FAILURE, "Cannot get NF msg ring\n");
		
	RTE_LOG(INFO, NFRT, "Finished process scale init.\n");
	return 0;
}


int d2sc_nfrt_run_callback(struct d2sc_nf_info *info, struct buf_queue *bq, pkt_handler handler, 
callback_handler callback) {
	struct rte_mbuf *pkts[PKT_RD_SIZE];
	int ret;
	uint16_t nb_pkts;
	
	static uint64_t last_cycle;
	static uint64_t cur_cycles;
	
	printf("\n NF %d handling packets\n", info->inst_id);
	
	/* Listen for ^C and docker stop so we can exit gracefully */
//	signal(SIGINT, d2sc_nfrt_handle_signal);
//	signal(SIGTERM, d2sc_nfrt_handle_signal);
	
	printf("Sending NF_READY message to manager...\n");
	ret = d2sc_nfrt_nf_ready(info);
	if (ret != 0) rte_exit(EXIT_FAILURE, "Unable to send ready message to manager\n");
		
	printf("[Press Ctrl-C to quit ...]\n");
	last_cycle = rte_get_tsc_cycles();
	for (; keep_running; ) {
		for (; non_blocking; ) {
			nb_pkts = d2sc_nfrt_dequeue_pkts((void **)pkts, info, handler);
		
			if (likely(nb_pkts > 0)) {
				d2sc_pkt_process_tx_batch(bq, pkts, nb_pkts, &nfs[info->inst_id]);
			}
		
			/* Flush the packet buffers */
			d2sc_pkt_enqueue_tx_ring(bq->tx_buf, info->inst_id);
			d2sc_pkt_fush_all_bqs(bq);
		
			d2sc_nfrt_dequeue_new_msgs(nfs[info->inst_id].msg_q);
		
			if (callback != D2SC_NO_CALLBACK) {
				keep_running = !(*callback)() && keep_running;
			}
			cur_cycles = rte_get_tsc_cycles();
			info->srv_time = (cur_cycles - last_cycle) / rte_get_timer_hz();
			last_cycle = cur_cycles;
		
			/* Send nf srv_time msg to manager */
			ret = d2sc_nfrt_nf_srv_time(info);
			if (ret != 0) {
				rte_exit(EXIT_FAILURE, "Unable to send nf service time msg to manager\n");
			}
		}
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


int d2sc_nfrt_handle_new_msg(struct d2sc_nf_msg *msg) {
	switch (msg->msg_type) {
		case MSG_STOP:
			RTE_LOG(INFO, NFRT, "Shutting down...\n");
			keep_running = 0;
			non_blocking = 0;
			break;
		case MSG_NOOP:
		default:
			break;
	}
	return 0;
}


uint8_t d2sc_nfrt_check_scale_msg(struct d2sc_nf_info *nf_info) {
	int i;
	void *msgs[MAX_NTS];
	struct d2sc_scale_msg *msg;	
	struct d2sc_scale_info *scale_info;
	uint8_t scale_sig = SCALE_NO;
	int num_msgs = rte_ring_count(scale_msg_queue);
	
	if (num_msgs == 0) return SCALE_NO;
		
	if (rte_ring_dequeue_bulk(scale_msg_queue, msgs, num_msgs, NULL) == 0)
		return -1;
		
	for (i = 0; i < num_msgs; i++) {
		msg = (struct d2sc_scale_msg *)msgs[i];
		scale_info = (struct d2sc_scale_info *)msg->scale_data;
		
		switch (msg->scale_sig) {
			case SCALE_UP:
				if (scale_info->type_id == nf_info->type_id) {
					scale_sig = SCALE_UP;
				}
				break;
			case SCALE_BLOCK:
				if (scale_info->inst_id == nf_info->inst_id) {
					scale_sig = SCALE_BLOCK;
					non_blocking = 0;
					d2sc_nfrt_scale_block(nf_info);
				}
				break;
			case SCALE_RUN:
				if (scale_info->inst_id == nf_info->inst_id) {
					scale_sig = SCALE_RUN;
					non_blocking = 1;
					d2sc_nfrt_scale_run(nf_info);
				}
				break;
			case SCALE_NO:
			default:
				break;		
		}
		
		rte_mempool_put(nf_msg_pool, (void *)msg);
	}
	return scale_sig;
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


int d2sc_nfrt_run(struct d2sc_nf_info *info, struct buf_queue *bq, pkt_handler handler) {
	return d2sc_nfrt_run_callback(info, bq, handler, D2SC_NO_CALLBACK);
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
				onvm_nfrt_usage(progname);
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

//static void d2sc_nfrt_handle_signal(int sig)
//{
//	if (sig == SIGINT || sig == SIGTERM)
//		keep_running = 0;
//}

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

static void d2sc_nfrt_nf_bq_init(struct buf_queue *bq) {
	bq = calloc(1, sizeof(struct buf_queue));
	bq->mgr_nf = 0;
	bq->tx_buf = calloc(1, sizeof(struct pkt_buf));
	bq->id = init_inst_id;
	bq->rx_bufs = calloc(MAX_NFS, sizeof(struct pkt_buf));
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
		ret_act = (*handler)((struct rte_mbuf *)pkts[i], meta);
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
d2sc_nfrt_dequeue_new_msgs(struct rte_ring *msg_ring) {
	struct d2sc_nf_msg *msg;
	
	/* Check whether this NF has any msgs from the manager */
	if (likely(rte_ring_count(msg_ring) == 0)) {
		return;
	}
	msg = NULL;
	rte_ring_dequeue(msg_ring, (void **)(&msg));
	d2sc_nfrt_handle_new_msg(msg);
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

