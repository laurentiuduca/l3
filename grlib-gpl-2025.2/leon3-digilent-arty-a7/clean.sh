set -x
rm -rf xdump.s dump.s ram.srec prom.srec
rm -rf init_disk.txt  init_kernel.txt  initmem.bin sparc.bin \
     initmem_gen2/init_disk.txt initmem_gen2/init_kernel.txt initmem_gen2/initmem.bin initmem_gen2/main-sparc.out initmem_gen2/sparc.bin
./xclean.sh
make soft-clean
make clean

