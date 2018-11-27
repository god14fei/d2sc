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

                                 d2sc_pkt_process.c
                                  
           This file contains all functions related to packet processing.
    
******************************************************************************/


#include "d2sc_mgr/d2sc_mgr.h"
#include "d2sc_mgr/d2sc_nf.h"
#include "d2sc_pkt_process.h"

/**********************Internal Functions Prototypes**************************/


static inline void d2sc_pkt_process_next_act(struct buf_queue *mgr_bq, struct rte_mbuf *pkt, struct d2sc_nf *nf);


static inline void d2sc_pkt_enqueue_port(struct buf_queue *mgr_bq, uint16_t portid, struct rte_mbuf *pkt);


static int d2sc_pkt_drop(struct rte_mbuf *pkt);


/**********************************Interfaces*********************************/


void d2sc_pkt_process_rx_batch(struct buf_queue *mgr_bq, struct rte_mbuf *rx_pkts[], uint16_t nb_rx) {
	uint16_t i;
	struct d2sc_pkt_meta *meta;
	struct d2sc_flow_entry *entry;
	struct d2sc_sc *sc;
	int ret;
	
	if (mgr_bq == NULL && rx_pkts == NULL)
		return;
	
	/* Each packet needs to looup the flow table, this might hurt performance */	
	for (i = 0; i < nb_rx; i++) {
		meta = (struct d2sc_pkt_meta *) &(rx_pkts[i]->udata64);
		meta->src = 0;
		meta->sc_index = 0;
		ret = d2sc_fs_get_entry(rx_pkts[i], &entry);
		if (ret >= 0) {		// Store the flow table lookup result in packet meta
			sc = entry->sc;
			meta->act = d2sc_sc_next_act(sc, rx_pkts[i]);
			meta->dst = d2sc_sc_next_dst(sc, rx_pkts[i]);
		} else {
			meta->act = d2sc_sc_next_act(default_sc, rx_pkts[i]);
			meta->dst = d2sc_sc_next_dst(default_sc, rx_pkts[i]);
		}
		
		(meta->sc_index)++;
		d2sc_pkt_enqueue_bq(mgr_bq, meta->dst, rx_pkts[i]);
	}
	
	d2sc_pkt_flush_all_bqs(mgr_bq);
}


void d2sc_pkt_enqueue_bq(struct buf_queue *mgr_bq, uint16_t dst_nt_id, struct rte_mbuf *pkt) {
	struct d2sc_nf *nf;
	uint16_t dst_nf_id;
	struct pkt_buf *nf_buf;
	
	if (mgr_bq == NULL || pkt == NULL)
		return;
		
	// map nf type to instance and check one exists
	dst_nf_id = d2sc_sc_nt_to_nf_map(dst_nt_id, pkt);
	if (dst_nf_id == 0) {
		d2sc_pkt_drop(pkt);
		return;
	}
	
	//Ensure destination NF is running and ready to receive packets
	nf = &nfs[dst_nf_id];
	if (!d2sc_nf_is_valid(nf)) {
		d2sc_pkt_drop(pkt);
		return;
	}
	
	nf_buf = &mgr_bq->rx_bufs[dst_nf_id];
	nf_buf->buf[nf_buf->cnt++] = pkt;
	if (nf_buf->cnt == PKT_RD_SIZE) {
		d2sc_pkt_flush_nf_bq(nf_buf, dst_nf_id);
	}
}


void d2sc_pkt_flush_nf_bq(struct pkt_buf *nf_buf, uint16_t nf_id) {
	uint16_t i;
	struct d2sc_nf *nf;
		
	if (nf_buf->cnt == 0)
		return;
	
	nf = &nfs[nf_id];
	
	if (!d2sc_nf_is_valid(nf)) 
		return;
		
	if (rte_ring_enqueue_bulk(nf->rx_q, (void **)nf_buf->buf, nf_buf->cnt, NULL) == 0) {
		for (i = 0; i < nf_buf->cnt; i++) {
			d2sc_pkt_drop(nf_buf->buf[i]);
		}
		nf->stats.rx_drop += nf_buf->cnt;
	} else {
		nf->stats.rx += nf_buf->cnt;
	}
	nf_buf->cnt = 0;
}


void d2sc_pkt_flush_all_bqs(struct buf_queue *mgr_bq) {
	uint16_t i;
	struct pkt_buf *nf_buf;
	
	if (mgr_bq == NULL)
		return;
		
	for (i = 0; i < MAX_NFS; i++) {
		nf_buf = &mgr_bq->rx_bufs[i];
		d2sc_pkt_flush_nf_bq(nf_buf, i);
	}
}


void d2sc_pkt_process_tx_batch(struct buf_queue *mgr_bq, struct rte_mbuf *tx_pkts[], uint16_t nb_tx, struct d2sc_nf *nf) {
	uint16_t i;
	struct d2sc_pkt_meta *meta;
	struct pkt_buf *out_buf;
	
	if (mgr_bq == NULL || tx_pkts == NULL || nf == NULL)
		return;
		
	for (i = 0; i < nb_tx; i++) {
		meta = (struct d2sc_pkt_meta *) &(tx_pkts[i]->udata64);
		meta->src = nf->inst_id;
		
		switch (meta->act) {
			case D2SC_NF_ACT_DROP:
				// if the packet is drop, then <return value> is 0
				// and !<return value> is 1.
				nf->stats.act_drop += !d2sc_pkt_drop(tx_pkts[i]);
				break;
			case D2SC_NF_ACT_NEXT:
				nf->stats.act_next++;
				d2sc_pkt_process_next_act(mgr_bq, tx_pkts[i], nf);
				break;
			case D2SC_NF_ACT_TONF:
				nf->stats.act_tonf++;
				d2sc_pkt_enqueue_bq(mgr_bq, meta->dst, tx_pkts[i]);
				break;
			case D2SC_NF_ACT_OUT:
				if (mgr_bq->mgr_nf == 1) {
					nf->stats.act_out++;
					d2sc_pkt_enqueue_port(mgr_bq, meta->dst, tx_pkts[i]);
					break;
				} else {
					out_buf = mgr_bq->tx_buf;
					out_buf->buf[out_buf->cnt++] = tx_pkts[i];
					if (out_buf->cnt == PKT_RD_SIZE) {
						d2sc_pkt_enqueue_tx_ring(out_buf, nf->inst_id);
					}
					break;
				}
			default:
				printf("Invalid action: this should not happen!\n");
				d2sc_pkt_drop(tx_pkts[i]);
				return;
		}
	}
}


void d2sc_pkt_flush_port_bq(struct buf_queue *mgr_bq, uint16_t portid) {
	uint16_t i, nb_tx;
	struct pkt_buf *port_buf;
	
	if (mgr_bq->mgr_nf != 1)
		return;
	
	port_buf = &mgr_bq->tx_thread->tx_bufs[portid];
	if (port_buf->cnt == 0)
		return;
		
	nb_tx = rte_eth_tx_burst(portid, mgr_bq->id, port_buf->buf, port_buf->cnt);
	if (unlikely(nb_tx < port_buf->cnt)) {
		for (i = nb_tx; i < port_buf->cnt; i++) {
			d2sc_pkt_drop(port_buf->buf[i]);
		}
		ports->tx_stats.tx_drop[portid] += (port_buf->cnt - nb_tx);
	}
	ports->tx_stats.tx[portid] += nb_tx;
	
	port_buf->cnt = 0;
}


void d2sc_pkt_flush_all_ports(struct buf_queue *mgr_bq) {
	uint16_t i;
	
	if (mgr_bq == NULL)
		return;
		
	for (i = 0; i < ports->n_ports; i++) {
		d2sc_pkt_flush_port_bq(mgr_bq, ports->id[i]);
	}
}


void d2sc_pkt_enqueue_tx_ring(struct pkt_buf *nf_buf, uint16_t nf_id) {
	unsigned ret;
	
	if (nf_buf->cnt == 0)
		return;
	
	ret = rte_ring_enqueue_bulk(nfs[nf_id].tx_q, (void **)nf_buf->buf, nf_buf->cnt, NULL);
	if (unlikely(nf_buf->cnt > 0 && ret == 0)) {
		nfs[nf_id].stats.tx_drop += nf_buf->cnt;
		d2sc_pkt_drop_batch(nf_buf->buf, nf_buf->cnt);
	} else {
		nfs[nf_id].stats.tx += nf_buf->cnt;
	}
	nf_buf->cnt = 0;
}


/****************************Internal functions*******************************/


static inline void d2sc_pkt_process_next_act(struct buf_queue *mgr_bq, struct rte_mbuf *pkt, struct d2sc_nf *nf) {
	struct d2sc_flow_entry *entry;
	struct d2sc_sc *sc;
	struct d2sc_pkt_meta *meta;
	int ret;
	
	if (mgr_bq == NULL || pkt == NULL || nf == NULL)
		return;
		
	meta = d2sc_get_pkt_meta(pkt);
	ret = d2sc_fs_get_entry(pkt, &entry);
	if (ret >= 0) {
		sc = entry->sc;
		meta->act = d2sc_sc_next_act(sc, pkt);
		meta->dst = d2sc_sc_next_dst(sc, pkt);
	} else {
		meta->act = d2sc_sc_next_act(default_sc, pkt);
		meta->dst = d2sc_sc_next_dst(default_sc, pkt);
	}
	
	switch (meta->act) {
		case D2SC_NF_ACT_DROP:
			nf->stats.act_drop += !d2sc_pkt_drop(pkt);
			break;
		case D2SC_NF_ACT_TONF:
			nf->stats.act_tonf++;
			d2sc_pkt_enqueue_bq(mgr_bq, meta->dst, pkt);
			break;
		case D2SC_NF_ACT_OUT:
			nf->stats.act_out++;
			d2sc_pkt_enqueue_port(mgr_bq, meta->dst, pkt);
			break;
		default:
			break;
	}
	(meta->sc_index)++;
}


static inline void d2sc_pkt_enqueue_port(struct buf_queue *mgr_bq, uint16_t portid, struct rte_mbuf *pkt) {
	struct pkt_buf *port_buf;
	
	if (mgr_bq == NULL || pkt == NULL)
		return;
	
	if (portid >= ports->n_ports)
		return;
		
	port_buf = &mgr_bq->tx_thread->tx_bufs[portid];
	port_buf->buf[port_buf->cnt++] = pkt;
	if (port_buf->cnt == PKT_RD_SIZE) {
		d2sc_pkt_flush_port_bq(mgr_bq, portid);
	}
}


/*******************************Helper function*******************************/


void d2sc_pkt_drop_batch(struct rte_mbuf **pkts, uint16_t size) {
	uint16_t i;

	if (pkts == NULL)
		return;

	for (i = 0; i < size; i++)
		rte_pktmbuf_free(pkts[i]);
}


static int d2sc_pkt_drop(struct rte_mbuf *pkt) {
	rte_pktmbuf_free(pkt);
	if (pkt != NULL) {
		return 1;
	}
	return 0;
}