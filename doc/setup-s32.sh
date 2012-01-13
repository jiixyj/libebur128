#!/bin/bash

set -e
cd

zypper -n install vim curl zsh cmake yasm unzip tar

curl -O http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_11.4/windows:mingw:win32.repo
mv windows\:mingw\:win32.repo /etc/zypp/repos.d/
zypper --gpg-auto-import-keys update

zypper -n install --auto-agree-with-licenses mingw32-cross-gcc-c++ mingw32-cross-gcc mingw32-cross-pkg-config mingw32-libsndfile mingw32-libsndfile-devel mingw32-taglib mingw32-taglib-devel mingw32-glib2-devel mingw32-libqt4-devel mingw32-libspeex-devel mingw32-libspeex


tar xf libmpc-0.1~r459.tar
cd libmpc-0.1~r459
CFLAGS='-O0 -g' ./configure --prefix=/usr/i686-w64-mingw32/sys-root/mingw/ --host=i686-w64-mingw32 --target=i686-w64-mingw32
make install
cd

tar xf ffmpeg-0.8.7.tar.bz2
cd ffmpeg-0.8.7
#./libavcodec/x86/fmtconvert_mmx.c
sed '28i\
#undef HAVE_YASM' ./libavcodec/x86/fmtconvert_mmx.c > new
mv new ./libavcodec/x86/fmtconvert_mmx.c
./configure --prefix=/usr/i686-w64-mingw32/sys-root/mingw/ --enable-runtime-cpudetect --target-os=mingw32 --arch=x86 --cross-prefix=i686-w64-mingw32- --enable-shared
make install
cd

unzip mpg123-1.13.4-x86.zip
cd mpg123-1.13.4-x86
chmod 644 libmpg123-0.dll mpg123.h
cp libmpg123-0.dll /usr/i686-w64-mingw32/sys-root/mingw/bin
cp mpg123.h /usr/i686-w64-mingw32/sys-root/mingw/include
cd
