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
* ReplayGain compatible tagging support for MP3, OGG, Musepack and FLAC


Requirements
------------

The library itself has no requirements besides ANSI C.

The scanner needs libsndfile, libmpg123, FFmpeg or libmpcdec.

You need at least Python 2.4 and Mutagen for the ReplayGain tagging support.


Download
--------

* [Source (tar.gz)](libebur128-0.1.10-Source.tar.gz)
* [Source (zip)](libebur128-0.1.10-Source.zip)
* [Win32 build (zip)](libebur128-0.1.10-win32.zip)
* [Win32 SSE2 build (zip)](libebur128-0.1.10-win32-sse2.zip)
* [Win64 build (zip)](libebur128-0.1.10-win64.zip)
* [Linux32 build (tar.gz)](libebur128-0.1.10-Linux.tar.gz)
* [Linux32 SSE2 build (tar.gz)](libebur128-0.1.10-Linux-sse2.tar.gz)
* [Linux64 build (tar.gz)](libebur128-0.1.10-Linux64.tar.gz)


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

and it will scan the directory as one album. Use the option "-r" to search
recursively for music files and tag them as one album per subfolder. The tagger
also supports file input; then all files are treated as one album.

The reference volume is -18 LU (5 dB louder than the EBU R 128 reference level
of -23 LU).

All scanners support loudness range measurement with the command line
option "-r".

Use the options "-s" and "-m" to print short-term (3s) or momentary (0.4s)
loudness information to stdout. For example:

    r128-sndfile -m 0.1 foo.wav

to print the momentary loudness of foo.wav to stdout every 0.1s.

For examples how to use the library, see ebur128.h, minimal-example.c,
sndfile-example.c, mpg123-example, mpcdec-example.c and ffmpeg-example.c.
