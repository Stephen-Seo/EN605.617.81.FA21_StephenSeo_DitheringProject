#include "image.h"

#include <array>
#include <cstdio>
#include <iostream>

#include <png.h>

Image::Image() : data_(), width_(0), height_(0), is_grayscale_(true) {}

Image::Image(const char *filename) : Image(std::string(filename)) {}

Image::Image(const std::string &filename)
    : data_(), width_(0), height_(0), is_grayscale_(true) {
  if (filename.compare(filename.size() - 4, filename.size(), ".png") == 0) {
    // filename expected to be .png
    DecodePNG(filename);
  } else if (filename.compare(filename.size() - 4, filename.size(), ".pgm") ==
             0) {
    // filename expected to be .pgm
    DecodePGM(filename);
  } else if (filename.compare(filename.size() - 4, filename.size(), ".ppm") ==
             0) {
    // filename expected to be .ppm
    DecodePPM(filename);
  } else {
    // unknown filename extension
    return;
  }
}

bool Image::IsValid() const {
  return !data_.empty() && width_ > 0 && height_ > 0;
}

uint8_t *Image::GetData() { return data_.data(); }

const uint8_t *Image::GetData() const { return data_.data(); }

unsigned int Image::GetSize() const { return data_.size(); }

unsigned int Image::GetWidth() const { return width_; }

unsigned int Image::GetHeight() const { return height_; }

bool Image::IsGrayscale() const { return is_grayscale_; }

void Image::DecodePNG(const std::string &filename) {
  FILE *file = std::fopen(filename.c_str(), "rb");

  // Check header of file to check if it is actually a png file.
  {
    std::array<unsigned char, 8> buf;
    if (std::fread(buf.data(), 1, 8, file) != 8) {
      std::cout << "ERROR: File \"" << filename << "\" is smaller than 8 bytes"
                << std::endl;
      std::fclose(file);
      return;
    } else if (!png_sig_cmp(reinterpret_cast<png_const_bytep>(buf.data()), 0,
                            8)) {
      // not png file, do nothing
      std::fclose(file);
      return;
    }
  }

  // seek to head of file
  std::rewind(file);

  // init required structs for png decoding
  png_structp png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    std::cout << "ERROR: Failed to initialize libpng (png_ptr) for decoding "
                 "PNG file \""
              << filename << '"' << std::endl;
    std::fclose(file);
    return;
  }

  png_infop png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr) {
    std::cout << "ERROR: Failed to initialize libpng (png_infop) for decoding "
                 "PNG file \""
              << filename << '"' << std::endl;
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    std::fclose(file);
    return;
  }

  png_infop png_end_info_ptr = png_create_info_struct(png_ptr);
  if (!png_end_info_ptr) {
    std::cout << "ERROR: Failed to initialize libpng (end png_infop) for "
                 "decoding PNG file \""
              << filename << '"' << std::endl;
    png_destroy_read_struct(&png_ptr, &png_info_ptr, nullptr);
    std::fclose(file);
    return;
  }

  // required to handle libpng errors
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &png_info_ptr, &png_end_info_ptr);
    std::fclose(file);
    return;
  }

  // pass the FILE pointer to libpng
  png_init_io(png_ptr, file);

  // TODO BEGIN
  //// have libpng process the png data
  // png_read_png(png_ptr, png_info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

  //// get rows of pixels from libpng
  // png_bytep row_pointer = nullptr;
  // png_read_row(png_ptr, row_pointer, nullptr);

  // TODO END

  // cleanup
  png_destroy_read_struct(&png_ptr, &png_info_ptr, &png_end_info_ptr);
}

void Image::DecodePGM(const std::string &filename) {
  // TODO
}

void Image::DecodePPM(const std::string &filename) {
  // TODO
}
