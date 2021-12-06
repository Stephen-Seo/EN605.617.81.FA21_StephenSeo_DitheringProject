#include "video.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

Video::Video(const char *video_filename) : Video(std::string(video_filename)) {}

Video::Video(const std::string &video_filename)
    : image_(),
      input_filename_(video_filename),
      sws_dec_context_(nullptr),
      sws_enc_context_(nullptr),
      frame_count_(0),
      packet_count_(0),
      was_grayscale_(false) {}

Video::~Video() {
  if (sws_dec_context_ != nullptr) {
    sws_freeContext(sws_dec_context_);
  }
}

bool Video::DitherVideo(const char *output_filename, Image *blue_noise,
                        bool grayscale, bool overwrite, bool output_as_pngs) {
  return DitherVideo(std::string(output_filename), blue_noise, grayscale,
                     overwrite, output_as_pngs);
}

bool Video::DitherVideo(const std::string &output_filename, Image *blue_noise,
                        bool grayscale, bool overwrite, bool output_as_pngs) {
  if (!overwrite && !output_as_pngs) {
    // check if output_file exists
    std::ifstream ifs(output_filename);
    if (ifs.is_open()) {
      std::cout << "ERROR: output file \"" << output_filename
                << "\" exists "
                   "and overwrite is disabled"
                << std::endl;
      return false;
    }
  }

  frame_count_ = 0;

  bool color_changed = false;
  if (was_grayscale_ != grayscale) {
    color_changed = true;
  }
  was_grayscale_ = grayscale;

  // set up decoding

  // Get AVFormatContext for input file
  AVFormatContext *avf_dec_context = nullptr;
  std::string url = std::string("file:") + input_filename_;
  int return_value =
      avformat_open_input(&avf_dec_context, url.c_str(), nullptr, nullptr);
  if (return_value != 0) {
    std::cout << "ERROR: Failed to open input file to determine format"
              << std::endl;
    return false;
  }

  // Read from input file to fill in info in AVFormatContext
  return_value = avformat_find_stream_info(avf_dec_context, nullptr);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to determine input file stream info"
              << std::endl;
    avformat_close_input(&avf_dec_context);
    return false;
  }

  // Get "best" video stream
  AVCodec *dec_codec = nullptr;
  return_value = av_find_best_stream(
      avf_dec_context, AVMediaType::AVMEDIA_TYPE_VIDEO, -1, -1, &dec_codec, 0);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to get video stream in input file" << std::endl;
    avformat_close_input(&avf_dec_context);
    return false;
  }
  int video_stream_idx = return_value;

  // Alloc codec context
  AVCodecContext *codec_ctx = avcodec_alloc_context3(dec_codec);
  if (!codec_ctx) {
    std::cout << "ERROR: Failed to alloc codec context" << std::endl;
    avformat_close_input(&avf_dec_context);
    return false;
  }

  // Set codec parameters from input stream
  return_value = avcodec_parameters_to_context(
      codec_ctx, avf_dec_context->streams[video_stream_idx]->codecpar);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to set codec parameters from input stream"
              << std::endl;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_dec_context);
    return false;
  }

  // Init codec context
  return_value = avcodec_open2(codec_ctx, dec_codec, nullptr);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to init codec context" << std::endl;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_dec_context);
    return false;
  }

  std::cout << "Dumping input video format info..." << std::endl;
  av_dump_format(avf_dec_context, video_stream_idx, input_filename_.c_str(), 0);

  // get input stream info
  unsigned int width =
      avf_dec_context->streams[video_stream_idx]->codecpar->width;
  unsigned int height =
      avf_dec_context->streams[video_stream_idx]->codecpar->height;

  // try to get frame rate from duration, nb_frames, and input time_base
  AVRational input_time_base =
      avf_dec_context->streams[video_stream_idx]->time_base;
  double duration = avf_dec_context->streams[video_stream_idx]->duration;
  double frames = avf_dec_context->streams[video_stream_idx]->nb_frames;
  AVRational time_base = {0, 0};
  if (duration > 0 && frames > 0 && input_time_base.num > 0 &&
      input_time_base.den > 0) {
    double fps = (double)input_time_base.den / (double)input_time_base.num /
                 (duration / frames);
    std::cout << "Got fps == " << fps << std::endl;
    if (fps > 0) {
      time_base.den = fps * 100000;
      time_base.num = 100000;
    }
  }

  if (time_base.num == 0 && time_base.den == 0) {
    if (avf_dec_context->streams[video_stream_idx]->codecpar->codec_id ==
        AV_CODEC_ID_H264) {
      // get time_base from avg_frame_rate
      AVRational *avg_frame_rate =
          &avf_dec_context->streams[video_stream_idx]->avg_frame_rate;
      time_base = {avg_frame_rate->den, avg_frame_rate->num};
    } else {
      // get time_base from r_frame_rate
      AVRational *r_frame_rate =
          &avf_dec_context->streams[video_stream_idx]->r_frame_rate;
      time_base = {r_frame_rate->den, r_frame_rate->num};
    }
  }
  std::cout << "Setting time_base of " << time_base.num << "/" << time_base.den
            << std::endl;

  // Alloc a packet object for reading packets
  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    std::cout << "ERROR: Failed to alloc an AVPacket" << std::endl;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_dec_context);
    return false;
  }

  // Alloc a frame object for reading frames
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    std::cout << "ERROR: Failed to alloc video frame object" << std::endl;
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_dec_context);
    return false;
  }

  // Set up encoding

  // alloc/init encoding AVFormatContext
  AVFormatContext *avf_enc_context = nullptr;
  if (!output_as_pngs) {
    return_value = avformat_alloc_output_context2(
        &avf_enc_context, nullptr, nullptr, output_filename.c_str());
    if (return_value < 0) {
      std::cout << "ERROR: Failed to alloc/init avf_enc_context" << std::endl;
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }
  }

  // set output video codec (h264)
  AVCodecContext *enc_codec_context = nullptr;
  AVCodec *enc_codec = nullptr;

  // get H264 codec
  if (!output_as_pngs) {
    enc_codec = avcodec_find_encoder(AVCodecID::AV_CODEC_ID_H264);
    if (enc_codec == nullptr) {
      std::cout << "ERROR: Failed to get H264 codec for encoding" << std::endl;
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }
  }

  // create new video stream
  AVStream *enc_stream = nullptr;
  if (!output_as_pngs) {
    enc_stream = avformat_new_stream(avf_enc_context, enc_codec);
    if (enc_stream == nullptr) {
      std::cout << "ERROR: Failed to create encoding stream" << std::endl;
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }
    // assign its id
    enc_stream->id = avf_enc_context->nb_streams - 1;
    // alloc enc AVCodecContext
    enc_codec_context = avcodec_alloc_context3(enc_codec);
    if (enc_codec_context == nullptr) {
      std::cout << "ERROR: Failed to create AVCodecContext for encoding"
                << std::endl;
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }

    // set values on enc_codec_context
    enc_codec_context->codec_id = AVCodecID::AV_CODEC_ID_H264;
    enc_codec_context->bit_rate = kOutputBitrate;
    enc_codec_context->width = width;
    enc_codec_context->height = height;
    enc_stream->time_base = time_base;
    enc_codec_context->time_base = time_base;
    enc_codec_context->gop_size = 128;
    enc_codec_context->global_quality = 23;
    enc_codec_context->qmax = 35;
    enc_codec_context->qmin = 20;
    enc_codec_context->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUV444P;
    if (avf_enc_context->oformat->flags & AVFMT_GLOBALHEADER) {
      enc_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // more init on enc_codec_context
    return_value = avcodec_open2(enc_codec_context, enc_codec, nullptr);
    if (return_value != 0) {
      std::cout << "ERROR: Failed to init enc_codec_context" << std::endl;
      avcodec_close(enc_codec_context);
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }

    return_value = avcodec_parameters_from_context(enc_stream->codecpar,
                                                   enc_codec_context);
    if (return_value < 0) {
      std::cout << "ERROR: Failed to set encoding codec parameters in stream"
                << std::endl;
      avcodec_close(enc_codec_context);
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }

    std::cout << "Dumping output video format info..." << std::endl;
    av_dump_format(avf_enc_context, enc_stream->id, output_filename.c_str(), 1);

    // open output file if needed
    if (!(avf_enc_context->oformat->flags & AVFMT_NOFILE)) {
      return_value = avio_open(&avf_enc_context->pb, output_filename.c_str(),
                               AVIO_FLAG_WRITE);
      if (return_value < 0) {
        std::cout << "ERROR: Failed to open file \"" << output_filename
                  << "\" for writing" << std::endl;
        avcodec_close(enc_codec_context);
        avformat_free_context(avf_enc_context);
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&avf_dec_context);
        return false;
      }
    }

    // write header
    return_value = avformat_write_header(avf_enc_context, nullptr);
    if (return_value < 0) {
      std::cout << "ERROR: Failed to write header in output video file"
                << std::endl;
      avcodec_close(enc_codec_context);
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }
  }  // if (!output_as_pngs)

  // do decoding, then encoding per frame

  // read frames
  while (av_read_frame(avf_dec_context, pkt) >= 0) {
    if (pkt->stream_index == video_stream_idx) {
      auto ret_tuple =
          HandleDecodingPacket(codec_ctx, pkt, frame, blue_noise, grayscale,
                               color_changed, output_as_pngs);
      if (!std::get<0>(ret_tuple)) {
        avcodec_close(enc_codec_context);
        avformat_free_context(avf_enc_context);
        av_frame_free(&frame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&avf_dec_context);
        return false;
      } else if (!output_as_pngs && !std::get<1>(ret_tuple).empty()) {
        for (auto *yuv_frame : std::get<1>(ret_tuple)) {
          if (!HandleEncodingFrame(avf_enc_context, enc_codec_context,
                                   yuv_frame, enc_stream)) {
            av_frame_free(&yuv_frame);
            avcodec_close(enc_codec_context);
            avformat_free_context(avf_enc_context);
            av_frame_free(&frame);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&avf_dec_context);
            return false;
          }
          av_frame_free(&yuv_frame);
        }
      }
    }
    av_packet_unref(pkt);
  }

  // flush decoders
  auto ret_tuple =
      HandleDecodingPacket(codec_ctx, nullptr, frame, blue_noise, grayscale,
                           color_changed, output_as_pngs);
  if (!std::get<0>(ret_tuple)) {
    avcodec_close(enc_codec_context);
    avformat_free_context(avf_enc_context);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&avf_dec_context);
    return false;
  } else if (!output_as_pngs && !std::get<1>(ret_tuple).empty()) {
    for (auto *yuv_frame : std::get<1>(ret_tuple)) {
      if (!HandleEncodingFrame(avf_enc_context, enc_codec_context, yuv_frame,
                               enc_stream)) {
        av_frame_free(&yuv_frame);
        avcodec_close(enc_codec_context);
        avformat_free_context(avf_enc_context);
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&avf_dec_context);
        return false;
      }
      av_frame_free(&yuv_frame);
    }
  }

  if (!output_as_pngs) {
    // flush encoder
    if (!HandleEncodingFrame(avf_enc_context, enc_codec_context, nullptr,
                             enc_stream)) {
      avcodec_close(enc_codec_context);
      avformat_free_context(avf_enc_context);
      av_frame_free(&frame);
      av_packet_free(&pkt);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&avf_dec_context);
      return false;
    }

    // finish encoding
    av_write_trailer(avf_enc_context);
  }

  // cleanup
  if (enc_codec_context) {
    avcodec_close(enc_codec_context);
  }
  if (!output_as_pngs && !(avf_enc_context->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&avf_enc_context->pb);
  }
  if (avf_enc_context) {
    avformat_free_context(avf_enc_context);
  }
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&avf_dec_context);
  return true;
}

std::tuple<bool, std::vector<AVFrame *>> Video::HandleDecodingPacket(
    AVCodecContext *codec_ctx, AVPacket *pkt, AVFrame *frame, Image *blue_noise,
    bool grayscale, bool color_changed, bool output_as_pngs) {
  int return_value = avcodec_send_packet(codec_ctx, pkt);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to decode packet (" << packet_count_ << ')'
              << std::endl;
    return {false, {}};
  }

  return_value = 0;
  std::vector<AVFrame *> return_frames{};

  while (return_value >= 0) {
    return_value = avcodec_receive_frame(codec_ctx, frame);
    if (return_value == AVERROR(EAGAIN) || return_value == AVERROR_EOF) {
      return {true, return_frames};
    } else if (return_value < 0) {
      std::cout << "ERROR: Failed to get frame from decoded packet(s)"
                << std::endl;
      return {false, {}};
    }
    ++frame_count_;

    std::cout << "Frame " << frame_count_ << std::endl;  // TODO DEBUG

    AVFrame *temp_frame = av_frame_alloc();
    temp_frame->format = AVPixelFormat::AV_PIX_FMT_RGBA;
    temp_frame->width = frame->width;
    temp_frame->height = frame->height;
    return_value = av_frame_get_buffer(temp_frame, 0);
    if (return_value != 0) {
      std::cout << "ERROR: Failed to init temp_frame to receive RGBA data"
                << std::endl;
      av_frame_free(&temp_frame);
      return {false, {}};
    }

    // Convert colors to RGBA
    if (sws_dec_context_ == nullptr) {
      sws_dec_context_ = sws_getContext(
          frame->width, frame->height, (AVPixelFormat)frame->format,
          frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_RGBA,
          SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (sws_dec_context_ == nullptr) {
        std::cout << "ERROR: Failed to init sws_dec_context_" << std::endl;
        av_frame_free(&temp_frame);
        return {false, {}};
      }
    }

    return_value =
        sws_scale(sws_dec_context_, frame->data, frame->linesize, 0,
                  frame->height, temp_frame->data, temp_frame->linesize);
    if (return_value < 0) {
      std::cout << "ERROR: Failed to convert pixel format of frame"
                << std::endl;
      av_frame_free(&temp_frame);
      return {false, {}};
    }

    // put RGBA data into image
    image_.width_ = frame->width;
    image_.height_ = frame->height;
    image_.is_grayscale_ = false;
    image_.data_.resize(frame->width * frame->height * 4);
    for (unsigned int y = 0; (int)y < frame->height; ++y) {
      for (unsigned int x = 0; (int)x < frame->width; ++x) {
        image_.data_.at(x * 4 + y * 4 * frame->width) =
            temp_frame->data[0][x * 4 + y * 4 * frame->width];
        image_.data_.at(1 + x * 4 + y * 4 * frame->width) =
            temp_frame->data[0][1 + x * 4 + y * 4 * frame->width];
        image_.data_.at(2 + x * 4 + y * 4 * frame->width) =
            temp_frame->data[0][2 + x * 4 + y * 4 * frame->width];
        image_.data_.at(3 + x * 4 + y * 4 * frame->width) =
            temp_frame->data[0][3 + x * 4 + y * 4 * frame->width];
      }
    }

    av_frame_unref(temp_frame);

    std::unique_ptr<Image> dithered_image;
    if (grayscale) {
      dithered_image = image_.ToGrayscaleDitheredWithBlueNoise(blue_noise);
    } else {
      dithered_image = image_.ToColorDitheredWithBlueNoise(blue_noise);
    }
    if (!dithered_image) {
      std::cout << "ERROR: Failed to dither video frame" << std::endl;
      return {false, {}};
    }

    if (output_as_pngs) {
      // free temp_frame as it will not be used on this branch
      av_frame_free(&temp_frame);
      // get png output name padded with zeroes
      std::string out_name = "output_";
      unsigned int tens = 1;
      for (unsigned int i = 0; i < 9; ++i) {
        if (frame_count_ < tens) {
          out_name += "0";
        }
        tens *= 10;
      }
      out_name += std::to_string(frame_count_);
      out_name += ".png";
      // write png from frame
      if (!dithered_image->SaveAsPNG(out_name, true)) {
        return {false, {}};
      }
    } else {
      // convert grayscale/RGBA to YUV444p
      if (sws_enc_context_ != nullptr && color_changed) {
        // switched between grayscale/RGBA, context needs to be recreated
        sws_freeContext(sws_enc_context_);
        sws_enc_context_ = nullptr;
      }
      if (sws_enc_context_ == nullptr) {
        sws_enc_context_ = sws_getContext(
            frame->width, frame->height,
            grayscale ? AVPixelFormat::AV_PIX_FMT_GRAY8
                      : AVPixelFormat::AV_PIX_FMT_RGBA,
            frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_YUV444P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (sws_enc_context_ == nullptr) {
          std::cout << "ERROR: Failed to init sws_enc_context_" << std::endl;
          return {false, {}};
        }
      }

      // rgba data info
      if (grayscale) {
        av_frame_free(&temp_frame);
        temp_frame = av_frame_alloc();
        temp_frame->format = AVPixelFormat::AV_PIX_FMT_GRAY8;
        temp_frame->width = frame->width;
        temp_frame->height = frame->height;
        return_value = av_frame_get_buffer(temp_frame, 0);
        if (return_value != 0) {
          std::cout << "ERROR: Failed to init temp_frame for conversion from "
                       "grayscale"
                    << std::endl;
          av_frame_free(&temp_frame);
          return {false, {}};
        }
        std::memcpy(temp_frame->data[0], dithered_image->data_.data(),
                    frame->width * frame->height);
      } else {
        temp_frame->format = AVPixelFormat::AV_PIX_FMT_RGBA;
        temp_frame->width = frame->width;
        temp_frame->height = frame->height;
        return_value = av_frame_get_buffer(temp_frame, 0);
        if (return_value != 0) {
          std::cout
              << "ERROR: Failed to init temp_frame for conversion from RGBA"
              << std::endl;
          av_frame_free(&temp_frame);
          return {false, {}};
        }
        std::memcpy(temp_frame->data[0], dithered_image->data_.data(),
                    4 * frame->width * frame->height);
      }

      AVFrame *yuv_frame = av_frame_alloc();
      if (frame == nullptr) {
        std::cout
            << "ERROR: Failed to alloc AVFrame for receiving YUV444p from RGBA"
            << std::endl;
        av_frame_free(&temp_frame);
        return {false, {}};
      }
      yuv_frame->format = AVPixelFormat::AV_PIX_FMT_YUV444P;
      yuv_frame->width = frame->width;
      yuv_frame->height = frame->height;
      return_value = av_frame_get_buffer(yuv_frame, 0);

      return_value =
          sws_scale(sws_enc_context_, temp_frame->data, temp_frame->linesize, 0,
                    frame->height, yuv_frame->data, yuv_frame->linesize);
      if (return_value <= 0) {
        std::cout << "ERROR: Failed to convert RGBA to YUV444p with sws_scale"
                  << std::endl;
        av_frame_free(&yuv_frame);
        av_frame_free(&temp_frame);
        return {false, {}};
      }

      // cleanup
      av_frame_free(&temp_frame);
      yuv_frame->pts = frame_count_ - 1;
      yuv_frame->pkt_duration = 1;
      return_frames.push_back(yuv_frame);
    }  // else (!output_as_pngs)
  }    // while (return_value >= 0)

  return {true, return_frames};
}

bool Video::HandleEncodingFrame(AVFormatContext *enc_format_ctx,
                                AVCodecContext *enc_codec_ctx,
                                AVFrame *yuv_frame, AVStream *video_stream) {
  int return_value;

  return_value = avcodec_send_frame(enc_codec_ctx, yuv_frame);
  if (return_value < 0) {
    std::cout << "ERROR: Failed to send frame to encoder" << std::endl;
    return false;
  }

  AVPacket pkt;
  std::memset(&pkt, 0, sizeof(AVPacket));
  while (return_value >= 0) {
    std::memset(&pkt, 0, sizeof(AVPacket));

    return_value = avcodec_receive_packet(enc_codec_ctx, &pkt);
    if (return_value == AVERROR(EAGAIN) || return_value == AVERROR_EOF) {
      break;
    } else if (return_value < 0) {
      std::cout << "ERROR: Failed to encode a frame" << std::endl;
      return false;
    }

    // rescale timing fields (timestamps / durations)
    av_packet_rescale_ts(&pkt, enc_codec_ctx->time_base,
                         video_stream->time_base);
    pkt.stream_index = video_stream->index;

    // write frame
    return_value = av_interleaved_write_frame(enc_format_ctx, &pkt);
    av_packet_unref(&pkt);
    if (return_value < 0) {
      std::cout << "ERROR: Failed to write encoding packet" << std::endl;
      return false;
    }
  }

  return true;
}
