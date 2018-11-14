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


#ifndef _D2SC_NF_H_
#define _D2SC_NF_H_

extern uint16_t next_inst_id;


/********************************Interfaces***********************************/


/*
 * Interface giving the smallest unsigned integer unused for a NF instance.
 *
 * Output : the unsigned integer 
 *
 */
uint16_t d2sc_nf_next_inst_id(void);


/*
 * Interface looking through all registered NFs if one needs to start, scale, or stop.
 *
 */
void d2sc_nf_check_status(void);


/*
 * Interface to send a message to a certain NF. This interface will only be called when
 * the manager wants to send a stop message
 *
 * Input  : The destination NF instance ID, a constant denoting the message type
 *          (see onvm_nflib/onvm_msg_common.h), and a pointer to a data argument.
 *          The data argument should be allocated in the hugepage region (so it can
 *          be shared), i.e. using rte_malloc
 * Output : 0 if the message was successfully sent, -1 otherwise
 */
int d2sc_nf_send_msg(uint16_t dst, uint8_t msg_type, void *msg_data);


#endif // _D2SC_NF_H_