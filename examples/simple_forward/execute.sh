#!/bin/bash

function usage {
	echo "$0 CPU-LIST TYPE-ID DST [-p PRINT] [-n NF-ID]"
	echo ""
	echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Type ID 1, and forwards to Type ID 2"
	echo "$0 3,7,9 1 2 1000 --> cores 3,7, and 9, with Type ID 1, forwards to Type ID 2,  and Print Rate of 1000"
	exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

if [ -z $1 ]
then
  usage
fi

cpu=$1
nt=$2
dst=$3

shift 3

if [ -z $dst ]
then
    usage
fi

while getopts ":p:n:" opt; do
	case $opt in
		p) print="-p $OPTARG";;
		n) inst="-n $OPTARG";;
		\?) echo "Unknown option -$OPTARG" && usage
		;;
	esac
done

exec sudo $SCRIPTPATH/build/app/forward -l $cpu -n 3 --proc-type=secondary -- -t $nt $inst -- -d $dst $print