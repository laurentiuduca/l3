
cat sparc-v8-linux6.13.tar.xz | split -b 40m - sparc-v8-linux6.13.tar.xz.
cat sparc-v8-linux6.13.tar.xz.a* | tar -xJvf -
