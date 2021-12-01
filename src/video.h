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

/*!
 * \brief Helper class that uses Image and OpenCLHandle to dither video frames.
 *
 * Note that libraries provided by ffmpeg are used to decode the video.
 */
class Video {
 public:
  explicit Video(const char *video_filename);
  explicit Video(const std::string &video_filename);

  ~Video();

  /// Same as DitherVideo(const std::string&, Image*, bool, bool)
  bool DitherVideo(const char *output_filename, Image *blue_noise,
                   bool grayscale = false, bool overwrite = false);

  /*!
   * \brief Dithers the frames in the input video.
   *
   * Currently, the program doesn't create the output video, but instead outputs
   * each frame as an individual image in the current directory. If things go
   * well, the expected behavior will be implemented soon.
   *
   * \return True on success.
   */
  bool DitherVideo(const std::string &output_filename, Image *blue_noise,
                   bool grayscale = false, bool overwrite = false);

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
