#ifndef IGPUP_DITHERING_PROJECT_ARG_PARSE_
#define IGPUP_DITHERING_PROJECT_ARG_PARSE_

#include <string>

struct Args {
  Args();

  static void PrintUsage();

  /// Returns true if help was printed
  bool ParseArgs(int argc, char **argv);

  bool do_dither_image_;
  bool do_dither_grayscaled_;
  bool do_overwrite_;
  std::string input_filename;
  std::string output_filename;
  std::string blue_noise_filename;
};

#endif
