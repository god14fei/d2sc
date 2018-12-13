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
                              d2sc_nf.c
                              
       This file contains all functions related to NF management.
       
******************************************************************************/

#include "d2sc_mgr.h"
#include "d2sc_nf.h"
#include "d2sc_stats.h"

/***********************************global variable***************************/
uint16_t next_inst_id = 0;


/************************Internal functions prototypes************************/

/*
 * Function starting a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int d2sc_nf_start(struct d2sc_nf_info *nf_info);


/*
 * Function to mark an NF as ready.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int d2sc_nf_ready(struct d2sc_nf_info *nf_info);


/*
 * Function stopping an NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int d2sc_nf_stop(struct d2sc_nf_info *nf_info);


inline static int d2sc_nf_block(struct d2sc_nf_info *nf_info);


inline static int d2sc_nf_run(struct d2sc_nf_info *nf_info);

/*
 * Function to deliver NF service time information
 */
inline static int d2sc_nf_srv_time(struct d2sc_nf_info *nf_info);


/********************************Interfaces***********************************/

uint16_t d2sc_nf_next_inst_id(void) {
	struct d2sc_nf *nf;
	uint16_t inst_id = MAX_NFS;
	
	while (next_inst_id < MAX_NFS) {
		inst_id = next_inst_id++;
		nf = &nfs[inst_id];
		if (!d2sc_nf_is_valid(nf))	// find an unused NF instance
			break;
	}
	
	return inst_id;
}


void d2sc_nf_check_status(void) {
	int i;
	void *msgs[MAX_NFS];
	struct d2sc_nf_msg *nf_msg;
	struct d2sc_nf_info *nf_info;
	int nb_msgs = rte_ring_count(new_msg_ring);
	
	if (nb_msgs == 0) return;

	if (rte_ring_dequeue_bulk(new_msg_ring, msgs, nb_msgs, NULL) == 0)
		return;
	
	for (i = 0; i < nb_msgs; i++) {
		nf_msg = (struct d2sc_nf_msg *) msgs[i];
		switch (nf_msg->msg_type) {
			case MSG_NF_STARTING:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				d2sc_nf_start(nf_info);
				break;
			case MSG_NF_READY:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				d2sc_nf_ready(nf_info);
				break;
			case MSG_NF_STOPPING:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				if (!d2sc_nf_stop(nf_info))
					num_nfs--;
				break;
			case MSG_NF_BLOCKING:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				d2sc_nf_block(nf_info);
				break;
			case MSG_NF_RUNNING:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				d2sc_nf_run(nf_info);
				break;
			case MSG_NF_SRV_TIME:
				nf_info = (struct d2sc_nf_info *) nf_msg->msg_data;
				d2sc_nf_srv_time(nf_info);
				break;
		}	
		rte_mempool_put(nf_msg_mp, (void *)nf_msg);
	}
}

int d2sc_nf_send_msg(uint16_t dst, uint8_t msg_type, void *msg_data) {
	int ret;
	struct d2sc_nf_msg *msg;
	
	ret = rte_mempool_get(nf_msg_mp, (void **)(&msg));
	if (ret != 0) {
		RTE_LOG(INFO, MGR, "Unable to allocate msg from pool!\n");
		return ret;
	}
	
	msg->msg_type = msg_type;
	msg->msg_data = msg_data;
	
	return rte_ring_sp_enqueue(nfs[dst].msg_q, (void *)msg);
}

/******************************Internal functions*****************************/


inline static int d2sc_nf_start(struct d2sc_nf_info *nf_info) {
	if (nf_info == NULL || nf_info->status != NF_WAITTING_FOR_ID)
		return 1;
		
	// if NF passed its own id on the command line, don't assign here
	uint16_t nf_id = nf_info->inst_id == (uint16_t)NF_NO_ID 
		? d2sc_nf_next_inst_id() : nf_info->inst_id;
			
	if (nf_id >= MAX_NFS) {
		// There is no more available IDs for this NF
		nf_info->status = NF_NO_IDS;
		return 1;
	}
	
	if (d2sc_nf_is_valid(&nfs[nf_id])) {
		// This NF is trying to declare an ID already in use
		nf_info->status = NF_ID_CONFLICT;
		return 1;
	}
	
	// Keep reference to this NF in the manager
	nf_info->inst_id = nf_id;
	nfs[nf_id].nf_info = nf_info;
	nfs[nf_id].inst_id = nf_id;
	
	// Let the NF continue its init process
	nf_info->status = NF_STARTING;
	return 0;
}


inline static int d2sc_nf_ready(struct d2sc_nf_info *nf_info) {
	uint16_t nt_id;
	uint16_t nt_num;
	
	// Ensure we have already called nf_start for this NF
	if (nf_info->status != NF_STARTING) return -1;
	
	// switch the NF to running status	
	nf_info->status = NF_RUNNING;
	
	// Register this NF running with its NF type
	nt_id = nf_info->type_id;
	nt_num = nfs_per_nt_num[nt_id]++;
	nfs_per_nt_available[nt_id]++;
	nts[nt_id][nt_num] = nf_info->inst_id;
	num_nfs++;
	
	return 0;
}


inline static int d2sc_nf_stop(struct d2sc_nf_info *nf_info) {
	uint16_t nf_id;
	uint16_t nt_id;
	int match_id;
	struct rte_mempool *nf_info_mp;
	
	if (nf_info == NULL && nf_info->status != NF_STOPPED)
		return 1;
	
	nf_id = nf_info->inst_id;
	nt_id = nf_info->type_id;
	
	/* Clean up dangling pointers to nf info struct */
	nfs[nf_id].nf_info = NULL;
	
	/* Reset stats */
	d2sc_stats_clear_nf(nf_id);
	
	/* Remove this NF from the NF type map */
	nfs_per_nt_num[nt_id]--;
	nfs_per_nt_available[nt_id]--;
	for (match_id = 0; match_id < MAX_NFS_PER_NT; match_id++) {
		if (nts[nt_id][match_id] == nf_id) {
			break;
		}
	}
	
	if (match_id < MAX_NFS_PER_NT) {
		nts[nt_id][match_id] = 0;
		for (; match_id < MAX_NFS_PER_NT - 1; match_id++) {
			if (nts[nt_id][match_id+1] == 0) {	// skip the inst_id that equals to 0 after match_id
				break;
			}
			nts[nt_id][match_id] = nts[nt_id][match_id+1];
			nts[nt_id][match_id+1] = 0;
		}
	}
	
	/* Remove this NF from the NF available map */
//	for (match_id = 0; match_id < MAX_NFS_PER_NT; match_id++) {
//		if (nts_available[nt_id][match_id] == nf_id) {
//			break;
//		}
//	}
//	
//	if (match_id < MAX_NFS_PER_NT) {
//		nts_available[nt_id][match_id] = 0;
//		for (; match_id < MAX_NFS_PER_NT - 1; match_id++) {
//			if (nts_available[nt_id][match_id+1] == 0) {
//				break;
//			}
//			nts_available[nt_id][match_id] = nts_available[nt_id][match_id+1];
//			nts_available[nt_id][match_id+1] = 0;
//		}
//	}
	
	/* First lookup mempool for nf_info struct, then free this nf_info struct */
	nf_info_mp = rte_mempool_lookup(MP_NF_INFO_NAME);
	if (nf_info_mp == NULL)
		return 1;
	
	rte_mempool_put(nf_info_mp, (void *)nf_info);
	
	return 0;
}

inline static int d2sc_nf_block(struct d2sc_nf_info *nf_info) {
	uint16_t nf_id;
	uint16_t nt_id;
	
	// Ensure we have already received block info from this NF
	if (nf_info->status != NF_BLOCKED) return -1;
	
	nf_id = nf_info->inst_id;	
	nt_id = nf_info->type_id;
	// available NFs of this type minus 1
	nfs_per_nt_available[nt_id]--;
	nfs[nf_id].nf_info->status = nf_info->status;
	return 0;
}

inline static int d2sc_nf_run(struct d2sc_nf_info *nf_info) {
	uint16_t nf_id;
	uint16_t nt_id;
	
	if (nf_info->status != NF_RUNNING) return -1;
		
	nf_id = nf_info->inst_id;
	nt_id = nf_info->type_id;
	
	// available nfs of this type add 1
	nfs_per_nt_available[nt_id]++;
	nfs[nf_id].nf_info = nf_info;
	return 0;
}

inline static int d2sc_nf_srv_time(struct d2sc_nf_info *nf_info) {
	uint16_t nf_id;
	
	/* Ensure this NF is running normally */
	if (nf_info == NULL && nf_info->status != NF_RUNNING) return -1;
	
	nf_id = nf_info->inst_id;
	/* Deliver the nf info with srv time to global nfs */
	nfs[nf_id].nf_info->srv_time = nf_info->srv_time;
			
	return 0;	
}