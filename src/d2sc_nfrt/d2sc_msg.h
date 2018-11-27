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
                               d2sc_msg.h
           Shared structures about messages between the manager and NFs
 *
 *****************************************************************************/

#ifndef _D2SC_MSG_H_
#define _D2SC_MSG_H_

#include <stdint.h>

#define MSG_NOOP 0
#define MSG_STOP 1
#define MSG_NF_STARTING 2
#define MSG_NF_READY 3
#define MSG_NF_STOPPING 4
#define MSG_NF_BLOCKING 5
#define MSG_NF_RUNNING 6
#define MSG_NF_SRV_TIME 7		// Send the NF service time info to the Manager

#define MGR_MSG_Q_NAME "mgr_msg_q"		// Mgr msg queue name
#define MGR_SCALE_Q_NAME "mgr_scale_q"	// Mgr scale queue name
#define NF_MSG_Q_NAME "nf_%u_msg_q"		 // NF msg queue name
#define MP_NF_MSG_NAME "nf_msg_mp"		// NF msg mempool name

struct d2sc_nf_msg {
	uint8_t msg_type;	/* Constant saying what type of message is */
	void *msg_data;	/* It should be rte_malloc'd to keep it store in hugepages */
};


/*
 * Get the msg name from the manager to an NF with an NF ID
 */
static inline const char * get_msg_queue_name(unsigned id) {
	static char buffer[sizeof(NF_MSG_Q_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_MSG_Q_NAME, id);
	return buffer;
}

#endif