libebur128
==========

libebur128 is a library that implements the EBU R 128 standard for loudness
normalisation.
There is also a scanner using libsndfile for audio input.
It is redistributed under the MIT license. See LICENSE file for details.

Features
--------

* Portable ANSI C code
* Implements M, S and I modes
* Supports all samplerates by recalculation of the filter coefficients


Requirements
------------

The library itself has no requirements besides ANSI C.
The scanner needs libsndfile or ffmpeg (libavcodec/libavformat).


Installation
-----------

In the root folder, type:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make


It is also possible to compile the scanner directly with gcc:

    gcc -O3 -I../include sndfile-example.c ebur128.c -o r128-test -lm -lsndfile


Usage
-----

Run r128-sndfile or r128-ffmpeg with the files you want to scan as arguments.
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


For examples how to use the library, see minimal-example.c, sndfile-example.c
and ffmpeg-example.c.
