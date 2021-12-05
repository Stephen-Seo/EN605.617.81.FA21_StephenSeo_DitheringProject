#ifndef IGPUP_DITHERING_PROJECT_VIDEO_H_
#define IGPUP_DITHERING_PROJECT_VIDEO_H_

#include <tuple>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "image.h"

constexpr unsigned int kReadBufSize = 4096;
constexpr unsigned int kReadBufPaddingSize = AV_INPUT_BUFFER_PADDING_SIZE;
constexpr unsigned int kReadBufSizeWithPadding =
    kReadBufSize + kReadBufPaddingSize;

constexpr unsigned int kOutputBitrate = 80000000;

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

  // disable copy
  Video(const Video &other) = delete;
  Video &operator=(const Video &other) = delete;

  // allow move
  Video(Video &&other) = default;
  Video &operator=(Video &&other) = default;

  /// Same as DitherVideo(const std::string&, Image*, bool, bool)
  bool DitherVideo(const char *output_filename, Image *blue_noise,
                   bool grayscale = false, bool overwrite = false,
                   bool output_as_pngs = false);

  /*!
   * \brief Dithers the frames in the input video.
   *
   * If output_as_pngs is true, then the output will be individaul PNGs of each
   * frame instead of a video file. This may be desireable for more control over
   * the params set when encoding the resulting video.
   *
   * \return True on success.
   */
  bool DitherVideo(const std::string &output_filename, Image *blue_noise,
                   bool grayscale = false, bool overwrite = false,
                   bool output_as_pngs = false);

 private:
  Image image_;
  std::string input_filename_;
  SwsContext *sws_dec_context_;
  SwsContext *sws_enc_context_;
  unsigned int frame_count_;
  unsigned int packet_count_;
  bool was_grayscale_;

  std::tuple<bool, std::vector<AVFrame *>> HandleDecodingPacket(
      AVCodecContext *codec_ctx, AVPacket *pkt, AVFrame *frame,
      Image *blue_noise, bool grayscale, bool color_changed,
      bool output_as_pngs);

  bool HandleEncodingFrame(AVFormatContext *enc_format_ctx,
                           AVCodecContext *enc_codec_ctx, AVFrame *yuv_frame,
                           AVStream *video_stream);
};

#endif
