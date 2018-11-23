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

#define SCALE_SLEEP_TIME 3
#define SCALE_UP 2
#define SCALE_BLOCK 1
#define SCALE_NO 0

struct d2sc_scale_info {
	union {
		uint16_t type_id;
		uint16_t inst_id;
	};
	const char *name;
};

struct d2sc_scale_msg {
	uint8_t scale_sig;
	void *scale_data;
};


void d2sc_scale_check_overload(void);

void d2sc_scale_up_signal(void);

void d2sc_scale_up_execute(void);

void d2sc_scale_block_signal(void);

void d2sc_scale_block_execute(void);


#endif //	_D2SC_SCALE_H_