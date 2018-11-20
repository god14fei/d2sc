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

                                  d2sc_init.c
                                  
                  File containing initialization functions.
                  
******************************************************************************/

#include "d2sc_mgr/d2sc_init.h"

/********************************Global variables*****************************/

struct rte_ring *new_msg_ring;
struct rte_ring *scale_msg_ring;

struct d2sc_nf *nfs = NULL;
struct port_info *ports = NULL;

struct rte_mempool *pktmbuf_mp;
struct rte_mempool *nf_info_mp;
struct rte_mempool *nf_msg_mp;
uint16_t **nts;
uint16_t *nfs_per_nt_num;
uint16_t *nfs_pet_nt_available;		// Number of available NFs per NF type, i.e., not overloaded
struct d2sc_sc *default_sc;
struct d2sc_sc **default_sc_p;


/*************************Internal Functions Prototypes***********************/

static int init_mbuf_mps(void);
static int init_nf_info_mp(void);
static int init_nf_msg_mp(void);
static int init_port(uint8_t port_id);
static int init_shm_rings(void);
static int init_new_msg_ring(void);
static int init_scale_msg_ring(void);
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);


/*****************Internal Configuration Structs and Constants*****************/

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /* Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /* Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /* Default values of TX write-back threshold reg. */

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0,                    /* header split disabled */
		.hw_ip_checksum = 1,                    /* IP checksum offload enabled */
		.hw_vlan_filter = 0,                    /* VLAN filtering disabled */
		.jumbo_frame    = 0,                    /* jumbo frame support disabled */
		.hw_strip_crc   = 1,                    /* CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = rss_symmetric_key,
			.rss_hf  = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
  	.pthresh = RX_PTHRESH,
  	.hthresh = RX_HTHRESH,
  	.wthresh = RX_WTHRESH,
	},
	.rx_free_thresh = 32,
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
  	.pthresh = TX_PTHRESH,
   	.hthresh = TX_HTHRESH,
   	.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 0,
	.tx_rs_thresh   = 0,
	.txq_flags      = 0,
};


/*********************************Interfaces**********************************/

int init(int argc, char *argv[]) {
	int retval;
	const struct rte_memzone *mz_nf;
	const struct rte_memzone *mz_port;
	const struct rte_memzone *mz_scp;
	const struct rte_memzone *mz_nts;
	const struct rte_memzone *mz_nfs_per_nt;
	const struct rte_memzone *mz_nt_available;
	uint8_t i, n_ports, port_id;
	
	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0)
		return -1;
	argc -= retval;
	argv += retval;
	
#ifdef RTE_LIBRTE_PDUMP
	rte_pdump_init(NULL);
#endif
	
	/* get total number of ports */
	n_ports = rte_eth_dev_count();
	
	/* set up memory zone for nfs */
	mz_nf = rte_memzone_reserve(MZ_NF_INFO, sizeof(*nfs) * MAX_NFS, rte_socket_id(), NO_FLAGS);
	if(mz_nf == NULL) 
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for nf information.\n");
	else {
		memset(mz_nf->addr, 0, sizeof(*nfs) * MAX_NFS);
		nfs = mz_nf->addr;
	}
	
	/* set up memory zone for port info */
	mz_port = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports), rte_socket_id(), NO_FLAGS);
	if(mz_port == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for port information.\n");
	else
		ports = mz_port->addr;
	
	/* set up memory zone for NF type info */
	mz_nts = rte_memzone_reserve(MZ_NTS_INFO, sizeof(uint16_t) * num_nts, rte_socket_id(), NO_FLAGS);
	if (mz_nts == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for nf type information.\n");
	else {
		nts = mz_nts->addr;
		for (i=0; i < num_nts; i++) {
			nts[i] = rte_calloc("NF instances of one type", MAX_NFS_PER_NT, sizeof(uint16_t), 0);
		}
	}
	
	/* set up memory zone for nfs per nf types info */
	mz_nfs_per_nt = rte_memzone_reserve(MZ_NFS_PER_NT_INFO, sizeof(uint16_t) * num_nts, rte_socket_id(), NO_FLAGS);
	if (mz_nfs_per_nt == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for nfs per nf type information.\n");
	else
		nfs_per_nt_num = mz_nfs_per_nt->addr;
		
	mz_nt_available = rte_memzone_reserve(MZ_NT_AVAILABLE_INFO, sizeof(uint16_t) * num_nts, rte_socket_id(), NO_FLAGS);
	if (mz_nt_available == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for nt available information.\n");
	else
		nfs_per_nt_available = mz_nt_available->addr;
		
	/* parse manger specific arguments */
	retval = parse_mgr_args(n_ports, argc, argv);
	if (retval != 0) 
		return -1;
	
	/* initialize mbuf mempool */
	retval = init_mbuf_mps();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create the mbuf mempools.\n");
		
	/* initialize nf info mempool */
	retval = init_nf_info_mp();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create nf info mempools.\n");
		
	/* initialize nf message mempool */
	retval = init_nf_msg_mp();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create nf message mempool.\n");
		
	/* initialize the ports */
	for (i=0; i < ports->n_ports; i++) {
		port_id = ports->id[i];
		rte_eth_macaddr_get(port_id, &ports->mac[port_id]);
		retval = init_port(port_id);
		if (retval != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize port %u\n", port_id);
	}
	
	check_all_ports_link_status(ports->n_ports, (~0x0));
	
	/* initialize the NF queues/rings */
	init_shm_rings();
	
	/* initialize a queue for newly created NFs */
	init_new_msg_ring();
	
	/* initialize a queue for NF scaling msg */
	init_scale_msg_ring();
	
	/* initialize a default service chain */
	default_sc = d2sc_sc_create();
	retval = d2sc_sc_merge_entry(default_sc, D2SC_NF_ACT_TONF, 1);
	if (retval == ENOSPC) {
		printf("service chain length cannot exceed the maxmum chain length!\n");
		exit(1);
	}
	printf("Deafult service chain: send to a specific NF\n");
	
	/* set up service chain pointer shared to NFs */
	mz_scp = rte_memzone_reserve(MZ_SCP_INFO, sizeof(struct d2sc_sc *),
				rte_socket_id(), NO_FLAGS);
	if (mz_scp == NULL) 
		rte_exit(EXIT_FAILURE, "Cannot reserve memzone for service chain pointer\n");
	else {
		memset(mz_scp->addr, 0, sizeof(struct d2sc_sc *));
		default_sc_p = mz_scp->addr;
	}
	*default_sc_p = default_sc;
	d2sc_sc_print(default_sc);
	
	d2sc_fs_init();
	
	return 0;
}


/*****************************Internal functions******************************/


/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the manager - currently none.
 */
static int init_mbuf_mps(void) {
	const unsigned n_mbufs = (MAX_NFS * MBUFS_PER_NF) + (ports->n_ports * MBUFS_PER_PORT);
	
	printf("Creating mbuf mempool '%s' [%u mbufs] ...\n", MP_PKTMBUF_NAME, n_mbufs);
	pktmbuf_mp = rte_mempool_create(MP_PKTMBUF_NAME, n_mbufs, MBUF_SIZE, MBUF_CACHE_SIZE, 
				sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_init, 
				NULL, rte_pktmbuf_init, NULL, rte_socket_id(), NO_FLAGS);

	return (pktmbuf_mp == NULL); /* 0 on success */
}

/**
 * Set up a mempool to store nf_info structs
 */
static int init_nf_info_mp(void) {
	
	printf("Creating mbuf pool '%s' ...\n", MP_NF_INFO_NAME);
	nf_info_mp = rte_mempool_create(MP_NF_INFO_NAME, MAX_NFS, NF_INFO_SIZE, NF_INFO_CACHE_SIZE, 
				0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
																	
	return (nf_info_mp == NULL); /* 0 on success */
}

/**
 * Set up a mempool to store nf_msg structs
 */
static int init_nf_msg_mp(void) {
	
	printf("Creating mbuf pool '%s' ...\n", MP_NF_MSG_NAME);
	nf_msg_mp = rte_mempool_create(MP_NF_MSG_NAME, MAX_NFS * NF_MSG_Q_SIZE, NF_MSG_SIZE, 
				NF_MSG_CACHE_SIZE, 0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
							 
	return (nf_msg_mp == NULL); /* 0 on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int init_port(uint8_t port_id) {
	const uint16_t rx_rings = D2SC_NUM_RX_THREADS, tx_rings = MAX_NFS;
	const uint16_t rx_ring_size = RTE_RX_DESC_DEFAULT;
	const uint16_t tx_ring_size = RTE_TX_DESC_DEFAULT;
	
	uint16_t q;
	int retval;
	
	printf("Port %u init ... \n", (unsigned)port_id);
	printf("Port %u socket id %u ... \n", (unsigned)port_id, (unsigned)rte_eth_dev_socket_id(port_id));
	printf("Port %u Rx rings %u ... \n", (unsigned)port_id, (unsigned)rx_rings);
	fflush(stdout);
  
	/* port initialization - configure port, and set up rx and tx rings */
	if ((retval = rte_eth_dev_configure(port_id, rx_rings, tx_rings, &port_conf)) != 0)
  	return retval;
  
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port_id, q, rx_ring_size, rte_eth_dev_socket_id(port_id), 
  				&rx_conf, pktmbuf_mp);
		if (retval < 0)
			return retval;
	}
  
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port_id, q, tx_ring_size, rte_eth_dev_socket_id(port_id),
  				&tx_conf);
		if (retval < 0)
  		return retval;
	}
  
	/* Enable RX in promiscuous mode for the Ethernet device */
	rte_eth_promiscuous_enable(port_id);
  
	/* Start the Ethernet port */
	retval = rte_eth_dev_start(port_id);
	if (retval < 0)
		return retval;
  	
	printf("done: \n");
  
	return 0;
}	

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, n, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	
	printf("\nChecking link status:");
	fflush(stdout);
	
	for (n = 0; n <= MAX_CHECK_TIME; n++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << ports->id[portid])) == 0)
				continue;
			memset(&link, 0, sizeof(link));
		 	rte_eth_link_get_nowait(ports->id[portid], &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if(link.link_status)
					printf("Port %d Link Up - speed %u "
      					"Mbps - %s\n", ports->id[portid],
      					(unsigned)link.link_speed,
								(link.link_duplex == ETH_LINK_FULL_DUPLEX) ? 
								("full-duplex") : ("half-duplex"));
				else
					printf("Port %d Link Down\n",
								(uint8_t)ports->id[portid]);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;
		
		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}
	      
		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || n == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			print("done\n");
		}
	}
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and NF processes.
 * Each NF needs one RX queue.
 */
static int
init_shm_rings(void) {
	unsigned i, socket_id;
	const char *rxq_name;
	const char *txq_name;
	const char *msgq_name;
	const unsigned ring_size = NF_RING_SIZE;
	const unsigned msg_ring_size = NF_MSG_RING_SIZE;
	
	for (i=0; i < MAX_NFS; i++) {
		/* Create an rx queue and tx queue for each NF */
		socket_id = rte_socket_id();
		rxq_name = get_rx_queue_name(i);
		txq_name = get_tx_queue_name(i);
		msgq_name = get_msg_queue_name(i);
		nfs[i].inst_id = i;
		nfs[i].ol_flag = 0;		// preset the ovload flat to 0, no overload
		nfs[i].pkt_rate = 0;	// initialize the pkt rate of all NFs
		nfs[i].rx_q = rte_ring_create(rxq_name, ring_size,
					socket_id, RING_F_SC_DEQ);	/* multi prod, single cons */
		nfs[i].tx_q = rte_ring_create(txq_name, ring_size,
					socket_id, RING_F_SC_DEQ);	/* multi prod, single cons */
		nfs[i].msg_q = rte_ring_create(msgq_name, msg_ring_size,
					socket_id, RING_F_SC_DEQ);	/* multi prod, single cons */
		
		if (nfs[i].rx_q == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for NF %u\n", i);
			
		if (nfs[i].tx_q == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for NF %u\n", i);
		
		if (nfs[i].msg_q == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create msg ring queue for NF %u\n", i);														 
	}
	return 0;
}

/**
 * Allocate a rte_ring for newly created NFs
 */
static int init_new_msg_ring(void) {
	new_msg_ring = rte_ring_create(MGR_MSG_Q_NAME, MAX_NFS, 
				rte_socket_id(), RING_F_SC_DEQ);	/* multi prod, single cons */
											
	if (new_msg_ring == NULL) 
		rte_exit(EXIT_FAILURE, "Cannot create msg queue for new NFs");
		
	return 0;
}

/**
 * Allocate a rte_ring for NF scaling msgs
 */
static int init_scale_msg_ring(void) {
	scale_msg_ring = rte_ring_create(MGR_SCALE_Q_NAME, MAX_NTS, 
				rte_socket_id(), RING_F_SC_DEQ);	/* multi prod, single cons */
											
	if (scale_msg_ring == NULL) 
		rte_exit(EXIT_FAILURE, "Cannot create scale queue for NFs");
		
	return 0;
}