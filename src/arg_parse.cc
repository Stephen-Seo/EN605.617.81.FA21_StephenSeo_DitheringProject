#include "arg_parse.h"

#include <cstring>
#include <iostream>

Args::Args()
    : do_dither_image_(true),
      do_dither_grayscaled_(false),
      do_overwrite_(false),
      input_filename(),
      output_filename() {}

void Args::PrintUsage() {
  std::cout
      << "Usage: [-h | --help] [-i <filename> | --input <filename>] [-o "
         "<filename> | --output <filename>] [-b <filename> | --blue "
         "<filename>] [-g | --gray] [--image] [--video] [--overwrite]\n"
         "  -h | --help\t\t\t\tPrint this usage text\n"
         "  -i <filename> | --input <filename>\tSet input filename\n"
         "  -o <filename> | --output <filename>\tSet output filename\n"
         "  -b <filename> | --blue <filename>\tSet input blue_noise filename\n"
         "  -g | --gray\t\t\t\tDither output in grayscale\n"
         "  --image\t\t\t\tDither a single image\n"
         "  --video\t\t\t\tDither frames in a video\n"
         "  --overwrite\t\t\t\tAllow overwriting existing files\n"
      << std::endl;
}

bool Args::ParseArgs(int argc, char **argv) {
  --argc;
  ++argv;
  while (argc > 0) {
    if (std::strcmp(argv[0], "-h") == 0 ||
        std::strcmp(argv[0], "--help") == 0) {
      PrintUsage();
      return true;
    } else if (argc > 1 && (std::strcmp(argv[0], "-i") == 0 ||
                            std::strcmp(argv[0], "--input") == 0)) {
      input_filename = std::string(argv[1]);
      --argc;
      ++argv;
    } else if (argc > 1 && (std::strcmp(argv[0], "-o") == 0 ||
                            std::strcmp(argv[0], "--output") == 0)) {
      output_filename = std::string(argv[1]);
      --argc;
      ++argv;
    } else if (argc > 1 && (std::strcmp(argv[0], "-b") == 0 ||
                            std::strcmp(argv[0], "--blue") == 0)) {
      blue_noise_filename = std::string(argv[1]);
      --argc;
      ++argv;
    } else if (std::strcmp(argv[0], "-g") == 0 ||
               std::strcmp(argv[0], "--gray") == 0) {
      do_dither_grayscaled_ = true;
    } else if (std::strcmp(argv[0], "--image") == 0) {
      do_dither_image_ = true;
    } else if (std::strcmp(argv[0], "--video") == 0) {
      do_dither_image_ = false;
    } else if (std::strcmp(argv[0], "--overwrite") == 0) {
      do_overwrite_ = true;
    } else {
      std::cout << "WARNING: Ignoring invalid input \"" << argv[0] << '"'
                << std::endl;
    }
    --argc;
    ++argv;
  }
  return false;
}
