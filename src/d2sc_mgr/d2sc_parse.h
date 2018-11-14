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

                                 d2sc_parse.h
                                 
    Header file with functions for parsing DPDK and D2SC command-line arguments
    
******************************************************************************/

#ifndef _D2SC_parse_H_
#define _D2SC_parse_H_

#include "getopt.h"

#include "d2sc_includes.h"
#include "d2sc_mgr/d2sc_init.h"

#define DEFAULT_NT_ID 1

int parse_mgr_args(uint8_t n_ports, int argc, char *argv[]);

#endif 

