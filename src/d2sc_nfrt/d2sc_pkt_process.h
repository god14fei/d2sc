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

                                 d2sc_pkt_process.h
                                  
     Header file containing all prototypes of packet processing functions
    
******************************************************************************/

#ifndef _D2SC_PKT_PROCESS_H_
#define _D2SC_PKT_PROCESS_H_


#include "d2sc_common.h"
#include "d2sc_sc.h"
#include "d2sc_includes.h"
#include "d2sc_flow_steer.h"
#include "d2sc_mgr/d2sc_nf.h"
#include "d2sc_mgr/d2sc_mgr.h"

extern struct port_info *ports;
extern struct d2sc_sc *default_sc;

/*********************************Interfaces**********************************/

/*
 * Interface to process packets in a given RX queue.
 *
 * Inputs : a pointer to the rx buffer queue
 *          an array of packets
 *          the size of the array
 *
 */
void d2sc_pkt_process_rx_batch(struct buf_queue *mgr_bq, struct rte_mbuf *rx_pkts[], uint16_t nb_rx);


/*
 * Interface to drop a batch of packets.
 *
 * Inputs : the array of packets
 *          the size of the array
 *
 */
void d2sc_pkt_drop_batch(struct rte_mbuf **pkts, uint16_t size);


void d2sc_pkt_enqueue_bq(struct buf_queue *mgr_bq, uint16_t dst_nt_id, struct rte_mbuf *pkt);


void d2sc_pkt_process_tx_batch(struct buf_queue *mgr_bq, struct rte_mbuf *tx_pkts[], uint16_t nb_tx, struct d2sc_nf *nf);


void d2sc_pkt_flush_all_bqs(struct buf_queue *mgr_bq);


void d2sc_pkt_flush_nf_bq(struct pkt_buf *nf_buf, uint16_t nf_id);


void d2sc_pkt_flush_port_bq(struct buf_queue *mgr_bq, uint16_t portid);


void d2sc_pkt_flush_all_ports(struct buf_queue *mgr_bq);


void d2sc_pkt_enqueue_tx_ring(struct pkt_buf *nf_buf, uint16_t nf_id);


#endif // _D2SC_PKT_PROCESS_H_