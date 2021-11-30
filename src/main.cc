#include <iostream>

#include "image.h"
#include "video.h"

int main(int argc, char **argv) {
  Image blue_noise("bluenoise.png");
  if (!blue_noise.IsValid()) {
    std::cout << "ERROR: Invalid bluenoise.png" << std::endl;
    return 1;
  }
  Video video("input.mp4");
  if (!video.DitherVideo("output.mp4", &blue_noise)) {
    std::cout << "ERROR: Failed to dither video" << std::endl;
    return 1;
  }

  return 0;
}
