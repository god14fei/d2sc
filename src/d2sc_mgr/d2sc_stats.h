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
                                   d2sc_stats.h
      Header file containing all function declarations related to statistis
      display.

******************************************************************************/

#ifndef _D2SC_STATS_H_
#define _D2SC_STATS_H_

#define D2SC_STR_STATS_STDOUT "stdout"
#define D2SC_STR_STATS_STDERR "stderr"

typedef enum {
	D2SC_STATS_NONE = 0,
	D2SC_STATS_STDOUT,
	D2SC_STATS_STDERR
} D2SC_STATS_OUTPUT;

/*********************************Interfaces**********************************/


/*
 * Interface called by the manager to tell the stats module where to print
 * You should only call this once
 *
 * Input: a STATS_OUTPUT enum value representing output destination.  If
 * STATS_NONE is specified, then stats will not be printed to the console or web
 * browser.  If STATS_STDOUT or STATS_STDOUT is specified, then stats will be
 * output the respective stream.
 */
void d2sc_stats_set_output(D2SC_STATS_OUTPUT stats_dst);


/*
 * Interface called by the D2SC Manager to display all statistics
 * available.
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
void d2sc_stats_display_all(unsigned stime);


/*
 * Interface called by the D2SC Manager to clear all NFs statistics
 * available.
 *
 * Note : this function doesn't use d2sc_stats_clear_nf for each nf,
 * since with a huge number of NFs, the additional functions calls would
 * incur a visible slowdown.
 *
 */
void d2sc_stats_clear_all_nfs(void);


/*
 * Interface called by the D2SC Manager to clear one NF's statistics.
 *
 * Input : the NF id
 *
 */
void d2sc_stats_clear_nf(uint16_t id);


#endif	// _D2SC_STATS_H_