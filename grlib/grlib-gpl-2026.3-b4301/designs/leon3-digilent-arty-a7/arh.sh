set -x
set -e
export brhome=../../../gaisler-buildroot-2025.02-1.1
tar -czf ../../../l3.tgz ../../bin/Makefile ../leon3-digilent-arty-a7 ../../boards/digilent-arty-a7 ../../lib/work/sd ../../../mkprom2/src $brhome/output/build/mklinuximg-2.0.20/src $brhome/output/build/mklinuximg-2.0.20/include $brhome/output/build/linux-6.13.12/.config $brhome/.config $brhome/output/build/linux-6.13.12/drivers/net/ethernet/aeroflex/greth_main.c


