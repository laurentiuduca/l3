## l3soc
* 10klut
* sparcv8, grlib, no grmon
* microsd, fast ethernet
* linux 6.13, gaisler-buildroot-2025.02-1.1
* gcc-14.4.0 binutils-2.46.1 uClibc-ng-1.0.58
* nvc vhdl and verilog mixed simulation 
* qmtech wukong v3 $120 fpga board

## 1. Quick start
The wukong board has the same ddr3 memory as digilent arty.
That's why we modified the arty's folders in grlib.

### 1.1 Workspace
#### grlib
```
tar -xf grlib-gpl-2026.3-b4301.tar.gz
cp grlib/grlib-gpl-2026.3-b4301 . -r
```
#### gcc
<!-- cat sparc-v8-linux6.13.tar.xz | split -b 40m - sparc-v8-linux6.13.tar.xz. -->
```
cat sparc-v8-linux6.13.tar.xz.a* | tar -xJf -
```
Add the full path to sparc-v8-linux6.13/bin to the PATH variable
#### buildroot 
```
tar -xf gaisler-buildroot-2025.02-1.1.tar.bz2
cp buildroot/gaisler-buildroot-2025.02-1.1 . -r
```

### 1.2. Build
#### buildroot
```
cd gaisler-buildroot-2025.02-1.1
make -j5
```
After linux kernel error
```
cp ../buildroot/gaisler-buildroot-2025.02-1.1/output/build/linux-6.13.12/.config output/build/linux-6.13.12/
make -j5
cd -
```
#### grlib
Replace in bin/Makefile paths that start with '/home/laur/' with your paths
or ignore if you do not want to use ghdl or xsim
```
cd grlib-gpl-2026.3-b4301/designs/
./run-sparc.sh
```

### 1.3 Simulate
Install nvc simulator
Every time you modify a vhd or verilog source
```
make nvc
make nvc-run
```

### 1.4 Generate bit file
```
make vivado
```
#### write microsd (replace /dev/sdb with your microsd)
```
dd if=sparc.bin of=/dev/sdb bs=512 seek=0 && sync
```

## 2. Compile gcc
### 2.1 Environment variables
Please replace /home/laur/rtos with your folder
```
mkdir /home/laur/rtos/sparc-v8/sparc-v8-linux6.13
export PATH="$PATH:/home/laur/rtos/sparc-v8/sparc-v8-linux6.13/bin"
export TOOLS=/home/laur/rtos/sparc-v8/sparc-v8-linux6.13
```

2.2 Build binutils
download from http://www.gnu.org/software/binutils
```
mkdir bbinutils
cd bbinutils
../binutils-2.46.1/configure --prefix=${TOOLS} --target=sparc-leon3-linux-uclibc --with-sysroot=${TOOLS} --disable-nls --disable-multilib --with-cpu=v8 --with-float=soft --disable-libssp 
make
make install
cd -
```
2.3 Build linux headers
download from kernel.org
```
cd linux-6.13.12
make mrproper
make ARCH=sparc INSTALL_HDR_PATH=${TOOLS} headers_install
cd -
```
2.4 gcc pass 1 - without libc
download from gcc.gnu.org
```
cd ../gcc-14.4.0/
./contrib/download_prerequisites
mkdir -p ../bgcc
cd ../bgcc
../gcc-14.4.0/configure --prefix=${TOOLS} --target=sparc-leon3-linux-uclibc --disable-nls --disable-shared --disable-multilib --disable-libgomp --disable-libmudflap --disable-libssp --disable-threads --enable-languages=c --with-cpu=v8 --with-float=soft
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

2.5 Configure and build uclibc
download from uclibc.org
```
tar xjvf uClibc-1.0.58.tar.bz2
cd uClibc-1.0.58
make menuconfig
```

```
make
make PREFIX=${TOOLS} install
```

## 3. Build mig file
We have already built the mig file; the step-by-step tutorial is in the migpng folder.
We included mig.dcp in the build in order to save time,
but if you want to rebuild it from scratch when building the bit file,
please modify grlib/grlib-gpl-2026.3-b4301/designs/leon3-digilent-arty-a7/xilinx_ips/mig.tcl
with mig.tclo
 
