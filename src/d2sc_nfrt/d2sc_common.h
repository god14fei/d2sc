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
                               d2sc_common.h
                       Shared data between host and NFs
 *
 *****************************************************************************/


#ifndef _D2SC_COMMON_H_
#define _D2SC_COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ether.h>

#include "d2sc_msg_common.h"

#define D2SC_MAX_SC_LENGTH 6		// the maximum service chain length
#define MAX_NFS 16							// total numer of NF instances allowed
#define MAX_NTS 16							// total numer of different NF types allowed
#define MAX_NFS_PER_NT 8				// max numer of NF instances per NF type

#define NF_BQ_SIZE 16384			//size of the NF buffer queue

#define PKT_RD_SIZE ((unit16_t)32)

#define D2SC_NF_ACTION_DROP 0		// NF instance drop the packet
#define D2SC_NF_ACTION_NEXT 1   // to perform the next action determined by the flow table lookup
#define D2SC_NF_ACTION_TONF 2		// sent the packet to a specified NF instance
#define D2SC_NF_ACTION_OUT 3		// sent the packet to the NIC port 

//flag operations that should be used on d2sc_pkt_meta
#define D2SC_CHECK_BIT(flags, n) !!((flags) & (1 << (n)))
#define D2SC_SET_BIT(flags, n) ((flags) | (1 << (n)))
#define D2SC_CLEAR_BIT(flags, n) ((flags) & (0 << (n)))

struct d2sc_pkt_meta{
	unit8_t action;
	unit16_t des;
	unit16_t src;
	unit8_t sc_index;
	unit8_t flags;
}