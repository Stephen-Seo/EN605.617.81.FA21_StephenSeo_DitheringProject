#include "video.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

Video::Video(const char *video_filename) : Video(std::string(video_filename)) {}

Video::Video(const std::string &video_filename)
    : image_(),
      input_filename_(video_filename),
      sws_context_(nullptr),
      frame_count_(0),
      packet_count_(0) {}

Video::~Video() {
  if (sws_context_ != nullptr) {
    sws_freeContext(sws_context_);
  }
}

bool Video::DitherVideo(const char *output_filename, Image *blue_noise,
                        bool grayscale) {
  return DitherVideo(std::string(output_filename), blue_noise, grayscale);
}

bool Video::DitherVideo(const std::string &output_filename, Image *blue_noise,
                        bool grayscale) {
  // Get AVFormatContext for input file
  AVFormatContext *avf_context = nullptr;
  std::string url = std::string("file:") + input_filename_;
  int return_value =
      avformat_open_input(&avf_context, url.c_str(), nullptr, nullptr);
  if (return_value != 0) {
    std::cout << "ERROR: Failed to open input file to determine format"
              << std::endl;
    return false;
  }

  // Read from input file to fill in info in AVFormatContext
  return_value = avformat_find_stream_info(avf_context, nullptr);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to determine input file stream info"
              << std::endl;
    avformat_close_input(&avf_context);
    return false;
  }

  // Get "best" video stream
  AVCodec *avcodec = nullptr;
  return_value = av_find_best_stream(
      avf_context, AVMediaType::AVMEDIA_TYPE_VIDEO, -1, -1, &avcodec, 0);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to get video stream in input file" << std::endl;
    avformat_close_input(&avf_context);
    return false;
  }
  int video_stream_idx = return_value;

  // Alloc codec context
  AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec);
  if (!codec_ctx) {
    std::cout << "ERROR: Failed to alloc codec context" << std::endl;
    avformat_close_input(&avf_context);
    return false;
  }

  // Set codec parameters from input stream
  return_value = avcodec_parameters_to_context(
      codec_ctx, avf_context->streams[video_stream_idx]->codecpar);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to set codec parameters from input stream"
              << std::endl;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_context);
    return false;
  }

  // Init codec context
  return_value = avcodec_open2(codec_ctx, avcodec, nullptr);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to init codec context" << std::endl;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_context);
    return false;
  }

  av_dump_format(avf_context, video_stream_idx, input_filename_.c_str(), 0);

  // Alloc a packet object for reading packets
  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    std::cout << "ERROR: Failed to alloc an AVPacket" << std::endl;
    avcodec_free_context(&codec_ctx);
    return false;
  }

  // Alloc a frame object for reading frames
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    std::cout << "ERROR: Failed to alloc video frame object" << std::endl;
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    return false;
  }

  // read frames
  while (av_read_frame(avf_context, pkt) >= 0) {
    if (pkt->stream_index == video_stream_idx) {
      if (!HandleDecodingPacket(codec_ctx, pkt, frame, blue_noise, grayscale)) {
        return false;
      }
    }
  }

  // flush decoders
  if (!HandleDecodingPacket(codec_ctx, nullptr, frame, blue_noise, grayscale)) {
    return false;
  }

  // cleanup
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&avf_context);
  return true;
}

bool Video::HandleDecodingPacket(AVCodecContext *codec_ctx, AVPacket *pkt,
                                 AVFrame *frame, Image *blue_noise,
                                 bool grayscale) {
  int return_value = avcodec_send_packet(codec_ctx, pkt);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to decode packet (" << packet_count_ << ')'
              << std::endl;
    return false;
  }

  return_value = 0;
  while (return_value >= 0) {
    return_value = avcodec_receive_frame(codec_ctx, frame);
    if (return_value == AVERROR(EAGAIN) || return_value == AVERROR_EOF) {
      return true;
    } else if (return_value < 0) {
      std::cout << "ERROR: Failed to get frame from decoded packet(s)"
                << std::endl;
      return false;
    }
    ++frame_count_;

    std::cout << "Frame " << frame_count_ << std::endl;  // TODO DEBUG

    // output buffer info for converting pixel format to RGBA
    uint8_t *dst[AV_NUM_DATA_POINTERS];
    dst[0] = (uint8_t *)calloc(4 * frame->width * frame->height + 16,
                               sizeof(uint8_t));
    for (unsigned int i = 1; i < AV_NUM_DATA_POINTERS; ++i) {
      dst[i] = nullptr;
    }
    std::array<int, AV_NUM_DATA_POINTERS> dst_strides = {
        frame->width * (grayscale ? 1 : 4), 0, 0, 0, 0, 0, 0, 0};

    unsigned int line_count = 0;
    for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
      if (frame->linesize[i] > 0) {
        ++line_count;
      }
    }

    if (line_count == 0) {
      std::cout << "ERROR: Invalid number of picture planes" << std::endl;
      for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        free(dst[i]);
      }
      return false;
    }

    // Convert colors to RGBA
    if (sws_context_ == nullptr) {
      sws_context_ = sws_getContext(frame->width, frame->height,
                                    (AVPixelFormat)frame->format, frame->width,
                                    frame->height,
                                    grayscale ? AVPixelFormat::AV_PIX_FMT_GRAY8
                                              : AVPixelFormat::AV_PIX_FMT_RGBA,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (sws_context_ == nullptr) {
        std::cout << "ERROR: Failed to init sws_context_" << std::endl;
        for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
          free(dst[i]);
        }
        return false;
      }
    }

    return_value = sws_scale(sws_context_, frame->data, frame->linesize, 0,
                             frame->height, dst, dst_strides.data());
    if (return_value < 0) {
      std::cout << "ERROR: Failed to convert pixel format of frame"
                << std::endl;
      for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        free(dst[i]);
      }
      return false;
    }

    // put RGBA data into image
    image_.width_ = frame->width;
    image_.height_ = frame->height;
    if (grayscale) {
      image_.is_grayscale_ = true;
      image_.data_.resize(frame->width * frame->height);
      for (unsigned int i = 0; (int)i < frame->width * frame->height; ++i) {
        image_.data_.at(i) = dst[0][i];
      }
    } else {
      image_.is_grayscale_ = false;
      image_.data_.resize(frame->width * frame->height * 4);
      for (unsigned int y = 0; (int)y < frame->height; ++y) {
        for (unsigned int x = 0; (int)x < frame->width; ++x) {
          image_.data_.at(x * 4 + y * 4 * frame->width) =
              dst[0][x * 4 + y * 4 * frame->width];
          image_.data_.at(1 + x * 4 + y * 4 * frame->width) =
              dst[0][1 + x * 4 + y * 4 * frame->width];
          image_.data_.at(2 + x * 4 + y * 4 * frame->width) =
              dst[0][2 + x * 4 + y * 4 * frame->width];
          image_.data_.at(3 + x * 4 + y * 4 * frame->width) =
              dst[0][3 + x * 4 + y * 4 * frame->width];
        }
      }
    }

    std::unique_ptr<Image> dithered_image;
    if (grayscale) {
      dithered_image = image_.ToGrayscaleDitheredWithBlueNoise(blue_noise);
    } else {
      dithered_image = image_.ToColorDitheredWithBlueNoise(blue_noise);
    }

    std::string out_name = "output_";
    if (frame_count_ < 10) {
      out_name += "000" + std::to_string(frame_count_);
    } else if (frame_count_ < 100) {
      out_name += "00" + std::to_string(frame_count_);
    } else if (frame_count_ < 1000) {
      out_name += "0" + std::to_string(frame_count_);
    } else {
      out_name += std::to_string(frame_count_);
    }
    out_name += ".png";
    dithered_image->SaveAsPNG(out_name, false);
    // TODO encode video with dithered_image

    // cleanup
    for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
      free(dst[i]);
    }
  }

  return true;
}
