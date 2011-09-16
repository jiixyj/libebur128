#include <stdio.h>

static void print_help(void) {
    printf(""
"Usage: loudness scan|tag|dump [OPTION...] [FILE|DIRECTORY]...\n"
"\n"
"`loudness' scans audio files according to the EBU R128 standard. It can output\n"
"loudness and peak information, write it to ReplayGain conformant tags, or dump\n"
"momentary/shortterm/integrated loudness in fixed intervals to the console.\n"
"\n"
"Examples:\n"
"  loudness scan foo.wav       # Scans foo.wav and writes information to stdout.\n"
"  loudness tag -r bar/        # Tag all files in foo as one album per subfolder.\n"
"  loudness dump -m 1.0 a.wav  # Each second, write momentary loudness to stdout.\n"
"\n"
" Main operation mode:\n"
"  scan                       output loudness and peak information\n"
"  tag                        tag files with ReplayGain conformant tags\n"
"  dump                       output momentary/shortterm/integrated loudness\n"
"                             in fixed intervals\n"
"\n"
" Global options:\n"
"  -r, --recursive            recursively scan files in subdirectories\n"
"  -L, --follow-symlinks      follow symbolic links (*nix only)\n"
"  --no-sort                  do not sort command line arguments alphabetically\n"
"  --force-plugin=PLUGIN      force input plugin; PLUGIN is one of:\n"
"                             sndfile, mpg123, musepack, ffmpeg\n"
"\n"
" Scan options:\n"
"  -l, --lra                  calculate loudness range in LRA\n"
"  -p, --peak=sample|true|dbtp|all  -p sample: sample peak (float value)\n"
"                                   -p true:   true peak (float value)\n"
"                                   -p dbtp:   true peak (dB True Peak)\n"
"                                   -p all:    show all peak values\n"
"\n"
" Tag options:\n"
"  -t, --track                write only track gain (album gain is default)\n"
"  -n, --dry-run              perform a trial run with no changes made\n"
"\n"
" Dump options:\n"
"  -m, --momentary=INTERVAL   print momentary loudness every INTERVAL seconds\n"
"  -s, --shortterm=INTERVAL   print shortterm loudness every INTERVAL seconds\n"
"  -i, --integrated=INTERVAL  print integrated loudness every INTERVAL seconds\n"
    );
}

int main(int argc, char *argv[])
{
    print_help();
    return 0;
}
