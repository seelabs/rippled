# VCPKG Branch

This is an experimental branch. It uses vcpkg to install rippled dependencies.
The modivation for doing so is to more easily introduce new dependencies into
rippled without compilcating the build process.

# Install rippled's vcpkg repository

Rippled currently requires patched versions of some dependencies - in particular
boost and soci. Even if you already have vcpkg installed on your system, for now
we need to use a custom one for rippled.

```
git clone --recursive https://github.com/seelabs/vcpkg-rippled.git
cd vcpkg-rippled
./bootstrap-vcpkg.sh -disableMetrics
```

Optionally add the `vcpkg` executable to the path. I do so by adding a symbolic
link to `vcpkg` in my `~/bin` directory. If `vcpkg` was already installed,
rename this `vcpkg` to `vcpkg-rippled` or somesuch.

# Install the dependencies

```
vcpkg-rippled install boost libarchive lz4 openssl rocksdb 'soci[sqlite3]' sqlite3 zlib
```

# Build rippled

To use vcpkg, the `vcpkg.cmake` toolchain file must be added to the build. On my system, I need to add:
```
-DCMAKE_TOOLCHAIN_FILE=/home/swd/projs/packages/vcpkg-rippled/scripts/buildsystems/vcpkg.cmake
```
Of course, the path should be adjusted to where the file is installed on your local system.

Currently, I get build errors with non-static configurations (hopefully I can fixing this). For now, disable static builds with:
```
-Dstatic=Off
```

For reference, here's the cmake command I use locally:
```
cmake -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache -Dunity=On -DCMAKE_C_COMPILER=/home/swd/apps/gcc-latest/bin/gcc -DCMAKE_CXX_COMPILER=/home/swd/apps/gcc-latest/bin/g++ -DCMAKE_BUILD_TYPE=Debug      -DCMAKE_TOOLCHAIN_FILE=/home/swd/projs/packages/vcpkg-rippled/scripts/buildsystems/vcpkg.cmake -Dlocal_protobuf=On -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -GNinja ../..
```
