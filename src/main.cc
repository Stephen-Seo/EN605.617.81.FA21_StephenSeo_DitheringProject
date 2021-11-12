#include "image.h"

int main(int argc, char **argv) {
  Image image("testin.png");

  image.SaveAsPNG("testout.png", true);

  return 0;
}
