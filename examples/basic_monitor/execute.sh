#!/bin/bash

function usage {
        echo "$0 CPU-LIST NT-ID [-p PRINT] [-n NF-ID]"
        echo "$0 3 0 --> core 3, NT ID 0"
        echo "$0 3,7,9 1 --> cores 3,7, and 9 with NT ID 1"
        echo "$0 -p 1000 -n 6 3,7,9 1 --> cores 3,7, and 9 with NT ID 1 and Print Rate of 1000 and instance ID 6"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

cpu=$1
nt=$2

shift 2 

if [ -z $nt ]
then
	usage
fi

while getopts ":p:n" opt; do
	case $opt in
		p) print="-p $OPTARG";;
		n) inst="-n $OPTARG";;
		\?) echo "Unknown option -$OPTARG" && usage
		;;
	esac
done

exec sudo $SCRIPTPATH/build/app/monitor -l $cpu -n 3 --proc-type=secondary -- -t $nt $inst -- $print
