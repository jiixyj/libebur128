libebur128
==========

libebur128 is a library that implements the EBU R 128 standard for loudness
normalisation.

There is also a loudness scanner using different libraries for audio input.

All source code is licensed under the MIT license. See LICENSE file for
details.

Features
--------

* Portable ANSI C code
* Implements M, S and I modes
* Implements loudness range measurement (EBU - TECH 3342)
* True peak scanning
* Supports all samplerates by recalculation of the filter coefficients
* ReplayGain compatible tagging support for MP3, OGG, Musepack and FLAC


Requirements
------------

The library itself has no requirements besides ANSI C.

The scanner needs taglib, libsndfile, libmpg123, FFmpeg and libmpcdec.


Download
--------

* [Source (tar.gz)](libebur128-0.3.2-Source.tar.gz)
* [Source (zip)](libebur128-0.3.2-Source.zip)
* [Win32 build (zip)](libebur128-0.3.2-win32.zip)
* [Win32 SSE2 build (zip)](libebur128-0.3.2-win32-sse2.zip)
* [Win64 build (zip)](libebur128-0.3.2-win64.zip)
* [Linux32 build (tar.gz)](libebur128-0.3.2-Linux.tar.gz)
* [Linux32 SSE2 build (tar.gz)](libebur128-0.3.2-Linux-sse2.tar.gz)
* [Linux64 build (tar.gz)](libebur128-0.3.2-Linux64.tar.gz)


Installation
-----------

In the root folder, type:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make


Usage
-----

Run r128-scanner with the files you want to scan as arguments. The scanner will
automatically choose the best input plugin for each file. You can force an
input plugin with the command line option "--force-plugin=PLUGIN", where PLUGIN
is one of sndfile, mpg123, musepack or ffmpeg.

The output will look like this:

    $ ./r128-scanner ~/music/bad\ loop\ -\ Luo/*.flac

    -12.8 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 01 Nio.flac
    -11.1 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 02 Eri Valeire.flac
    -10.1 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 03 Kauniit Ihmiset.flac
    -11.3 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 04 Mmin.flac
    -26.1 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 05 3b Or T.flac
    -14.1 LUFS, /home/jan/music/bad loop - Luo/bad loop - Luo - 06 Kannas Nsp.flac
    --------------------------------------------------------------------------------
    -11.8 LUFS

or with more options:

    $ ./r128-scanner -p dbtp -l ~/music/bad\ loop\ -\ Luo/*.flac

    -12.8 LUFS, LRA: 14.2 LU, true peak: -0.0 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 01 Nio.flac
    -11.1 LUFS, LRA: 8.3 LU, true peak: -0.1 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 02 Eri Valeire.flac
    -10.1 LUFS, LRA: 11.8 LU, true peak: -0.1 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 03 Kauniit Ihmiset.flac
    -11.3 LUFS, LRA: 11.8 LU, true peak: -0.6 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 04 Mmin.flac
    -26.1 LUFS, LRA: 14.9 LU, true peak: -12.0 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 05 3b Or T.flac
    -14.1 LUFS, LRA: 11.4 LU, true peak: 0.2 dBTP, /home/jan/music/bad loop - Luo/bad loop - Luo - 06 Kannas Nsp.flac
    --------------------------------------------------------------------------------
    -11.8 LUFS, LRA: 13.3 LU, true peak: 0.2 dBTP

Scripts can parse standard output easily:

    $ ./r128-scanner -p dbtp -l ~/music/bad\ loop\ -\ Luo/*.flac 2> /dev/null
    -12.8,14.2,-0.0,/home/jan/music/bad loop - Luo/bad loop - Luo - 01 Nio.flac
    -11.1,8.3,-0.1,/home/jan/music/bad loop - Luo/bad loop - Luo - 02 Eri Valeire.flac
    -10.1,11.8,-0.1,/home/jan/music/bad loop - Luo/bad loop - Luo - 03 Kauniit Ihmiset.flac
    -11.3,11.8,-0.6,/home/jan/music/bad loop - Luo/bad loop - Luo - 04 Mmin.flac
    -26.1,14.9,-12.0,/home/jan/music/bad loop - Luo/bad loop - Luo - 05 3b Or T.flac
    -14.1,11.4,0.2,/home/jan/music/bad loop - Luo/bad loop - Luo - 06 Kannas Nsp.flac
    -11.8,13.3,0.2


The scanners also support ReplayGain tagging with the option "-t". Run it like
this:

    r128-scanner -t album <directory>

or:

    r128-scanner -t track <directory>

and it will scan the directory as one album/as tracks. Use the option "-r" to
search recursively for music files and tag them as one album per subfolder. The
tagger also supports file input; then all files are treated as one album.

Use the option "-p" to print information about peak values. Use "-p sample" for
sample peaks, "-p true" for true peaks, "-p dbtp" for true peaks in dBTP and
"-p all" to print all values.

The reference volume is -18 LUFS (5 dB louder than the EBU R 128 reference level
of -23 LUFS).

The scanner supports loudness range measurement with the command line
option "-l".

Use the options "-s", "-m" or "-i" to print short-term (last 3s), momentary
(last 0.4s) or integrated loudness information to stdout. For example:

    r128-scanner -m 0.1 foo.wav

to print the momentary loudness of foo.wav to stdout every 0.1s.
