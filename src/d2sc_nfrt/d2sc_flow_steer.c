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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include "d2sc_common.h"
#include "d2sc_flow_table.h"
#include "d2sc_flow_steer.h"

#define NO_FLAGS 0
#define FT_ENTRIES 1024

struct d2sc_ft *ft;
struct d2sc_ft **ft_p;

int d2sc_fs_init(void) {
	const struct rte_memzone *mz_ftp;
	
	ft = d2sc_ft_create(FT_ENTRIES, sizeof(struct d2sc_flow_entry));
	if (ft == NULL) {
		rte_exit(EXIT_FAILURE, "Unable to create flow table\n");
	}
	mz_ftp = rte_memzone_reserve(MZ_FTP_INFO, sizeof(struct d2sc_ft *), 
			rte_socket_id(), NO_FLAGS);
	if (mz_ftp == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for flow table pointer\n");
	}
	memset(mz_ftp->addr, 0, sizeof(struct d2sc_ft *));
	ft_p = mz_ftp->addr;
	*ft_p = ft;
	return 0;
}

int d2sc_fs_get_entry(rte_mbuf *pkt, struct d2sc_flow_entry **flow_entry) {
	int ret; 
	ret = d2sc_ft_lookup_pkt(ft, pkt, (char **)flow_entry);
	
	return ret;
}