#!/usr/bin/env python
# See LICENSE file for copyright and license details.
import sys
import os
import subprocess
import threading
try:
  import queue
except ImportError:
  import Queue as queue
import signal
import string
import getopt
import time

import rgtag

def signal_handler(signal, frame):
  print('\nExiting!')
  sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

def usage():
  print("usage: r128-tag [-r] <directory(ies)> ...\n"
        "       r128-tag <file(s)> ...\n\n"
        "    -h: show this help\n"
        "    -r: scan directory(ies) recursively\n\n"
        "  r128-tag scans music files and tags them with ReplayGain "
                                                           "compatible tags.\n"
        "  If more than one directory is given, all available cores will be"
         " used.")

recursive = False

try:
  opts, args = getopt.getopt(sys.argv[1:], "hr")
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

def worker():
  while True:
    item = q.get()
    item.wait()
    if item.returncode != 0:
      q.task_done()
      w.get()
      w.task_done()
      return
    stdoutdata, stderrdata = item.communicate()
    rginfo = stdoutdata.decode('ascii').splitlines()
    q.task_done()
    witem = w.get()
    try:
      for i, name in enumerate(witem[1:]):
        print(name)
        rgdata = rginfo[i].split(" ")
        print(rgdata)
        rgtag.rgtag(os.path.join(witem[0], name), float(rgdata[0]),
                                                  float(rgdata[1]),
                                                  float(rgdata[2]),
                                                  float(rgdata[3]))
    except:
      print("Tagging error!")
      w.task_done()
      return
    w.task_done()

def process_dir(root, files):
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
  w.put([root] + r128_files)
  try:
    pipe = subprocess.Popen([os.path.join(topdir, r128_scanner), '-t'] +
                                                                     r128_files,
                            stdout=subprocess.PIPE,
                            cwd=root)
  except OSError:
    print("Error starting scanner " + r128_scanner + "!")
    sys.exit(1)

  q.put(pipe)
  return True




topdir = os.path.abspath(os.path.dirname(sys.argv[0]))

number_threads = 1
try:
  import multiprocessing
  number_threads = multiprocessing.cpu_count()
except (ImportError,NotImplementedError):
  pass

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


w = queue.Queue(number_threads)
q = queue.Queue(number_threads)

t = threading.Thread(target=worker)
t.daemon = True
t.start()

if all_directory:
  for directory in args:
    if recursive:
      for root, dirs, files in os.walk(args[0]):
        process_dir(root, files)
    else:
      if not process_dir(args[0], os.listdir(args[0])):
        print("No files to scan!")
else:
  process_dir(os.getcwd(), args)


w.join()
q.join()
