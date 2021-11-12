#include "image.h"

int main(int argc, char **argv) {
  Image image("testin.png");

  image.SaveAsPPM("testout.ppm", true, true);

  return 0;
}
