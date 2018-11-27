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

set -e

DPDK_DEVBIND=$RTE_SDK/usertools/dpdk-devbind.py 

# Confirm environment variables
if [ -z "$RTE_TARGET" ]; then
	echo "Please export \$RTE_TARGET"
	exit 1
fi

if [ -z "$RTE_SDK" ]; then
	echo "Please export \$RTE_SDK"
	exit 1
fi

# Ensure we're working relative to the d2sc root directory
if [ $(basename $(pwd)) == "scripts" ]; then
	cd ..
fi

start_dir=$(pwd)

if [ -z "$D2SC_HOME" ]; then
	echo "Please export \$D2SC_HOME and set it to $start_dir"
	exit 1
fi

# Disable ASLR
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

# Septup/Check for free Hugepages if the user wants to
if [ -z "$D2SC_SKIP_HUGEPAGES" ]; then
	hp_size=$(cat /proc/meminfo | grep Hugepagesize | awk '{print $2}')
	hp_count="${D2SC_NUM_HUGEPAGES:-1024}"
	
	sudo sh -c "echo $hp_count > /sys/devices/system/node/node0/hugepages/hugepages-${hp_size}kB/nr_hugepages"
	hp_free=$(cat /proc/meminfo | grep HugePages_Free | awk '{print $2}')
	if [ $hp_free == "0" ]; then
		echo "No free hugepages. Did you try turning it off and on again?"
		exit 1
	fi
fi

# Verify sudo access
sudo -v

# Load uio kernel modules
grep -m 1 "igb_uio" /proc/modules | cat
if [ ${PIPESTATUS[0]} != 0 ]; then
	echo "Loading uio kernel modules"
	sleep 1
	cd $RTE_SDK/$RTE_TARGET/kmod
	sudo modprobe uio
	sudo insmod igb_uio.ko
else
	echo "IGB UIO module already loaded."
fi

echo "Checking NIC status"
sleep 1
$DPDK_DEVBIND --status

echo "Binding NIC status"
if [ -z "$D2SC_NIC_PCI" ]; then
	for id in $($DPDK_DEVBIND --status | grep -v Active | grep -e "10G" -e "10-Gigabit" | grep unused=igb_uio | cut -f 1 -d " ")
	do
		read -r -p "Bind interface $id to DPDK? [y/N] " response
		if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]]; then
			echo "Binding $id to dpdk"
			sudo $DPDK_DEVBIND -b igb_uio $id 
		fi
	done
else
	for nic_id in $D2SC_NIC_PCI
	do
		echo "Binding $nic_id to DPDK"
		sudo $DPDK_DEVBIND -b igb_uio $nic_id
	done
fi

echo "Finished Binding"
$DPDK_DEVBIND --status

sudo bash $D2SC_HOME/scripts/no_hyperthread.sh

echo "Environment setup complete."
