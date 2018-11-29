#!/bin/bash

function usage {
	echo "$0 CPU-LIST PORTMASK [-t NUM-NTS] [-d DEFAULT-NT] [-r NUM-RX-THREADS] [-s STATS-OUTPUT] [-z STATS-SLEEP-TIME]"
	# this works well on our 2x6-core nodes
	echo "$0 0,1,2,6 3 --> cores 0, 1, 2 and 6 with ports 0 and 1"
	echo -e "\tCores will be used as follows in numerical order:"
	echo -e "\t\tRX thread, TX thread, ..., TX thread for last NF, Stats thread"
	echo -e "$0 0,1,2,6 3 -s stdout"
	echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout"
	echo -e "$0 0,1,2,6 3 -r 10 -d 2"
	echo -e "\tRuns ONVM the same way as above, but limits max service IDs to 10 and uses service ID 2 as the default"
	exit 1
}

cpu=$1
ports=$2

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

shift 2 

if [ -z $ports ]
then 
	usage
fi

while getopts "t:d:r:s:z" opt; do
	case $opt in
		t) num_nts="-t $OPTARG";;
		d) def_nt="-d $OPTARG";;
		r) num_rx_threads="-r $OPTARG";;
		s) stats="-s $OPTARG";;
		z) stats_sleep_time="-z $OPTARG";;
		\?) echo "Unknown option -$OPTARG" && usage
		   ;;
		esac
done

sudo rm -rf /mnt/huge/rtemap_*
sudo $SCRIPTPATH/d2sc_mgr/$RTE_TARGET/d2sc_mgr -l $cpu -n 4 --proc-type=primary -- -p ${ports} ${num_nts} ${def_nt} ${num_rx_threads} ${stats} ${stats_sleep_time}
