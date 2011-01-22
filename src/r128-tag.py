#!/usr/bin/env python
# See LICENSE file for copyright and license details.
import sys
import os
import subprocess
import threading
import Queue
import signal
import string

import rgtag

def worker():
  while True:
    item = q.get()
    item.wait()
    stdoutdata, stderrdata = item.communicate()
    rginfo = string.split(stdoutdata, "\n")
    q.task_done()
    witem = w.get()
    try:
      for i, name in enumerate(witem[1:]):
        print name
        rgdata = string.split(rginfo[i], " ")
        print rgdata
        rgtag.rgtag(os.path.join(witem[0], name), float(rgdata[0]),
                                                  float(rgdata[1]),
                                                  float(rgdata[2]),
                                                  float(rgdata[3]))
    except ValueError:
      pass
    w.task_done()

topdir = os.path.abspath(os.path.dirname(sys.argv[0]))

number_threads = 1
try:
  import multiprocessing
  number_threads = multiprocessing.cpu_count()
except (ImportError,NotImplementedError):
  pass

w = Queue.Queue(number_threads)
q = Queue.Queue(number_threads)

t = threading.Thread(target=worker)
t.daemon = True
t.start()

for root, dirs, files in os.walk(sys.argv[1]):
  currentdir = os.getcwd()
  files_mp3 = [elem for elem in files if elem[-4:].find(".mp3") == 0]
  files_snd = [elem for elem in files if elem[-4:].find(".ogg") == 0 or
                                         elem[-4:].find(".oga") == 0 or
                                         elem[-5:].find(".flac") == 0]
  files_mpc = [elem for elem in files if elem[-4:].find(".mpc") == 0]
  files_mp3.sort()
  files_snd.sort()
  files_mpc.sort()
  r128_scanner = 'r128-ffmpeg'
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
    r128_files += files_mp3 + files_snd + files_mpc

  if len(r128_files) == 0:
    continue
  w.put([root] + r128_files)
  pipe = subprocess.Popen([os.path.join(topdir, r128_scanner), '-t'] + r128_files,
                          stdout=subprocess.PIPE,
                          cwd=root)
  q.put(pipe)

w.join()
q.join()
