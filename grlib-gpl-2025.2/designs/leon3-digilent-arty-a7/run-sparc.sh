set -x
set -e

#make soft
sparc-gaisler-elf-gcc -nostdlib -Tlinkprom -Ttext=0x44000000 systest.c laur.c -o systest.exe

# ramsize in k
image=../../../gaisler-buildroot-2025.02-1.1/output/images/image.ram
mkprom2 -msoft-float -freq 83 -baud 38400 -v -rmw -romsize 0x8000 -ramsize 0x18000 -rstaddr 0x40000000 -dump $image
#mkprom2 -msoft-float -freq 83 -baud 38400 -v -rmw -romsize 0x8000 -ramsize 0x18000 -rstaddr 0x40000000 -dump systest.exe
cp prom.out prom.exe
#make ahbrom.vhd
sparc-gaisler-elf-objcopy -O srec --gap-fill 0 --set-section-flags .bss=alloc,contents,load prom.exe ram.srec
#qemu-system-sparc -nographic -M leon3_generic -m 128M -kernel prom.exe
./q.sh

sparc-gaisler-elf-objcopy -O binary prom.out sparc.bin
#make run-sparc
gcc main-sparc.c -o main-sparc.out
./main-sparc.out sparc.bin

