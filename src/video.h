#ifndef IGPUP_DITHERING_PROJECT_VIDEO_H_
#define IGPUP_DITHERING_PROJECT_VIDEO_H_

extern "C" {
#include <libavcodec/avcodec.h>
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

  bool DitherGrayscale(const char *output_filename);
  bool DitherGrayscale(const std::string &output_filename);

 private:
  Image image;
  std::string input_filename;
};

#endif
