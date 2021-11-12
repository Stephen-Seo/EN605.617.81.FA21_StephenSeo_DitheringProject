#include "image.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>

#include <png.h>

Image::Image() : data_(), width_(0), height_(0), is_grayscale_(true) {}

Image::Image(const char *filename) : Image(std::string(filename)) {}

Image::Image(const std::string &filename)
    : data_(), width_(0), height_(0), is_grayscale_(true) {
  if (filename.compare(filename.size() - 4, filename.size(), ".png") == 0) {
    // filename expected to be .png
    std::cout << "INFO: PNG filename extension detected, decoding..."
              << std::endl;
    DecodePNG(filename);
  } else if (filename.compare(filename.size() - 4, filename.size(), ".pgm") ==
             0) {
    // filename expected to be .pgm
    std::cout << "INFO: PGM filename extension detected, decoding..."
              << std::endl;
    DecodePGM(filename);
  } else if (filename.compare(filename.size() - 4, filename.size(), ".ppm") ==
             0) {
    // filename expected to be .ppm
    std::cout << "INFO: PPM filename extension detected, decoding..."
              << std::endl;
    DecodePPM(filename);
  } else {
    // unknown filename extension
    std::cout << "ERROR: Unknown filename extension" << std::endl;
    return;
  }
}

bool Image::IsValid() const {
  if (!data_.empty() && width_ > 0 && height_ > 0) {
    if (is_grayscale_ && data_.size() == width_ * height_) {
      return true;
    } else if (!is_grayscale_ && data_.size() == 4 * width_ * height_) {
      return true;
    }
  }
  return false;
}

uint8_t *Image::GetData() { return data_.data(); }

const uint8_t *Image::GetData() const { return data_.data(); }

unsigned int Image::GetSize() const { return data_.size(); }

unsigned int Image::GetWidth() const { return width_; }

unsigned int Image::GetHeight() const { return height_; }

bool Image::IsGrayscale() const { return is_grayscale_; }

bool Image::SaveAsPPM(const std::string &filename, bool overwrite,
                      bool packed) {
  if (!IsValid()) {
    std::cout << "ERROR: Image is not valid" << std::endl;
    return false;
  }

  if (!overwrite) {
    std::ifstream ifs(filename);
    if (ifs.is_open()) {
      std::cout << "ERROR: file with name \"" << filename
                << "\" already exists and overwite is not set to true"
                << std::endl;
      return false;
    }
  }

  std::ofstream ofs(filename);
  if (packed) {
    ofs << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for (unsigned int j = 0; j < height_; ++j) {
      for (unsigned int i = 0; i < width_; ++i) {
        if (is_grayscale_) {
          for (unsigned int c = 0; c < 3; ++c) {
            ofs.put(data_.at(i + j * width_));
          }
        } else {
          // data is stored as rgba, but ppm is rgb
          for (unsigned int c = 0; c < 3; ++c) {
            ofs.put(data_.at(c + i * 4 + j * width_ * 4));
          }
        }
      }
    }
  } else {
    ofs << "P3\n" << width_ << ' ' << height_ << "\n255\n";
    for (unsigned int j = 0; j < height_; ++j) {
      for (unsigned int i = 0; i < width_; ++i) {
        if (is_grayscale_) {
          int value = data_.at(i + j * width_);
          for (unsigned int c = 0; c < 3; ++c) {
            ofs << value << ' ';
          }
        } else {
          // data is stored as rgba, but ppm is rgb
          for (unsigned int c = 0; c < 3; ++c) {
            int value = data_.at(c + i * 4 + j * width_ * 4);
            ofs << value << ' ';
          }
        }
      }
      ofs << '\n';
    }
  }

  return true;
}

bool Image::SaveAsPPM(const char *filename, bool overwrite, bool packed) {
  return SaveAsPPM(std::string(filename), overwrite, packed);
}

void Image::DecodePNG(const std::string &filename) {
  FILE *file = std::fopen(filename.c_str(), "rb");

  // Check header of file to check if it is actually a png file.
  {
    std::array<unsigned char, 8> buf;
    if (std::fread(buf.data(), 1, 8, file) != 8) {
      std::fclose(file);
      std::cout << "ERROR: File \"" << filename << "\" is smaller than 8 bytes"
                << std::endl;
      return;
    } else if (png_sig_cmp(reinterpret_cast<png_const_bytep>(buf.data()), 0,
                           8) != 0) {
      // not png file, do nothing
      std::fclose(file);
      std::cout << "ERROR: File \"" << filename << "\" is not a png file"
                << std::endl;
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
  // have libpng process the png data
  png_read_png(png_ptr, png_info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

  // get image width (in pixels)
  width_ = png_get_image_width(png_ptr, png_info_ptr);

  // get image height (in pixels)
  height_ = png_get_image_height(png_ptr, png_info_ptr);

  // get channel count of image
  unsigned int channels = png_get_channels(png_ptr, png_info_ptr);
  if (channels == 1) {
    is_grayscale_ = true;
  } else {
    is_grayscale_ = false;
  }

  if (height_ > PNG_UINT_32_MAX) {
    png_error(png_ptr, "Image is too tall to process in memory");
  } else if (width_ > PNG_UINT_32_MAX) {
    png_error(png_ptr, "Image is too wide to process in memory");
  }

  png_byte **row_pointers = png_get_rows(png_ptr, png_info_ptr);

  data_.clear();
  if (channels == 3 || channels == 4) {
    data_.reserve(width_ * 4 * height_);
  } else if (channels == 1) {
    data_.reserve(width_ * height_);
  } else {
    std::cout << "ERROR: PNG has invalid channel count == " << channels
              << std::endl;
    png_destroy_read_struct(&png_ptr, &png_info_ptr, &png_end_info_ptr);
    return;
  }
  for (unsigned int y = 0; y < height_; ++y) {
    for (unsigned int x = 0; x < width_; ++x) {
      if (is_grayscale_) {
        data_.push_back(row_pointers[y][x]);
      } else if (channels == 3) {
        for (unsigned int c = 0; c < channels; ++c) {
          data_.push_back(row_pointers[y][x * channels + c]);
        }
        data_.push_back(255);
      } else /* if (channels == 4) */ {
        for (unsigned int c = 0; c < channels; ++c) {
          data_.push_back(row_pointers[y][x * channels + c]);
        }
      }
    }
  }

  // cleanup
  png_destroy_read_struct(&png_ptr, &png_info_ptr, &png_end_info_ptr);

  // verify
  if (is_grayscale_) {
    if (data_.size() != width_ * height_) {
      std::cout << "WARNING: data_.size() doesn't match width_ * height_"
                << std::endl;
    }
  } else {
    if (data_.size() != 4 * width_ * height_) {
      std::cout << "WARNING: data_.size() doesn't match 4 * width_ * height_"
                << std::endl;
    }
  }
}

void Image::DecodePGM(const std::string &filename) {
  // TODO
}

void Image::DecodePPM(const std::string &filename) {
  // TODO
}
