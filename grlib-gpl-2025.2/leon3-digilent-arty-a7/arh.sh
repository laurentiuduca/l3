set -x
set -e
export brhome=/home/laur/downloads/gaisler-buildroot-2025.02-1.1
tar -czf /home/laur/downloads/leon3.tgz ../leon3-digilent-arty-a7 ../../boards/digilent-arty-a7 ../../lib/work/sd /home/laur/rtos/mkprom2/src $brhome/output/build/mklinuximg-2.0.20/src $brhome/output/build/mklinuximg-2.0.20/include $brhome/output/build/linux-6.13.12/.config $brhome/.config $brhome/output/build/linux-6.13.12/drivers/net/ethernet/aeroflex/greth_main.c


