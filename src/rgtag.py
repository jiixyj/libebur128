#!/usr/bin/env python
# See LICENSE file for copyright and license details.
import sys
import mutagen
import mutagen.flac
import mutagen.oggvorbis
import mutagen.id3
import mutagen.apev2

def rgtag(filename, trackgain, trackpeak, albumgain, albumpeak):
  tg = "%.2f dB" % trackgain
  tp = "%.8f"    % trackpeak
  ag = "%.2f dB" % albumgain
  ap = "%.8f"    % albumpeak
  if filename[-5:].find(".flac") == 0:
    audio = mutagen.flac.FLAC(filename)
    audio["replaygain_track_gain"] = tg
    audio["replaygain_track_peak"] = tp
    audio["replaygain_album_gain"] = ag
    audio["replaygain_album_peak"] = ap
    audio.save()
  elif filename[-4:].find(".ogg") == 0 or filename[-4:].find(".oga") == 0:
    audio = mutagen.oggvorbis.OggVorbis(filename)
    audio["replaygain_track_gain"] = tg
    audio["replaygain_track_peak"] = tp
    audio["replaygain_album_gain"] = ag
    audio["replaygain_album_peak"] = ap
    audio.save()
  elif filename[-4:].find(".mp3") == 0:
    try:
      audio = mutagen.id3.ID3(filename)
    except mutagen.id3.ID3NoHeaderError:
      audio = mutagen.id3.ID3()
    frame = mutagen.id3.Frames["RVA2"](desc="track", channel=1,
                                       gain=trackgain, peak=trackpeak)
    audio.add(frame)
    frame = mutagen.id3.Frames["RVA2"](desc="album", channel=1,
                                       gain=albumgain, peak=albumpeak)
    audio.add(frame)
    frame = mutagen.id3.Frames["TXXX"](encoding=3,
                                       desc="replaygain_track_gain", text=tg)
    audio.add(frame)
    frame = mutagen.id3.Frames["TXXX"](encoding=3,
                                       desc="replaygain_track_peak", text=tp)
    audio.add(frame)
    frame = mutagen.id3.Frames["TXXX"](encoding=3,
                                       desc="replaygain_album_gain", text=ag)
    audio.add(frame)
    frame = mutagen.id3.Frames["TXXX"](encoding=3,
                                       desc="replaygain_album_peak", text=ap)
    audio.add(frame)
    audio.save(filename)
  elif filename[-4:].find(".mpc") == 0:
    try:
      audio = mutagen.apev2.APEv2(filename)
    except mutagen.apev2.APENoHeaderError:
      audio = mutagen.apev2.APEv2()
    audio["replaygain_track_gain"] = tg
    audio["replaygain_track_peak"] = tp
    audio["replaygain_album_gain"] = ag
    audio["replaygain_album_peak"] = ap
    audio.save(filename)

if __name__ == "__main__":
  import sys
  rgtag(sys.argv[1], float(sys.argv[2]), float(sys.argv[3]),
                     float(sys.argv[4]), float(sys.argv[5]))
