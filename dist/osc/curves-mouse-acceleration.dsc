Format: 3.0 (quilt)
Source: curves-mouse-acceleration
Binary: curves-mouse-acceleration
Architecture: amd64
Version: 0.0.0
Maintainer: Frank Secilia <frank.secilia@gmail.com>
Standards-Version: 4.7.3.0
Homepage: https://github.com/fsecilia/curves
Build-Depends: debhelper-compat (= 13), cmake, dh-dkms, qt6-base-dev, linux-headers-generic | linux-headers-amd64,
               gcc (>= 4:14) | clang (>= 1:18), g++ (>= 4:14) | clang (>= 1:18)
Package-List:
 curves-mouse-acceleration deb misc optional arch=any
