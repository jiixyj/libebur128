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
* ReplayGain compatible tagging support for MP3, OGG, Musepack, FLAC and more


Requirements
------------

The library itself has no requirements besides ANSI C.

The scanner needs Glib, GTK and taglib.
There is input support for gstreamer, libsndfile, libmpg123, FFmpeg
and libmpcdec.


Installation
------------

In the root folder, type:

    mkdir build
    cd build
    cmake ..
    make

If you want the git version, run:

    git clone git://github.com/jiixyj/libebur128.git
    cd libebur128
    git submodule init
    git submodule update

Usage
-----

Run "loudness scan" with the files you want to scan as arguments. The scanner
will automatically choose the best input plugin for each file. You can force an
input plugin with the command line option "--force-plugin=PLUGIN", where PLUGIN
is one of gstreamer, sndfile, mpg123, musepack or ffmpeg.

The scanner also support ReplayGain tagging. Run it like this:

    loudness tag <files>     # scan files as album

or:

    loudness tag -t <files>  # scan files as single tracks

Use the option "-r" to search recursively for music files and tag them as one
album per subfolder.

The reference volume is -18 LUFS (5 dB louder than the EBU R 128 reference level
of -23 LUFS).

Use the option "-p" to print information about peak values. Use "-p sample" for
sample peaks, "-p true" for true peaks, "-p dbtp" for true peaks in dBTP and
"-p all" to print all values.

The scanner supports loudness range measurement with the command line
option "-l".

In "dump" mode, use the options "-s", "-m" or "-i" to print short-term
(last 3s), momentary (last 0.4s) or integrated loudness information to stdout.
For example:

    loudness dump -m 0.1 foo.wav

to print the momentary loudness of foo.wav to stdout every 0.1s.
