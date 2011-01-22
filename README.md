libebur128
==========

libebur128 is a library that implements the EBU R 128 standard for loudness
normalisation.
There are also several scanners using different libraries for audio input.
It is redistributed under the MIT license. See LICENSE file for details.

Features
--------

* Portable ANSI C code
* Implements M, S and I modes
* Implements loudness range measurement (EBU - TECH 3342)
* Supports all samplerates by recalculation of the filter coefficients
* ReplayGain-compatible tagging support for MP3, OGG, Musepack and FLAC


Requirements
------------

The library itself has no requirements besides ANSI C.
The scanner needs libsndfile, libmpg123, FFmpeg or libmpcdec.
You need Python and Mutagen for the ReplayGain tagging support.


Installation
-----------

In the root folder, type:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make


It is also possible to compile the scanners directly with gcc:

    gcc -O3 -I../include sndfile-example.c ebur128.c -o r128-test -lm -lsndfile


Usage
-----

Run r128-sndfile, r128-ffmpeg, r128-mpg123 or r128-musepack with the files you
want to scan as arguments.
The output will look like this:

    segment 1: -9.8 LUFS
    segment 2: -9.9 LUFS
    segment 3: -10.6 LUFS
    segment 4: -11.9 LUFS
    segment 5: -10.9 LUFS
    segment 6: -12.9 LUFS
    segment 7: -10.7 LUFS
    segment 8: -11.9 LUFS
    segment 9: -10.9 LUFS
    segment 10: -12.8 LUFS
    segment 11: -12.1 LUFS
    segment 12: -15.4 LUFS
    segment 13: -13.7 LUFS
    segment 14: -11.6 LUFS
    global loudness: -11.3 LUFS


If you have Python and Mutagen (a tagging library) installed, the scanners also
support ReplayGain tagging with a little helper script called "r128-tag". Just
run the script like this:

    r128-tag.py <directory>

and it will scan the directory recursively for music files and tag them as one
album per subfolder.

The reference volume is -18 LU (5 dB louder than the EBU R 128 reference level
of -23 LU).
All scanners support loudness range measurement with the command line
option "-r".

For examples how to use the library, see ebur128.h, minimal-example.c,
sndfile-example.c, mpg123-example, mpcdec-example.c and ffmpeg-example.c.






Used libraries / compile options
--------------------------------

libebur128 (lsb 4.0 64 bit):
CC="lsbrun /opt/lsb/bin/lsbcc --lsb-besteffort \
--lsb-shared-libpath=/home/jan/r128/lib64:/home/jan/r128/lib64/ffmpeg-0.6.1" \
cmake .. -DCMAKE_BUILD_TYPE=Release

libebur128 (lsb 4.0 32 bit):
CC="lsbrun32 /opt/lsb/bin/lsbcc --lsb-besteffort \
--lsb-shared-libpath=/home/jan/r128/lib:/home/jan/r128/lib/ffmpeg-0.5.3 -m32" \
cmake .. -DCMAKE_BUILD_TYPE=Release

FFmpeg (0.6.1 - win64):
./configure --enable-memalign-hack --target-os=mingw32 --arch=x86_64 \
            --cross-prefix=x86_64-w64-mingw32- --enable-shared --disable-dxva2 \
            --disable-zlib --disable-bzlib

FFmpeg (0.6.1 - win32):
./configure --enable-memalign-hack --arch=x86 --target-os=mingw32 \
            --cross-prefix=i486-mingw32- --enable-shared \
            --disable-zlib --disable-bzlib

FFmpeg (0.6.1 - lsb 4.0 64 bit):
./configure --enable-shared --disable-static \
--disable-avdevice --disable-swscale --disable-avfilter --disable-everything \
--cc="lsbrun /opt/lsb/bin/lsbcc --lsb-verbose --lsb-besteffort \
-DPRId64='\"ld\"' -DPRIx64='\"lx\"' -DPRIu64='\"lu\"' -DPRIdFAST16='\"ld\"' \
-DPRIdFAST32='\"ld\"' -DPRIx8='\"x\"'"

FFmpeg (0.6.1 - lsb 4.0 32 bit):
./configure --enable-shared --disable-static \
--disable-avdevice --disable-swscale --disable-avfilter --disable-everything \
--cc="lsbrun32 /opt/lsb/bin/lsbcc --lsb-verbose --lsb-besteffort -m32 \
-DPRId64='\"lld\"' -DPRIx64='\"llx\"' -DPRIu64='\"llu\"' -DPRIdFAST16='\"d\"' \
-DPRIdFAST32='\"d\"' -DPRIx8='\"x\"'"

FFmpeg (0.5.3 - lsb 4.0 64 bit):
./configure --enable-shared --disable-static \
--disable-swscale --disable-avfilter --disable-encoders --disable-decoders \
--disable-muxers --disable-demuxers --disable-parsers --disable-bsfs \
--cc="lsbrun /opt/lsb/bin/lsbcc --lsb-verbose --lsb-besteffort \
-DPRId64='\"ld\"' -DPRIx64='\"lx\"' -DPRIu64='\"lu\"' -DPRIdFAST16='\"ld\"' \
-DPRIdFAST32='\"ld\"' -DPRIx8='\"x\"'"

FFmpeg (0.5.3 - lsb 4.0 32 bit):
./configure --enable-shared --disable-static \
--disable-swscale --disable-avfilter --disable-encoders --disable-decoders \
--disable-muxers --disable-demuxers --disable-parsers --disable-bsfs \
--cc="lsbrun32 /opt/lsb/bin/lsbcc --lsb-verbose --lsb-besteffort -m32 \
-DPRId64='\"lld\"' -DPRIx64='\"llx\"' -DPRIu64='\"llu\"' -DPRIdFAST16='\"d\"' \
-DPRIdFAST32='\"d\"' -DPRIx8='\"x\"'"

libmpcdec (r435 - win64):
cmake .. -DCMAKE_TOOLCHAIN_FILE=x86_64-w64-mingw32.cmake \
         -DCMAKE_BUILD_TYPE=Release

libmpcdec (r435 - win32):
cmake .. -DCMAKE_TOOLCHAIN_FILE=i486-mingw32.cmake \
         -DCMAKE_BUILD_TYPE=Release

libmpcdec (r435 - linux & lsb 4.0 64 bit):
cmake .. -DCMAKE_BUILD_TYPE=Release

libmpcdec (r435 - lsb 4.0 32 bit):
CC="gcc -m32" cmake .. -DCMAKE_BUILD_TYPE=Release

mpg123 (1.13.1 - win32/win64): precompiled binaries

mpg123 (1.13.1 - lsb 4.0 64 bit):
CC="lsbrun /opt/lsb/bin/lsbcc --lsb-besteffort" ./configure

mpg123 (1.13.1 - lsb 4.0 32 bit):
CC="lsbrun32 /opt/lsb/bin/lsbcc --lsb-besteffort -m32" ./configure \
--with-cpu=generic_fpu

libsndfile (1.0.23 - win32/win64): precompiled binaries

libsndfile (1.0.23 - lsb 4.0 64 bit):
CC="lsbrun /opt/lsb/bin/lsbcc --lsb-besteffort \
-DPRId64='\"ld\"' -DPRIx64='\"lx\"'" \
./configure --disable-sqlite --disable-external-libs --disable-alsa

libsndfile (1.0.23 - linux 32 bit):
CXX="g++ -m32" CC="gcc -m32 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" \
./configure --disable-sqlite --disable-external-libs --disable-alsa
