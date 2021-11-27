#include <iostream>

#include "image.h"

int main(int argc, char **argv) {
  // Image image("testin.ppm");
  // image.SaveAsPNG("testout.png", true);

  Image input("input.png");
  if (!input.IsValid()) {
    std::cout << "ERROR: input.png is invalid" << std::endl;
    return 1;
  }

  Image bluenoise("bluenoise.png");
  if (!bluenoise.IsValid()) {
    std::cout << "ERROR: bluenoise.png is invalid" << std::endl;
    return 1;
  }

  // auto output = input.ToGrayscaleDitheredWithBlueNoise(&bluenoise);
  auto output = input.ToColorDitheredWithBlueNoise(&bluenoise);
  if (!output || !output->IsValid()) {
    std::cout << "ERROR: output Image is invalid" << std::endl;
    return 1;
  }
  output->SaveAsPNG("output.png", true);

  return 0;
}
