#include <iostream>

#include "arg_parse.h"
#include "image.h"
#include "video.h"

int main(int argc, char **argv) {
  Args args{};
  if (args.ParseArgs(argc, argv)) {
    return 0;
  }

  Image blue_noise(args.blue_noise_filename);
  if (!blue_noise.IsValid() || !blue_noise.IsGrayscale()) {
    std::cout << "ERROR: Invalid blue noise file \"" << args.blue_noise_filename
              << '"' << std::endl;
    Args::PrintUsage();
    return 1;
  }

  if (args.do_dither_image_) {
    Image input_image(args.input_filename);
    if (!input_image.IsValid()) {
      std::cout << "ERROR: Invalid input image file \"" << args.input_filename
                << '"' << std::endl;
      Args::PrintUsage();
      return 2;
    }

    if (args.do_dither_grayscaled_) {
      auto output_image =
          input_image.ToGrayscaleDitheredWithBlueNoise(&blue_noise);
      if (!output_image) {
        std::cout << "ERROR: Failed to dither input image \""
                  << args.input_filename << '"' << std::endl;
        Args::PrintUsage();
        return 3;
      }
      if (!output_image->SaveAsPNG(args.output_filename, args.do_overwrite_)) {
        std::cout << "ERROR: Failed to saved dithered image from input \""
                  << args.input_filename << '"' << std::endl;
        Args::PrintUsage();
        return 4;
      }
    } else {
      auto output_image = input_image.ToColorDitheredWithBlueNoise(&blue_noise);
      if (!output_image) {
        std::cout << "ERROR: Failed to dither input image \""
                  << args.input_filename << '"' << std::endl;
        Args::PrintUsage();
        return 5;
      }
      if (!output_image->SaveAsPNG(args.output_filename, args.do_overwrite_)) {
        std::cout << "ERROR: Failed to saved dithered image from input \""
                  << args.input_filename << '"' << std::endl;
        Args::PrintUsage();
        return 6;
      }
    }
  } else {
    Video video(args.input_filename);
    if (!video.DitherVideo(args.output_filename, &blue_noise,
                           args.do_dither_grayscaled_, args.do_overwrite_)) {
      std::cout << "ERROR: Failed to dither frames from input video \""
                << args.input_filename << '"' << std::endl;
      Args::PrintUsage();
      return 7;
    }
  }

  return 0;
}
