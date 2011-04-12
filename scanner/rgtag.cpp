#include "./rgtag.h"

#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <relativevolumeframe.h>
#include <xiphcomment.h>
#include <apetag.h>

#include <mpegfile.h>
#include <flacfile.h>
#include <oggfile.h>
#include <vorbisfile.h>
#include <mpcfile.h>
#include <wavpackfile.h>

#include <cmath>
#include <sstream>

void set_txxx_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, std::string value) {
  TagLib::ID3v2::UserTextIdentificationFrame* txxx = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tag_name);
  if (!txxx) {
    txxx = new TagLib::ID3v2::UserTextIdentificationFrame;
    txxx->setDescription(tag_name);
    tag->addFrame(txxx);
  }
  txxx->setText(value);
}

void clear_txxx_tag(TagLib::ID3v2::Tag* tag, std::string tag_name) {
  TagLib::ID3v2::UserTextIdentificationFrame* txxx = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tag_name);
  if (txxx) {
    tag->removeFrame(txxx);
  }
}

void set_rva2_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, double gain, double peak) {
  TagLib::ID3v2::RelativeVolumeFrame* rva2 = NULL;
  TagLib::ID3v2::FrameList rva2_frame_list = tag->frameList("RVA2");
  TagLib::ID3v2::FrameList::ConstIterator it = rva2_frame_list.begin();
  for (; it != rva2_frame_list.end(); ++it) {
    TagLib::ID3v2::RelativeVolumeFrame* fr =
                         dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
    if (fr->identification() == tag_name) {
      rva2 = fr;
      break;
    }
  }
  if (!rva2) {
    rva2 = new TagLib::ID3v2::RelativeVolumeFrame;
    rva2->setIdentification(tag_name);
    tag->addFrame(rva2);
  }
  rva2->setChannelType(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
  rva2->setVolumeAdjustment(gain);

  TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peak_volume;
  peak_volume.bitsRepresentingPeak = 16;
  double amp_peak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
  unsigned int amp_peak_int = static_cast<unsigned int>(std::ceil(amp_peak));
  TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(amp_peak_int);
  peak_volume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
  rva2->setPeakVolume(peak_volume);
}

void clear_rva2_tag(TagLib::ID3v2::Tag* tag, std::string tag_name) {
  TagLib::ID3v2::RelativeVolumeFrame* rva2 = NULL;
  TagLib::ID3v2::FrameList rva2_frame_list = tag->frameList("RVA2");
  TagLib::ID3v2::FrameList::ConstIterator it = rva2_frame_list.begin();
  for (; it != rva2_frame_list.end(); ++it) {
    TagLib::ID3v2::RelativeVolumeFrame* fr =
                         dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
    if (fr->identification() == tag_name) {
      rva2 = fr;
      break;
    }
  }
  if (rva2) {
    tag->removeFrame(rva2);
  }
}

void set_rg_info(const char* filename,
                 double track_gain,
                 double track_peak,
                 int album_mode,
                 double album_gain,
                 double album_peak) {
  std::string fn(filename);
  std::string ag, tg, ap, tp;
  std::stringstream ss;
  ss.precision(2);
  ss << std::fixed;
  ss << album_gain; ss >> ag; ss.clear();
  ss << track_gain; ss >> tg; ss.clear();
  ss.precision(8);
  ss << album_peak; ss >> ap; ss.clear();
  ss << track_peak; ss >> tp; ss.clear();

  std::string extension = fn.substr(fn.find_last_of(".") + 1);

  if (extension == "mp3") {
    TagLib::MPEG::File f(filename);
    TagLib::ID3v2::Tag* id3v2tag = f.ID3v2Tag(true);

    set_txxx_tag(id3v2tag, "replaygain_track_gain", tg + " dB");
    set_txxx_tag(id3v2tag, "replaygain_track_peak", tp);
    set_rva2_tag(id3v2tag, "track", track_gain, track_peak);
    if (album_mode) {
      set_txxx_tag(id3v2tag, "replaygain_album_gain", ag + " dB");
      set_txxx_tag(id3v2tag, "replaygain_album_peak", ap);
      set_rva2_tag(id3v2tag, "album", album_gain, album_peak);
    } else {
      clear_txxx_tag(id3v2tag, "replaygain_album_gain");
      clear_txxx_tag(id3v2tag, "replaygain_album_peak");
      clear_rva2_tag(id3v2tag, "album");
    }

    f.save();
  } else if (extension == "flac" || extension == "ogg" || extension == "oga") {
    TagLib::File* file = NULL;
    TagLib::Ogg::XiphComment* xiph = NULL;
    if (extension == "flac") {
      TagLib::FLAC::File* f = new TagLib::FLAC::File(filename);
      xiph = f->xiphComment(true);
      file = f;
    } else if (extension == "ogg" || extension == "oga") {
      TagLib::Ogg::Vorbis::File* f = new TagLib::Ogg::Vorbis::File(filename);
      xiph = f->tag();
      file = f;
    }
    xiph->addField("replaygain_track_gain", tg + " dB");
    xiph->addField("replaygain_track_peak", tp);
    if (album_mode) {
      xiph->addField("replaygain_album_gain", ag + " dB");
      xiph->addField("replaygain_album_peak", ap);
    } else {
      xiph->removeField("replaygain_album_gain");
      xiph->removeField("replaygain_album_peak");
      xiph->removeField("REPLAYGAIN_ALBUM_GAIN");
      xiph->removeField("REPLAYGAIN_ALBUM_PEAK");
    }
    file->save();
    delete file;
  } else if (extension == "mpc" || extension == "wv") {
    TagLib::File* file = NULL;
    TagLib::APE::Tag* ape = NULL;
    if (extension == "mpc") {
      TagLib::MPC::File* f = new TagLib::MPC::File(filename);
      ape = f->APETag(true);
      file = f;
    } else if (extension == "wv") {
      TagLib::WavPack::File* f = new TagLib::WavPack::File(filename);
      ape = f->APETag(true);
      file = f;
    }
    ape->addValue("replaygain_track_gain", tg + " dB");
    ape->addValue("replaygain_track_peak", tp);
    if (album_mode) {
      ape->addValue("replaygain_album_gain", ag + " dB");
      ape->addValue("replaygain_album_peak", ap);
    } else {
      ape->removeItem("replaygain_album_gain");
      ape->removeItem("replaygain_album_peak");
      ape->removeItem("REPLAYGAIN_ALBUM_GAIN");
      ape->removeItem("REPLAYGAIN_ALBUM_PEAK");
    }
    file->save();
    delete file;
  }

}

#if 0
int main(int argc, char* argv[]) {
  double track_gain, track_peak, album_gain, album_peak;
  int album_mode = atoi(argv[2]);
  track_gain = atof(argv[3]);
  track_peak = atof(argv[4]);
  album_gain = atof(argv[5]);
  album_peak = atof(argv[6]);

  set_rg_info(argv[1], track_gain, track_peak, album_mode,
                       album_gain, album_peak);
  return 0;
}
#endif
