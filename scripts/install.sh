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


# Print a table with environment variable locations
echo "----------------------------------------"
echo "D2SC Environment Variables:"
echo "----------------------------------------"
echo "RTE_SDK: $RTE_SDK"
echo "RTE_TARGET: $RTE_TARGET"
echo "D2SC_NUM_HUGEPAGES: $D2SC_NUM_HUGEPAGES"
echo "D2SC_SKIP_HUGEPAGES: $D2SC_SKIP_HUGEPAGES"
echo "D2SC_SKIP_FSTAB: $D2SC_SKIP_FSTAB"
echo "----------------------------------------"

if [ -z "$RTE_TARGET" ]; then
	echo "Please export \$RTE_TARGET. Or try running this without sudo."
	exit 1
fi

if [ -z "$RTE_SDK" ]; then
	echo "Please export \$RTE_SDK"
	exit 1
fi

# Validate sudo access
sudo -v

# Ensure we're working relative to the d2sc root directory
if [ $(basename $(pwd)) == "scripts" ]; then
	cd ..
fi

# Set state variable
start_dir=$(pwd)

# Compile dpdk
cd $RTE_SDK
echo "Compiling and installing DPDK in $RTE_SDK"
sleep 1
make config T=$RTE_TARGET
make T=$RTE_TARGET -j 8
make install T=$RTE_TARGET -j 8

# Refresh sudo
sudo -v

cd $start_dir

# Configure Hugepages only if user wants to
if [ -z "D2SC_SKIP_HUGEPAGES" ]; then
	hp_size=$(cat /proc/meminfo | grep Hugepagesize | awk '{print $2}')
	hp_count="${D2SC_NUM_HUGEPAGES:-1024}"
	echo "Configuring $hp_count hugepages with size $hp_size"
	sleep 1
	sudo mkdir -p /mnt/huge
fi

grep -m 1 "huge" /etc/fstab | cat
# Only add to /etc/fstab if user wants it
if [ ${PIPESTATUS[0]} != 0] && [ -z "$D2SC_SKIP_FSTAB" ]; then
	echo "Adding huge fs to /etc/fstab"
	sleep 1
	sudo sh -c "echo \"huge /mnt/huge hugetabfs defaults 0 0\" >> /etc/fstab"
fi

# Only mount hugepages if user wants to
if [ -z "$D2SC_SKIP_HUGEPAGES" ]; then
	echo "Mounting hugepages"
	sleep 1
	sudo mount -t hugetlbfs nodev /mnt/huge
	echo "Creating $hp_count hugepages"
	sleep 1
	sudo sh -c "echo $hp_count > /sys/devices/system/node/node0/hugepages/hugepages-${hp_size}kB/nr_hugepages"
fi

# Configure local environment
echo "Configuring environment"
sleep 1
scripts/setup_environment.sh

echo "D2SC INSTALL COMPLETED SUCCESSFULLY"