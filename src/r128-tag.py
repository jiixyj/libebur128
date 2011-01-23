#!/usr/bin/env python
# See LICENSE file for copyright and license details.
import sys
import os
import getopt
import subprocess
import multiprocessing

import rgtag

def usage():
  print("usage: r128-tag [-r] <directory(ies)> ...\n"
        "       r128-tag <file(s)> ...\n\n"
        "    -h: show this help\n"
        "    -r: scan directory(ies) recursively\n\n"
        "  r128-tag scans music files and tags them with ReplayGain "
                                                           "compatible tags.\n"
        "  If more than one directory is given, all available cores will be"
         " used.")

def get_options(argv):
  recursive = False
  try:
    opts, args = getopt.getopt(argv, "hr")
  except getopt.GetoptError:
    usage()
    sys.exit(1)
  for opt, arg in opts:
    if opt in ("-h"):
      usage()
      sys.exit(1)
    elif opt in ("-r"):
      recursive = True
  if len(args) == 0:
    usage()
    sys.exit(1)
  return args, recursive


def process_dir(topdir, root, files):
  files_mp3 = [elem for elem in files if elem[-4:].find(".mp3") == 0]
  files_snd = [elem for elem in files if elem[-4:].find(".ogg") == 0 or
                                         elem[-4:].find(".oga") == 0 or
                                         elem[-5:].find(".flac") == 0]
  files_mpc = [elem for elem in files if elem[-4:].find(".mpc") == 0]
  files_mp3.sort()
  files_snd.sort()
  files_mpc.sort()
  r128_scanner = ''
  r128_files = []
  if len(files_mp3) != 0 and len(files_snd) == 0 and len(files_mpc) == 0:
    r128_scanner = 'r128-mpg123'
    r128_files += files_mp3
  elif len(files_mp3) == 0 and len(files_snd) != 0 and len(files_mpc) == 0:
    r128_scanner = 'r128-sndfile'
    r128_files += files_snd
  elif len(files_mp3) == 0 and len(files_snd) == 0 and len(files_mpc) != 0:
    r128_scanner = 'r128-musepack'
    r128_files += files_mpc
  else:
    return False

  if len(r128_files) == 0:
    return False
  try:
    pipe = subprocess.Popen([os.path.join(topdir, r128_scanner), '-t']
                                                                + r128_files,
                            stdout=subprocess.PIPE,
                            cwd=root)
    pipe.wait()
    stdoutdata, stderrdata = pipe.communicate()
    rginfo = stdoutdata.decode('ascii').splitlines()
    try:
      for i, name in enumerate(r128_files):
        print(name)
        rgdata = rginfo[i].split(" ")
        print(rgdata)
        rgtag.rgtag(os.path.join(root, name), float(rgdata[0]),
                                              float(rgdata[1]),
                                              float(rgdata[2]),
                                              float(rgdata[3]))
    except:
      print("Tagging error!")
      return
  except OSError:
    print("Error starting scanner " + r128_scanner + "!")
    sys.exit(1)
  return True

if __name__ == '__main__':
  args, recursive = get_options(sys.argv[1:])
  topdir = os.path.abspath(os.path.dirname(sys.argv[0]))
  # Process either directories _or_ files
  all_files = True
  all_directory = True
  for filename in args:
    if os.path.isdir(filename):
      all_files = False
    elif os.path.isfile(filename):
      all_directory = False
    else:
      print("Error opening file!")
      sys.exit(1)
  if not all_files and not all_directory:
    usage()
    sys.exit(1)
  if all_files and recursive:
    usage()
    sys.exit(1)

  number_threads = multiprocessing.cpu_count()
  pool = multiprocessing.Pool(processes=number_threads)
  results = []

  if all_directory:
    for directory in args:
      if recursive:
        for root, dirs, files in os.walk(directory):
          result = pool.apply_async(process_dir, (topdir, root, files,))
          results.append(result)
      else:
        result = pool.apply_async(process_dir,
                                  (topdir, directory, os.listdir(directory),));
        results.append(result)
  else:
    print(topdir, os.getcwd(), args)
    sys.exit(1)
    process_dir(os.getcwd(), args)

  try:
    for result in results:
      val = result.get(99999999)
      if not val:
        print("No files to scan!")
  except KeyboardInterrupt:
    pool.terminate()
    pool.close()
