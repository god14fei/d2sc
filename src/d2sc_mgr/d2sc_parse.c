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

                                  d2sc_parse.c
                                  
    File containing the function parsing all DPDK and D2SC arguments.
    
******************************************************************************/

#include "d2sc_mgr/d2sc_parse.h"
#include "d2sc_mgr/d2sc_stats.h"


/******************************Global variables*******************************/


/* global variable for number of currently active NFs */
uint16_t num_nfs;

/* global variable for numer of nf types */
uint16_t num_nts = MAX_NTS;

/* global variable for the default nf type id */
uint16_t default_nt = DEFAULT_NT_ID;

/* global variable for number of tx threads */
uint16_t num_rx_threads = D2SC_NUM_RX_THREADS;

/* global variable to where to print stats */
D2SC_STATS_OUTPUT stats_dst = D2SC_STATS_NONE;

/* global variable for how long stats should wait before updating */
uint16_t global_stats_sleep_time = 1;

/* global variable for program name */
static const char *progname;


/***********************Internal Functions prototypes*************************/


static void usage(void);


static int parse_port_mask(uint8_t n_ports, const char *port_mask);


static int parse_num_nts(const char *nts);


static int parse_default_nt(const char *nt);


static int parse_num_rx_threads(const char *num_rx);


static int parse_stats_output(const char *stats_output);


static int parse_stats_sleep_time(const char *sleep_time);


/*********************************Interfaces**********************************/


int parse_mgr_args(uint8_t n_ports, int argc, char *argv[]) {
	int opt_index, opt;
	char **argv_opt = argv;
	
	static struct option lgopts[] = {
		{"port-mask",        required_argument,    NULL,    'p'},
		{"num-nts",          required_argument,    NULL,    't'},
		{"default-nt",       required_argument,    NULL,    'd'},
		{"num_rx_threads",   no_argument,          NULL,    'r'},
		{"stats-output",     no_argument,          NULL,    's'},
		{"stats-sleep-time", no_argument,          NULL,    'z'}
	};
	
	progname = argv[0];
	
	while ((opt = getopt_long(argc, argv_opt, "p:t:d:r:s:z:", lgopts, &opt_index)) != EOF) {
		switch (opt) {
			case 'p': 
				if (parse_port_mask(n_ports, optarg) != 0) {
					usage();
					return -1;
				}
				break;
			case 't':
				if (parse_num_nts(optarg) != 0) {
					usage();
					return -1;
				}
				break;
			case 'd': 
				if (parse_default_nt(optarg) != 0) {
					usage();
					return -1;
				}
				break;
			case 'r':
				if (parse_num_rx_threads(optarg) !=0) {
					usage();
					return -1;
				}
				break;
			case 's':
				if (parse_stats_output(optarg) != 0) {
					usage();
					return -1;
				}
				d2sc_stats_set_output(stats_dst);
				break;
			case 'z':
				if (parse_stats_sleep_time(optarg) != 0) {
					usage();
					return -1;
				}
				break;
			default:
				printf("ERROR: unkown option '%c'\n", opt);
				usage();
				return -1;
		}
	}
	return 0;
}


static void usage(void) {
	printf(
		"%s [EAL options] -- \n"
		"\t-p (manatory) PORTMASK: hexadecimal bitmask of ports to use\n"
		"\t-t NUM_NTS: nuber of unique nf types allowed. defaults to 16 (optional)\n"
		"\t-d DEFAULT_NT: the nf type to initially receive packets. defaults to 1 (optional)\n"
		"\t-r NUM_RX_THREADS: the number of rx thread to use. defaults to 2 (optional)\n"
		"\t-s STATS_OUTPUT: where to output manager runtime stats (stdout/stderr). defaults to NONE (optional)\n"
		"\t-z STATS_SLEEP_TIME: the stats update interval (in seconds)\n",
		progname);
}


static int parse_port_mask(uint8_t n_ports, const char *port_mask) {
	char *end = NULL;
	unsigned long pm;
	uint8_t num = 0;
	
	if (port_mask == NULL)
		return -1;
		
	/* convert parameter to a number and verfy */
	pm = strtoul(port_mask, &end, 16);
	if(pm == 0) {
		printf("WARNING: no ports are being used! \n");
		return 0;
	}
	
	if (end == NULL || *end != '\0' || pm == 0)
		return -1;
		
	/* while loop go through bits of the mask and mark ports */
	while (pm != 0) {
		if (pm & 0x01) {	/*bit of 1 represents a usable port */
			if (num >= n_ports)
				printf("WARNING: requested port %u is non-existent - ignoring\n", (unsigned)num);
			else
				ports->id[ports->n_ports++] = num;			
		}
		pm = (pm >> 1);
		num++;
	}
	return 0;
}


static int parse_num_nts(const char *nts) {
	char *end = NULL;
	unsigned long temp;
	
	temp = strtoul(nts, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;
	
	num_nts = (uint16_t)temp;
	return 0;
}


static int parse_default_nt(const char *nt) {
	char *end = NULL;
	unsigned long temp;
	
	temp = strtoul(nt, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;
	
	default_nt = (uint16_t)temp;
	return 0;	
}


static int parse_num_rx_threads(const char *num_rx) {
	char *end = NULL;
	unsigned long temp;
	
	temp = strtoul(num_rx, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0) 
		return -1;
	
	num_rx_threads = (uint16_t)temp;
	return 0;
}


static int parse_stats_output(const char *stats_output) {
	if (!strcmp(stats_output, D2SC_STR_STATS_STDOUT)) {
		stats_dst = D2SC_STATS_STDOUT;
		return 0;
	} else if (!strcmp(stats_output, D2SC_STR_STATS_STDERR)) {
		stats_dst = D2SC_STATS_STDERR;
		return 0;
	} else {
		return -1;
	}
}


static in parse_stats_sleep_time(const char *sleep_time) {
	char *end = NULL;
	unsigned long temp;
	
	temp = strtoul(sleep_time, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;
		
	global_stats_sleep_time = (uint16_t)temp;
	return 0
}