cmd_d2sc_init.o = gcc -Wp,-MD,./.d2sc_init.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/feishen/d2sc/src/d2sc_mgr/build/include -I/home/feishen/d2sc/dpdk/x86_64-native-linuxapp-gcc/include -include /home/feishen/d2sc/dpdk/x86_64-native-linuxapp-gcc/include/rte_config.h -g -I/home/feishen/d2sc/src/d2sc_mgr/../ -I/home/feishen/d2sc/src/d2sc_mgr/../d2sc_nfrt/    -o d2sc_init.o -c /home/feishen/d2sc/src/d2sc_mgr/d2sc_init.c 
