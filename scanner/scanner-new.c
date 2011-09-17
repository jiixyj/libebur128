#include <stdio.h>

static void print_help(void) {
    printf("Usage: loudness scan|tag|dump [OPTION...] [FILE|DIRECTORY]...\n");
    printf("\n");
    printf("`loudness' scans audio files according to the EBU R128 standard. It can output\n");
    printf("loudness and peak information, write it to ReplayGain conformant tags, or dump\n");
    printf("momentary/shortterm/integrated loudness in fixed intervals to the console.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  loudness scan foo.wav       # Scans foo.wav and writes information to stdout.\n");
    printf("  loudness tag -r bar/        # Tag all files in foo as one album per subfolder.\n");
    printf("  loudness dump -m 1.0 a.wav  # Each second, write momentary loudness to stdout.\n");
    printf("\n");
    printf(" Main operation mode:\n");
    printf("  scan                       output loudness and peak information\n");
    printf("  tag                        tag files with ReplayGain conformant tags\n");
    printf("  dump                       output momentary/shortterm/integrated loudness\n");
    printf("                             in fixed intervals\n");
    printf("\n");
    printf(" Global options:\n");
    printf("  -r, --recursive            recursively scan files in subdirectories\n");
    printf("  -L, --follow-symlinks      follow symbolic links (*nix only)\n");
    printf("  --no-sort                  do not sort command line arguments alphabetically\n");
    printf("  --force-plugin=PLUGIN      force input plugin; PLUGIN is one of:\n");
    printf("                             sndfile, mpg123, musepack, ffmpeg\n");
    printf("\n");
    printf(" Scan options:\n");
    printf("  -l, --lra                  calculate loudness range in LRA\n");
    printf("  -p, --peak=sample|true|dbtp|all  -p sample: sample peak (float value)\n");
    printf("                                   -p true:   true peak (float value)\n");
    printf("                                   -p dbtp:   true peak (dB True Peak)\n");
    printf("                                   -p all:    show all peak values\n");
    printf("\n");
    printf(" Tag options:\n");
    printf("  -t, --track                write only track gain (album gain is default)\n");
    printf("  -n, --dry-run              perform a trial run with no changes made\n");
    printf("\n");
    printf(" Dump options:\n");
    printf("  -m, --momentary=INTERVAL   print momentary loudness every INTERVAL seconds\n");
    printf("  -s, --shortterm=INTERVAL   print shortterm loudness every INTERVAL seconds\n");
    printf("  -i, --integrated=INTERVAL  print integrated loudness every INTERVAL seconds\n");
}

int main(int argc, char *argv[])
{
    print_help();
    return 0;
}
