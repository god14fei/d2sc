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

#ifndef _D2SC_SCALE_H_
#define _D2SC_SCALE_H_

#define SCALE_SLEEP_TIME 4
#define SCALE_UP 3
#define SCALE_BLOCK 2
#define SCALE_RUN 1
#define SCALE_NO 0

#define NF_SCALE_INFO_NAME "nf_%u_scale_info_%"PRIu64
#define NF_SCALE_Q_NAME "nf_%u_scale_queue"

extern uint8_t up_signal;

struct d2sc_scale_info {
	union {
		uint16_t type_id;
		uint16_t inst_id;
	};
	uint16_t scale_num;
};

struct d2sc_scale_msg {
	uint8_t scale_sig;
	void *scale_data;
};


/*
 * Get the scale queue name from the manager to an NF with an NF ID
 */
static inline const char * get_scale_queue_name(unsigned id) {
	static char buffer[sizeof(NF_SCALE_Q_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_SCALE_Q_NAME, id);
	return buffer;
}

/* 
 * Get the scale info name with an NF ID
 */
static inline const char * get_scale_info_name(unsigned id) {
	static char buffer[sizeof(NF_SCALE_INFO_NAME) + 2];
	
	snprintf(buffer, sizeof(buffer) - 1, NF_SCALE_INFO_NAME, id, rte_get_tsc_cycles());
	return buffer;
}

void d2sc_scale_check_block(void);

void d2sc_scale_run_parent(uint16_t dst_type);

void d2sc_scale_check_load(uint16_t type_id);

void d2sc_scale_check_overload(void);

void d2sc_scale_up_signal(void);

void d2sc_scale_up_execute(uint16_t nf_id);

void d2sc_scale_block_signal(void);

void d2sc_scale_block_execute(uint16_t dst_nf, uint8_t msg_type);


#endif //	_D2SC_SCALE_H_