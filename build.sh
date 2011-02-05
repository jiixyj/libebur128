#!/bin/sh
rm -r build/*
rm -r build32/*
rm -r build32-sse2/*
rm -r mingw/*
rm -r mingw-sse2/*
rm -r mingw64/*

cd build
CC="lsbrun /opt/lsb/bin/lsbcc --lsb-besteffort \
--lsb-shared-libpath=/home/jan/r128/lib64:/home/jan/r128/lib64/ffmpeg-0.6.1" \
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake ..
make
cmake .. -DFFMPEG_VERSION=0.5.3
make
find r128* -exec chrpath -d {} \;
make package
make package_source
cd ..

mv /opt/lsb /opt/lsb64
mv /opt/lsb32 /opt/lsb
cd build32
CC="lsbrun32 /opt/lsb/bin/lsbcc --lsb-besteffort \
--lsb-shared-libpath=/home/jan/r128/lib:/home/jan/r128/lib/ffmpeg-0.6.1 -m32" \
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake ..
make
cmake .. -DFFMPEG_VERSION=0.5.3
make
find r128* -exec chrpath32 -d {} \;
make package
cd ..
cd build32-sse2
CC="lsbrun32 /opt/lsb/bin/lsbcc --lsb-besteffort \
--lsb-shared-libpath=/home/jan/r128/lib:/home/jan/r128/lib/ffmpeg-0.6.1 -m32" \
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SSE2=ON
cmake ..
make
cmake .. -DFFMPEG_VERSION=0.5.3
make
find r128* -exec chrpath32 -d {} \;
make package
cd ..
mv /opt/lsb /opt/lsb32
mv /opt/lsb64 /opt/lsb

mv include/sndfile.h include/sndfile.hh
cd mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/wine32.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake ..
make
make package
cd ../mingw-sse2
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/wine32.cmake \
         -DCMAKE_BUILD_TYPE=Release -DUSE_SSE2=ON
cmake ..
make
make package
cd ../mingw64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/wine.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake ..
make
make package
cd ..
mv include/sndfile.hh include/sndfile.h

cp build/libebur128-*-Linux64.tar.gz .
cp build/libebur128-*-Source.tar.gz .
cp build/libebur128-*-Source.zip .
cp build32/libebur128-*-Linux.tar.gz .
cp build32-sse2/libebur128-*-Linux-sse2.tar.gz .
cp mingw/libebur128-*-win32.zip .
cp mingw-sse2/libebur128-*-win32-sse2.zip .
cp mingw64/libebur128-*-win64.zip .
