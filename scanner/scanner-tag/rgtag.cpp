#include "rgtag.h"

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
#include <mp4file.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <ios>

#ifdef _WIN32
#define CAST_FILENAME (const wchar_t *)
#else
#define CAST_FILENAME
#endif

static bool clear_txxx_tag(TagLib::ID3v2::Tag* tag, TagLib::String tag_name) {
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    TagLib::ID3v2::UserTextIdentificationFrame* fr =
                dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);
    if (fr && fr->description().upper() == tag_name) {
      tag->removeFrame(fr);
      return true;
    }
  }
  return false;
}

static bool clear_rva2_tag(TagLib::ID3v2::Tag* tag, TagLib::String tag_name) {
  TagLib::ID3v2::FrameList l = tag->frameList("RVA2");
  for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    TagLib::ID3v2::RelativeVolumeFrame* fr =
                         dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
    if (fr && fr->identification().upper() == tag_name) {
      tag->removeFrame(fr);
      return true;
    }
  }
  return false;
}

static void set_txxx_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, std::string value) {
  TagLib::ID3v2::UserTextIdentificationFrame* txxx = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tag_name);
  if (!txxx) {
    txxx = new TagLib::ID3v2::UserTextIdentificationFrame;
    txxx->setDescription(tag_name);
    tag->addFrame(txxx);
  }
  txxx->setText(value);
}

static void set_rva2_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, double gain, double peak) {
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
  rva2->setVolumeAdjustment(float(gain));

  TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peak_volume;
  peak_volume.bitsRepresentingPeak = 16;
  double amp_peak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
  unsigned int amp_peak_int = static_cast<unsigned int>(std::ceil(amp_peak));
  TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(amp_peak_int);
  peak_volume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
  rva2->setPeakVolume(peak_volume);
}

struct gain_data_strings {
  gain_data_strings(struct gain_data* gd) {
    std::stringstream ss;
    ss.precision(2);
    ss << std::fixed;
    ss << gd->album_gain << " dB"; album_gain = ss.str(); ss.str(std::string()); ss.clear();
    ss << gd->track_gain << " dB"; track_gain = ss.str(); ss.str(std::string()); ss.clear();
    ss.precision(6);
    ss << gd->album_peak; ss >> album_peak; ss.str(std::string()); ss.clear();
    ss << gd->track_peak; ss >> track_peak; ss.str(std::string()); ss.clear();
  }
  std::string track_gain, track_peak, album_gain, album_peak;
};

static bool tag_id3v2(const char* filename,
                      struct gain_data* gd,
                      struct gain_data_strings* gds) {
  TagLib::MPEG::File f(CAST_FILENAME filename);
  TagLib::ID3v2::Tag* id3v2tag = f.ID3v2Tag(true);

  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_gain").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_peak").upper()));
  while (clear_rva2_tag(id3v2tag, TagLib::String("album").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_gain").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_peak").upper()));
  while (clear_rva2_tag(id3v2tag, TagLib::String("track").upper()));
  set_txxx_tag(id3v2tag, "replaygain_track_gain", gds->track_gain);
  set_txxx_tag(id3v2tag, "replaygain_track_peak", gds->track_peak);
  set_rva2_tag(id3v2tag, "track", gd->track_gain, gd->track_peak);
  if (gd->album_mode) {
    set_txxx_tag(id3v2tag, "replaygain_album_gain", gds->album_gain);
    set_txxx_tag(id3v2tag, "replaygain_album_peak", gds->album_peak);
    set_rva2_tag(id3v2tag, "album", gd->album_gain, gd->album_peak);
  }

  return !f.save(TagLib::MPEG::File::ID3v2, false);
}

static bool tag_vorbis_comment(const char* filename,
                               const char* extension,
                               struct gain_data* gd,
                               struct gain_data_strings* gds) {
  TagLib::File* file = NULL;
  TagLib::Ogg::XiphComment* xiph = NULL;
  if (!::strcmp(extension, "flac")) {
    TagLib::FLAC::File* f = new TagLib::FLAC::File(CAST_FILENAME filename);
    xiph = f->xiphComment(true);
    file = f;
  } else if (!::strcmp(extension, "ogg") || !::strcmp(extension, "oga")) {
    TagLib::Ogg::Vorbis::File* f = new TagLib::Ogg::Vorbis::File(CAST_FILENAME filename);
    xiph = f->tag();
    file = f;
  }
  xiph->addField("REPLAYGAIN_TRACK_GAIN", gds->track_gain);
  xiph->addField("REPLAYGAIN_TRACK_PEAK", gds->track_peak);
  if (gd->album_mode) {
    xiph->addField("REPLAYGAIN_ALBUM_GAIN", gds->album_gain);
    xiph->addField("REPLAYGAIN_ALBUM_PEAK", gds->album_peak);
  } else {
    xiph->removeField("REPLAYGAIN_ALBUM_GAIN");
    xiph->removeField("REPLAYGAIN_ALBUM_PEAK");
  }
  bool success = file->save();
  delete file;
  return !success;
}

static bool tag_ape(const char* filename,
                    const char* extension,
                    struct gain_data* gd,
                    struct gain_data_strings* gds) {
  TagLib::File* file = NULL;
  TagLib::APE::Tag* ape = NULL;
  if (!::strcmp(extension, "mpc")) {
    TagLib::MPC::File* f = new TagLib::MPC::File(CAST_FILENAME filename);
    ape = f->APETag(true);
    file = f;
  } else if (!::strcmp(extension, "wv")) {
    TagLib::WavPack::File* f = new TagLib::WavPack::File(CAST_FILENAME filename);
    ape = f->APETag(true);
    file = f;
  }
  ape->addValue("replaygain_track_gain", gds->track_gain);
  ape->addValue("replaygain_track_peak", gds->track_peak);
  if (gd->album_mode) {
    ape->addValue("replaygain_album_gain", gds->album_gain);
    ape->addValue("replaygain_album_peak", gds->album_peak);
  } else {
    ape->removeItem("replaygain_album_gain");
    ape->removeItem("replaygain_album_peak");
  }
  bool success = file->save();
  delete file;
  return !success;
}

static bool tag_mp4(const char* filename,
                    struct gain_data* gd,
                    struct gain_data_strings* gds) {
  TagLib::MP4::File f(CAST_FILENAME filename);
  TagLib::MP4::Tag* t = f.tag();
  TagLib::MP4::ItemListMap& ilm = t->itemListMap();
  ilm["----:com.apple.iTunes:replaygain_track_gain"] = TagLib::MP4::Item(TagLib::StringList(gds->track_gain));
  ilm["----:com.apple.iTunes:replaygain_track_peak"] = TagLib::MP4::Item(TagLib::StringList(gds->track_peak));
  if (gd->album_mode) {
    ilm["----:com.apple.iTunes:replaygain_album_gain"] = TagLib::MP4::Item(TagLib::StringList(gds->album_gain));
    ilm["----:com.apple.iTunes:replaygain_album_peak"] = TagLib::MP4::Item(TagLib::StringList(gds->album_peak));
  } else {
    ilm.erase("----:com.apple.iTunes:replaygain_album_gain");
    ilm.erase("----:com.apple.iTunes:replaygain_album_peak");
  }
  return !f.save();
}

int set_rg_info(const char* filename,
                const char* extension,
                struct gain_data* gd) {
  struct gain_data_strings gds(gd);

  if (!::strcmp(extension, "mp3") || !::strcmp(extension, "mp2")) {
    return tag_id3v2(filename, gd, &gds);
  } else if (!::strcmp(extension, "flac") || !::strcmp(extension, "ogg") || !::strcmp(extension, "oga")) {
    return tag_vorbis_comment(filename, extension, gd, &gds);
  } else if (!::strcmp(extension, "mpc") || !::strcmp(extension, "wv")) {
    return tag_ape(filename, extension, gd, &gds);
  } else if (!::strcmp(extension, "mp4") || !::strcmp(extension, "m4a")) {
    return tag_mp4(filename, gd, &gds);
  }
  return 1;
}
