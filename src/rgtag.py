#!/usr/bin/env python
# See LICENSE file for copyright and license details.
import sys
tg = "%.2f dB" % float(sys.argv[2])
tp = "%.8f"    % float(sys.argv[3])
ag = "%.2f dB" % float(sys.argv[4])
ap = "%.8f"    % float(sys.argv[5])

if sys.argv[1][-5:].find(".flac") == 0:
  from mutagen.flac import FLAC
  audio = FLAC(sys.argv[1])
  audio["replaygain_track_gain"] = tg
  audio["replaygain_track_peak"] = tp
  audio["replaygain_album_gain"] = ag
  audio["replaygain_album_peak"] = ap
  audio.save()
elif sys.argv[1][-4:].find(".ogg") == 0:
  from mutagen.oggvorbis import OggVorbis
  audio = OggVorbis(sys.argv[1])
  audio["replaygain_track_gain"] = tg
  audio["replaygain_track_peak"] = tp
  audio["replaygain_album_gain"] = ag
  audio["replaygain_album_peak"] = ap
  audio.save()
elif sys.argv[1][-4:].find(".mp3") == 0:
  import mutagen, mutagen.id3
  audio = mutagen.id3.ID3(sys.argv[1])
  frame = mutagen.id3.Frames["RVA2"](desc="track", channel=1, gain=float(sys.argv[2]), peak=float(sys.argv[3]))
  audio.add(frame)
  frame = mutagen.id3.Frames["RVA2"](desc="album", channel=1, gain=float(sys.argv[4]), peak=float(sys.argv[5]))
  audio.add(frame)
  frame = mutagen.id3.Frames["TXXX"](encoding=3, desc="replaygain_track_gain", text=tg)
  audio.add(frame)
  frame = mutagen.id3.Frames["TXXX"](encoding=3, desc="replaygain_track_peak", text=tp)
  audio.add(frame)
  frame = mutagen.id3.Frames["TXXX"](encoding=3, desc="replaygain_album_gain", text=ag)
  audio.add(frame)
  frame = mutagen.id3.Frames["TXXX"](encoding=3, desc="replaygain_album_peak", text=ap)
  audio.add(frame)
  audio.save()
