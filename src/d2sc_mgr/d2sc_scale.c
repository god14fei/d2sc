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

/**********************NF scale up and block signal***************************/

uint8_t up_signal = 0;


/************************Internal functions prototypes************************/

inline static uint32_t d2sc_scale_get_free_size(uint16_t nf_id);

inline static uint32_t d2sc_scale_get_used_size(uint16_t nf_id);

inline static int d2sc_scale_send_msg(uint8_t scale_sig,  struct d2sc_scale_info *scale_data);


/****************************Interfaces***************************************/
void d2sc_scale_check_block(uint16_t dst_type) {
	uint16_t i;
	struct d2sc_scale_info *scale_info;
	
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
	
		if ((nfs[i].nf_info->type_id == dst_type) && (nfs[i].bk_flag == 1)){
			scale_info = rte_calloc("scale_info", 1, sizeof(struct d2sc_scale_info), 0);
			scale_info->type_id = dst_type;
			scale_info->scale_num = 0;
//			scale_info->name = nfs[i].nf_info->name;
			d2sc_scale_send_msg(SCALE_RUN, scale_info);
		}
	}
}


void d2sc_scale_check_overload(void) {
	int i, ret;
	uint64_t load;
	uint16_t nt_id;
	uint16_t srv_time;
	uint64_t max_load;
	uint32_t used_size;
	
	for (i = 0; i < MAX_NFS; i++) {
		
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
		
		// Skip the NFs that do not get the service time info temporarily	
		if (nfs[i].nf_info->srv_time == 0)
			continue;
		
		nt_id = nfs[i].nf_info->type_id;
		/* The load of an NF is a product of its packet arrival rate and the per-packet processing time */	
		srv_time = nfs[i].nf_info->srv_time;
		max_load = nfs[i].nf_info->max_load;
		load = (uint64_t)(nfs[i].pkt_rate * srv_time);
		printf("the load of NF %u is %u\n", i, load);
		if (load >= max_load) {
			RTE_LOG(INFO, MGR, "Have detected overloaded NF %u with NF type %u\n", i, nt_id);
			nfs[i].ol_flag = 1;
			// Adjust the available NFs if the NF is overloaded
			nfs_per_nt_available[nt_id]--;
			
			// Calculate the needed NFs acc. to the load
			uint16_t scale_nfs = ceil((float)load/max_load);
			if (scale_nfs > nfs_per_nt_available[nt_id]) {
				nfs[i].scale_num = scale_nfs - nfs_per_nt_available[nt_id];
			}
		}		
	}

	/* Check whether the NF RX queue is full */
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
		
		nt_id = nfs[i].nf_info->type_id;	
		used_size = d2sc_scale_get_used_size(i);
		if (used_size == NF_RING_SIZE - 1) {	// if the RX queue of an NF is full
			if (nfs[i].ol_flag == 0) {
				nfs[i].ol_flag = 1;
				// Adjust the available NFs if the NF is overloaded
				if (--nfs_per_nt_available[nt_id] == 0)	
					nfs[i].scale_num++;
			}
		}			
	}
}


void d2sc_scale_up_signal(void) {
	uint16_t i;
	
	for (i = 0; i < MAX_NFS; i++) {
		if (!d2sc_nf_is_valid(&nfs[i]))
			continue;
			
		if (nfs[i].ol_flag == 1) {
			up_signal = 1;
			return;
		} 
	}
	up_signal = 0;
}

void d2sc_scale_block_signal(void) {
	uint16_t i;
	static uint32_t cnter = 0;
	static uint32_t check_interval = 5;
	
	for (i = 0; i < MAX_NFS; i++) {
	
		/* Check whether the NF queue is empty */
		if (d2sc_scale_get_free_size(i) == NF_RING_SIZE - 1) {
			// The NF queue keeps empty in 5 checks
			if (++cnter == check_interval) {
				nfs[i].bk_flag = 1;
				cnter = 0;
			}
		}
	}
}


void d2sc_scale_up_execute(uint16_t nf_id) {
	uint16_t i;
	uint16_t nt_id;
//	const char *name;
	struct d2sc_scale_info *scale_info;
	int ret;
	
	nt_id = nfs[nf_id].nf_info->type_id;
	scale_info = rte_calloc("scale_info", 1, sizeof(struct d2sc_scale_info), 0);
	scale_info->type_id = nt_id;
	scale_info->scale_num = nfs[nf_id].scale_num;
//	scale_info->name = name;
	printf("scale number is %u\n", nfs[nf_id].scale_num);
	ret = d2sc_scale_send_msg(SCALE_UP, scale_info);
	if (ret == 0) {
		RTE_LOG(INFO, MGR, "Send scale message to NF Type %u successfully\n", nt_id);
	}
	
}

void d2sc_scale_block_execute(uint16_t dst_nf, uint8_t msg_type) {
	struct d2sc_scale_info *scale_info;
	
	scale_info = rte_calloc("scale_info", 1, sizeof(struct d2sc_scale_info), 0);
	scale_info->inst_id = dst_nf;
//	scale_info->name = nfs[dst_nf].nf_info->name;
	d2sc_scale_send_msg(msg_type, scale_info);
}

/******************************Internal functions*****************************/

//inline static int d2sc_scale_get_info(uint16_t *ids) {
//	int i;
//	void *msgs[MAX_NFS];
//	struct d2sc_nf_msg *msg;
//	struct d2sc_nf_info *info;
//	
//	int num_msgs = rte_ring_count(new_msg_ring);
//	
//	if (num_msgs == 0) return 0;
//		
//	if (rte_ring_dequeue_bulk(new_msg_ring, msgs, num_msgs, NULL) == 0)
//		return 0;
//		
//	for (i = 0; i < num_msgs; i++) {
//		msg = (struct d2sc_nf_msg *) msgs[i];
//		
//		info = (struct d2sc_nf_info *) msg->msg_data;
//		
//		/* Skip those NFs that are not in running status */
//		if (info->status != NF_RUNNING)
//			rte_mempool_put(nf_msg_mp, (void *)msg);
//			continue;
//		
//		ids[i] = info->inst_id;	
//		nfs[ids[i]].nf_info = info;
//		
//		rte_mempool_put(nf_msg_mp, (void *)msg);
//	}
//	return num_msgs;
//}

inline static uint32_t d2sc_scale_get_free_size(uint16_t nf_id) {
	struct rte_ring *nf_q;
	uint32_t free_size;
	
	if (!d2sc_nf_is_valid(&nfs[nf_id]))
		return 0;
		
	nf_q = nfs[nf_id].rx_q;	
	free_size = nf_q->mask + nf_q->cons.tail - nf_q->prod.head;	
	return free_size;
}

inline static uint32_t d2sc_scale_get_used_size(uint16_t nf_id) {
	struct rte_ring *nf_q;
	uint32_t used_size;
	
	if (!d2sc_nf_is_valid(&nfs[nf_id]))
		return 0;
		
	nf_q = nfs[nf_id].rx_q;
	used_size = nf_q->prod.tail - nf_q->cons.head;
	return used_size;
}

inline static int d2sc_scale_send_msg(uint8_t scale_sig, struct d2sc_scale_info *scale_data) {
	int ret;
	struct d2sc_scale_msg *msg;	
	
	ret = rte_mempool_get(nf_msg_mp, (void **)(&msg));
	if (ret != 0) {
		RTE_LOG(INFO, MGR, "Unable to allocate scale msg from pool\n");
		return ret;
	}
	
	msg->scale_sig = scale_sig;
	msg->scale_data = scale_data;
	if (rte_ring_enqueue(scale_msg_ring, msg) < 0) {
		rte_mempool_put(nf_msg_mp, msg);
		rte_exit(EXIT_FAILURE, "Cannot send scale message to NF\n");
	}
	return 0;
}
