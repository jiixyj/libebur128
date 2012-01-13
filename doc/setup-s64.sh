#!/bin/bash

set -e
cd

zypper -n install vim curl zsh cmake yasm unzip tar

curl -O http://download.opensuse.org/repositories/windows:/mingw:/win64/openSUSE_11.4/windows:mingw:win64.repo
mv windows\:mingw\:win64.repo /etc/zypp/repos.d/
zypper --gpg-auto-import-keys update

zypper -n install --auto-agree-with-licenses mingw64-cross-gcc-c++ mingw64-cross-gcc mingw64-cross-pkg-config mingw64-libsndfile mingw64-libsndfile-devel mingw64-taglib mingw64-taglib-devel mingw64-glib2-devel mingw64-libqt4-devel mingw64-libspeex-devel mingw64-libspeex


tar xf libmpc-0.1~r459.tar 
cd libmpc-0.1~r459
CFLAGS='-O0 -g' ./configure --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ --host=x86_64-w64-mingw32 --target=x86_64-w64-mingw32
make install
cd

tar xf ffmpeg-0.8.7.tar.bz2
cd ffmpeg-0.8.7
./configure --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ --enable-runtime-cpudetect --target-os=mingw32 --arch=x86 --cross-prefix=x86_64-w64-mingw32- --enable-shared
make install
cd

unzip mpg123-1.13.4-x86-64.zip
cd mpg123-1.13.4-x86-64
chmod 644 libmpg123-0.dll mpg123.h
cp libmpg123-0.dll /usr/x86_64-w64-mingw32/sys-root/mingw/bin
cp mpg123.h /usr/x86_64-w64-mingw32/sys-root/mingw/include
cd
