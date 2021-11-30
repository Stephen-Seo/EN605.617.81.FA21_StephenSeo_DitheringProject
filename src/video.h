#ifndef IGPUP_DITHERING_PROJECT_VIDEO_H_
#define IGPUP_DITHERING_PROJECT_VIDEO_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "image.h"

constexpr unsigned int kReadBufSize = 4096;
constexpr unsigned int kReadBufPaddingSize = AV_INPUT_BUFFER_PADDING_SIZE;
constexpr unsigned int kReadBufSizeWithPadding =
    kReadBufSize + kReadBufPaddingSize;

class Video {
 public:
  explicit Video(const char *video_filename);
  explicit Video(const std::string &video_filename);

  ~Video();

  bool DitherVideo(const char *output_filename, Image *blue_noise,
                   bool grayscale = false);
  bool DitherVideo(const std::string &output_filename, Image *blue_noise,
                   bool grayscale = false);

 private:
  Image image_;
  std::string input_filename_;
  SwsContext *sws_context_;
  unsigned int frame_count_;
  unsigned int packet_count_;

  bool HandleDecodingPacket(AVCodecContext *codec_ctx, AVPacket *pkt,
                            AVFrame *frame, Image *blue_noise, bool grayscale);
};

#endif
