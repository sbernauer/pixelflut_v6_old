
Start in dpdk-xx.xx/

## Set up hugepages
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge


## Build dpdk
make config T=x86_64-native-linux-gcc
make install T=x86_64-native-linux-gcc


## Bind nic
usertools/dpdk-devbind.py --status
modprobe uio_pci_generic
ip link set dev eno1 down
usertools/dpdk-devbind.py --bind=uio_pci_generic 0000:00:19.0


## Build skeleton
cd examples/skeleton/
export RTE_SDK=/home/sbernauer/Desktop/pixelflut_v6/DPDK/dpdk-19.05
export RTE_TARGET=x86_64-native-linux-gcc
make
### And run
sudo build/app/basicfwd -l 0
Important: run as root, otherwise "EAL: Probing VFIO support... EAL: Cannot obtain physical addresses: No such file or directory. Only vfio will function."
Warning about missing hugepages is normal.
If only one nic bound the program won't start, that is no problem
cd ..


## Build pixelflut
cd examples/pixelflut/
export RTE_SDK=/home/sbernauer/Desktop/pixelflut_v6/DPDK/dpdk-19.05
export RTE_TARGET=x86_64-native-linux-gcc
make

### And run

