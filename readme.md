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

### 2.2 Build binutils
download from http://www.gnu.org/software/binutils
```
mkdir bbinutils
cd bbinutils
../binutils-2.46.1/configure --prefix=${TOOLS} --target=sparc-leon3-linux-uclibc --with-sysroot=${TOOLS} --disable-nls --disable-multilib --with-cpu=v8 --with-float=soft --disable-libssp 
make
make install
cd -
```
### 2.3 Build linux headers
download from http://kernel.org
```
cd linux-6.13.12
make mrproper
make ARCH=sparc INSTALL_HDR_PATH=${TOOLS} headers_install
cd -
```
### 2.4 gcc pass 1 - without libc
download from http://gcc.gnu.org
```
cd gcc-14.4.0/
./contrib/download_prerequisites
mkdir -p ../bgcc
cd ../bgcc
../gcc-14.4.0/configure --prefix=${TOOLS} --target=sparc-leon3-linux-uclibc --disable-nls --disable-shared --disable-multilib --disable-libgomp --disable-libmudflap --disable-libssp --disable-threads --enable-languages=c --with-cpu=v8 --with-float=soft
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

### 2.5 Configure and build uclibc
download from http://uclibc.org
```
tar xjvf uClibc-1.0.58.tar.bz2
cd uClibc-1.0.58
cp ../uclibc-ng-1.0.58.config .config
```
Some necessary menuconfig options
```
make menuconfig
Target Architecture: sparc
Target Options:
  Processor type: SPARC v8
  Target has MMU: yes
  Utilize MMU: yes
  Enable floating point: yes
  Target has FPU: no
  Enable C99 math: yes
  Symbol: UCLIBC_HAS_FENV [=n] 
     [ ] Enable C99 Floating-point environment 
  KERNEL_HEADERS: /home/laur/rtos/sparc-v8/sparc-v8-linux6.13/include
General Library Settings:
  Thread support (Native POSIX Threading (NPTL))
  SUSv3 legacy functions: yes
  SUSv3 legacy macros: yes
  SUSv4 legacy functions: yes
Networking:
  IPv6: yes
  RPC support: yes
  Full RPC: no
  DNS resolver functions: yes
String support:
  ctype argument checking: detect and handle
Installation options:
  RUNTIME_PREFIX: /
  DEVEL_PREFIX: /
  HARDWIRED_ABSPATH: no
Development and debugging options
  CROSS_COMPILER_PREFIX: sparc-leon3-linux-uclibc-
```
```
make
make PREFIX=${TOOLS} install
```

### 2.6 gcc pass 2
```
mkdir ../sparc-v8-linux6.13/usr
cp ../sparc-v8-linux6.13/include ../sparc-v8-linux6.13/usr -r
../gcc-14.4.0/configure --prefix=${TOOLS} --target=sparc-leon3-linux-uclibc --with-sysroot=${TOOLS} --with-build-sysroot=${TOOLS} --with-cpu=v8 --with-float=soft --with-gnu-as --with-gnu-ld --disable-nls --disable-multilib --enable-languages=c,c++ --enable-shared --enable-threads=posix --disable-libsanitizer --disable-libgomp --disable-libquadmath --disable-libssp --disable-libvtv --disable-libitm --disable-libmudflap --with-default-libc=uclibc
make -j4
make install-strip
```

### 2.7 test
```
cd /tmp
echo 'int main(){}' > t.c
sparc-leon3-linux-gcc-uclibc t.c -o t
readelf -l t | grep interpreter
```
[Requesting program interpreter: /lib/ld-uClibc.so.



## 3. Build mig file
We have already built the mig file; the step-by-step tutorial is in the migpng folder.
We included mig.dcp in the build in order to save time,
but if you want to rebuild it from scratch when building the bit file,
please modify grlib/grlib-gpl-2026.3-b4301/designs/leon3-digilent-arty-a7/xilinx_ips/mig.tcl
with mig.tclo
 
