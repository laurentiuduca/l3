# l3soc
* 10klut
* sparcv8
* linux 6.13, gaisler-buildroot-2025.02-1.1
* gcc-14.4.0 binutils-2.46.1 uClibc-ng-1.0.58
* nvc vhdl and verilog mixed simulation 

## Quick start
### Workspace
#### grlib
tar -xf grlib-gpl-2026.3-b4301.tar.gz
cp grlib/grlib-gpl-2026.3-b4301/* grlib-gpl-2026.3-b4301/ -r
#### gcc
<!-- cat sparc-v8-linux6.13.tar.xz | split -b 40m - sparc-v8-linux6.13.tar.xz. -->
cat sparc-v8-linux6.13.tar.xz.a* | tar -xJvf -


