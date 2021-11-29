#include "video.h"

#include <cstring>
#include <fstream>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

Video::Video(const char *video_filename) : Video(std::string(video_filename)) {}

Video::Video(const std::string &video_filename)
    : image(), input_filename(video_filename) {}

bool Video::DitherGrayscale(const char *output_filename) {
  return DitherGrayscale(std::string(output_filename));
}

bool Video::DitherGrayscale(const std::string &output_filename) {
  // determine input file format

  // Get AVFormatContext for input file
  AVFormatContext *avf_context = nullptr;
  std::string url = std::string("file:") + input_filename;
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

  // cleanup AVFormatContext as it is no longer needed
  avformat_close_input(&avf_context);

  // Init required objects for decoding

  // Init parser
  AVCodecParserContext *parser = av_parser_init(avcodec->id);
  if (!parser) {
    std::cout << "ERROR: Failed to init codec parser" << std::endl;
    return false;
  }

  // Alloc codec context
  AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec);
  if (!codec_ctx) {
    std::cout << "ERROR: Failed to alloc codec context" << std::endl;
    av_parser_close(parser);
    return false;
  }

  // Init codec context
  return_value = avcodec_open2(codec_ctx, avcodec, nullptr);
  if (return_value == 0) {
    std::cout << "ERROR: Failed to init codec context" << std::endl;
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    return false;
  }

  // Alloc a packet object for reading packets
  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    std::cout << "ERROR: Failed to alloc an AVPacket" << std::endl;
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    return false;
  }

  // Alloc a frame object for reading frames
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    std::cout << "ERROR: Failed to alloc video frame object" << std::endl;
    av_packet_free(&pkt);
    av_parser_close(parser);
    avcodec_free_context(&codec_ctx);
    return false;
  }

  // Now the file will be opened for decoding the "best" video stream
  std::ifstream ifs(input_filename);
  if (!ifs.is_open() || !ifs.good()) {
    std::cout << "ERROR: Failed to open input file \"" << input_filename << '"'
              << std::endl;
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    return false;
  }

  // Set up buffer to read from input file
  std::array<uint8_t, kReadBufSizeWithPadding> buf;
  // Fill end of buffer with 0 to avoid possible overreading (as shown in
  // example code)
  std::memset(buf.data() + kReadBufSize, 0, kReadBufPaddingSize);

  std::streamsize read_count;
  uint8_t *data_ptr;
  while (ifs.good()) {
    ifs.read(reinterpret_cast<char *>(buf.data()), kReadBufSize);
    read_count = ifs.gcount();
    data_ptr = buf.data();
    if (read_count == 0) {
      // read 0 bytes, probably reached exactly EOF
      break;
    }

    while (read_count > 0) {
      return_value =
          av_parser_parse2(parser, codec_ctx, &pkt->data, &pkt->size, data_ptr,
                           read_count, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (return_value < 0) {
        std::cout << "ERROR: Failed to parse input file" << std::endl;
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        av_parser_close(parser);
        return false;
      }
      data_ptr += return_value;
      read_count -= return_value;

      if (pkt->size) {
        // TODO use packet
      }
    }
  }

  if (ifs.fail()) {
    std::cout << "ERROR: Read error on input file" << std::endl;
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    return false;
  }

  // TODO flush decoder

  // cleanup
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&codec_ctx);
  av_parser_close(parser);
  return true;
}
