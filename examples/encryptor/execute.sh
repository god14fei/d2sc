#!/bin/bash

function usage {
	echo "$0 CPU-LIST TYPE-ID DST [-p PRINT] [-n NF-ID]"
	echo "$0 3 0 1 --> core 3, type ID 0, and forwards to type 1"
	echo "$0 3,7,9 1 2 --> cores 3,7, and 9 with type ID 1, and forwards to type id 2"
	echo "$0 -p 1000 -n 6 3,7,9 1 2 --> cores 3,7, and 9 with type ID 1, Print Rate of 1000, instance ID 6, and forwards to type id 2"
	exit 1
}

SCRIPT=$(readlink -f "$0");
SCRIPTPATH=$(dirname "$SCRIPT");
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

exec sudo $SCRIPTPATH/build/app/encryptor -l $cpu -n 3 --proc-type=secondary -- -t $nt $inst -- -d $dst $print