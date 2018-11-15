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

#include "d2sc_mgr.h"
#include "d2sc_scale.h"

/*********************NF overload signal************************************/

static uint8_t ol_signal = 0;


/************************Internal functions prototypes************************/

inline static int d2sc_scale_get_info(uint16_t *ids);

inline static uint32_t d2sc_scale_get_queue_size(uint16_t nf_id);

inline static int d2sc_scale_send_msg(uint8_t scale_sig, void *scale_data);


/****************************Interfaces***************************************/


void d2sc_scale_check_overload(void) {
	int i, ret;
	uint16_t load;
	uint16_t nf_id;
	uint16_t nt_id;
	uint16_t ids[MAX_NFS];
	uint16_t srv_time;
	uint16_t max_load;
	uint32_t queue_size;
	
	ret = d2sc_scale_get_info(ids); // check the new_msg_ring every a peried of time
	if (ret == 0) {
		RTE_LOG(INFO, MGR, "Don't get any information from NFs\n");
	} else {
		for (i = 0; i < ret; i++) {
			nf_id = ids[i];
			
			if (!d2sc_nf_is_valid(&nfs[nf_id]))
				continue;
			
			nt_id = nfs[nf_id].nf_info->type_id;
			/* The load of an NF is a product of its packet arrival rate and the per-packet processing time */
			srv_time = nfs[nf_id].nf_info->srv_time;
			max_load = nfs[nf_id].nf_info->max_load;
			load = nfs[nf_id].pkt_rate * srv_time;	
			if (load >= max_load) {
				nfs[nf_id].ol_flag = 1;
				// Adjust the available NFs if the NF is overloaded
				nfs_per_nt_available[nt_id]--;
			}
		}
	}
	
	/* Check whether the NF RX queue is full */
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
		
		nt_id = nfs[i].nf_info->type_id;	
		queue_size = d2sc_scale_get_queue_size(i);
		if (queue_size <= 0) {	// if the RX queue of an NF is full
			if (nfs[i].ol_flag == 0) {
				nfs[i].ol_flag = 1;
				// Adjust the available NFs if the NF is overloaded
				nfs_per_nt_available[nt_id]--;	
			}
		}			
	}
}

void d2sc_scale_ol_signal(void) {
	uint16_t i;
	
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
			
		if (nfs[i].ol_flag == 1) {
			ol_signal = 1;
			return;
		}
	}
	ol_signal = 0;
}


void d2sc_scale_execute(void) {
	uint16_t i, j;
	const char *name;
	struct d2sc_scale_info *scale_info;
	
	for (i = 0; i < num_nts; i++) {
		if (nfs_per_nt_num[i] == 0)
			continue;
		
		/* no available nfs of type i */
		if (nfs_per_nt_available[i] <=0) {
			for (j = 0; j < MAX_NFS; j++) {
				if (!d2sc_nf_is_valid(&nfs[j]))
					continue;
					
				if (nfs[j].nf_info->type_id == i) {
					name = nfs[j].nf_info->name;
					break;
				}		
			}
			scale_info = calloc(1, sizeof(struct d2sc_scale_info));
			scale_info->type_id = i;
			scale_info->name = name;
			d2sc_scale_send_msg(SCALE_YES, (void *)scale_info);
		}
	}
}


/******************************Internal functions*****************************/


inline static int d2sc_scale_get_info(uint16_t *ids) {
	int i;
	void *msgs[MAX_NFS];
	struct d2sc_nf_msg *msg;
	struct d2sc_nf_info *info;
	
	int num_msgs = rte_ring_count(new_msg_ring);
	
	if (num_msgs == 0) return 0;
		
	if (rte_ring_dequeue_bulk(new_msg_ring, msgs, num_msgs, NULL) == 0)
		return 0;
		
	for (i = 0; i < num_msgs; i++) {
		msg = (struct d2sc_nf_msg *) msgs[i];
		
		info = (struct d2sc_nf_info *) msg->msg_data;
		
		/* Skip those NFs that are not in running status */
		if (info->status != NF_RUNNING)
			rte_mempool_put(nf_msg_mp, (void *)msg);
			continue;
		
		ids[i] = info->inst_id;	
		nfs[ids[i]].nf_info = info;
		
		rte_mempool_put(nf_msg_mp, (void *)msg);
	}
	return num_msgs;
}

inline static uint32_t d2sc_scale_get_queue_size(uint16_t nf_id) {
	rte_ring *nf_q;
	uint32_t free_size;
	
	if (!d2sc_nf_is_valid(&nfs[nf_id]))
		return 0;
		
	nf_q = nfs[nf_id].rx_q;
	
	free_size = (nf_q->mask + nf_q->cons->tail - nf_q->prod->head) % ((uint32_t) NF_RING_SIZE);
	
	return free_size;
}

inline static int d2sc_scale_send_msg(uint8_t scale_sig, void *scale_data) {
	int ret;
	struct d2sc_scale_msg *msg;	
	
	ret = rte_mempool_get(nf_msg_mp, (void **)(&msg));
	if (ret != 0) {
		RTE_LOG(INFO, MGR, "Unable to allocate scale msg from pool\n");
		return ret;
	}
	
	msg->scale_sig = scale_sig;
	msg->scale_data = scale_data;
	return rte_ring_sp_enqueue(scale_msg_ring, (void *)msg);
}