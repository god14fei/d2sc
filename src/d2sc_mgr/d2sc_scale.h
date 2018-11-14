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

#define SCALE_SLEEP_TIME 2
#define SCALE_YES 1
#define SCALE_NO 0

struct d2sc_scale_info {
	uint16_t type_id;
	const char *name;
};

struct d2sc_scale_msg {
	uint8_t scale_sig;
	void *scale_data;
};