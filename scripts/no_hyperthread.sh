#! /bin/bash
#                           D2SC
#             https://github.com/god14fei/d2sc
#
#   BSD LICENSE
#
#   Copyright(c)
#            2018-2019 Huazhong University of Science and Technology
#
#   All rights reserved.
#
#

echo "Disabling hyperthreading..."

CPUS_TO_SKIP=" $(cat /sys/devices/system/cpu/cpu*/topology/thread_siblings_list | sed 's/[^0-9].*//' | sort | uniq | tr "\r\n" "  ") "

# Disable Hyperthreading
for CPU_PATH in /sys/devices/system/cpu/cpu[0-9]*; do
	CPU="$(echo $CPU_PATH | tr -cd "0-9")"
	echo "$CPUS_TO_SKIP" | grep " $CPU " > /dev/null
	if [ $? -ne 0 ]; then
		echo 0 > $CPU_PATH/online
	fi
done

lscpu | grep -i -E  "^CPU\(s\):|core|socket"