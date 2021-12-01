#include "image.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>

#define IGPUP_PROJECT_GRAYSCALE_KERNEL_NAME_ "GrayscaleDither"
#define IGPUP_PROJECT_COLOR_KERNEL_NAME_ "ColorDither"

const char *Image::kOpenCLGrayscaleKernel = nullptr;
const char *Image::kOpenCLColorKernel = nullptr;

const std::string Image::kBufferInputName = "DitherBufferInput";
const std::string Image::kBufferOutputName = "DitherBufferOutput";
const std::string Image::kBufferBlueNoiseName = "DitherBufferBlueNoise";
const std::string Image::kBufferBlueNoiseOffsetsName =
    "DitherBufferBlueNoiseOffsets";

const std::string Image::kGrayscaleKernelName =
    IGPUP_PROJECT_GRAYSCALE_KERNEL_NAME_;
const std::string Image::kColorKernelName = IGPUP_PROJECT_COLOR_KERNEL_NAME_;
const std::string Image::kEmptyString = {};

const std::array<png_color, 2> Image::kDitherBWPalette = {
    png_color{0, 0, 0},       // black
    png_color{255, 255, 255}  // white
};

const std::array<png_color, 8> Image::kDitherColorPalette = {
    png_color{0, 0, 0},        // black
    png_color{255, 255, 255},  // white
    png_color{255, 0, 0},      // red
    png_color{0, 255, 0},      // green
    png_color{0, 0, 255},      // blue
    png_color{255, 255, 0},    // yellow
    png_color{255, 0, 255},    // magenta
    png_color{0, 255, 255},    // cyan
};

Image::Image()
    : blue_noise_offsets_{0, 0, 0},
      data_(),
      width_(0),
      height_(0),
      is_grayscale_(true),
      is_dithered_grayscale_(false),
      is_dithered_color_(false),
      is_preserving_blue_noise_offsets_(true) {
  GenerateBlueNoiseOffsets();
}

Image::Image(const char *filename) : Image(std::string(filename)) {}

Image::Image(const std::string &filename)
    : blue_noise_offsets_{0, 0, 0},
      data_(),
      width_(0),
      height_(0),
      is_grayscale_(true),
      is_dithered_grayscale_(false),
      is_dithered_color_(false),
      is_preserving_blue_noise_offsets_(true) {
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

  GenerateBlueNoiseOffsets();
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

bool Image::SaveAsPNG(const std::string &filename, bool overwrite) {
  if (!overwrite) {
    std::ifstream ifs(filename);
    if (ifs.is_open()) {
      std::cout << "ERROR: File \"" << filename
                << "\" exists but overwrite == false" << std::endl;
      return false;
    }
  }

  FILE *file = std::fopen(filename.c_str(), "wb");
  if (!file) {
    std::cout << "ERROR: Failed to open file \"" << filename
              << "\" for writing png" << std::endl;
    return false;
  }

  // init required structs for png encoding
  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    std::cout << "ERROR: Failed to initialize libpng (png_ptr) for encoding "
                 "PNG file \""
              << filename << '"' << std::endl;
    std::fclose(file);
    return false;
  }

  png_infop png_info_ptr = png_create_info_struct(png_ptr);
  if (!png_info_ptr) {
    std::cout << "ERROR: Failed to initialize libpng (png_infop) for decoding "
                 "PNG file \""
              << filename << '"' << std::endl;
    png_destroy_write_struct(&png_ptr, nullptr);
    std::fclose(file);
    return false;
  }

  // required to handle libpng errors
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &png_info_ptr);
    std::fclose(file);
    return false;
  }

  // give FILE handle to libpng
  png_init_io(png_ptr, file);

  // set image information
  if (is_grayscale_) {
    if (is_dithered_grayscale_) {
      png_set_IHDR(png_ptr, png_info_ptr, width_, height_, 1,
                   PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      png_set_PLTE(png_ptr, png_info_ptr, kDitherBWPalette.data(),
                   kDitherBWPalette.size());
    } else {
      png_set_IHDR(png_ptr, png_info_ptr, width_, height_, 8,
                   PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    }
  } else {
    if (is_dithered_color_) {
      png_set_IHDR(png_ptr, png_info_ptr, width_, height_, 4,
                   PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      png_set_PLTE(png_ptr, png_info_ptr, kDitherColorPalette.data(),
                   kDitherColorPalette.size());
    } else {
      png_set_IHDR(png_ptr, png_info_ptr, width_, height_, 8,
                   PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    }
  }

  // write png info
  png_write_info(png_ptr, png_info_ptr);

  // write rows of image data
  if (is_dithered_grayscale_) {
    // using 1-bit palette in 1-bit color depth
    std::vector<unsigned char> row;
    unsigned char temp;
    unsigned int bidx;
    for (unsigned int y = 0; y < height_; ++y) {
      row.clear();
      temp = 0;
      bidx = 0;
      for (unsigned int x = 0; x < width_; ++x) {
        if (data_.at(x + y * width_) != 0) {
          temp |= 0x80 >> bidx;
        }
        ++bidx;
        if (bidx >= 8) {
          row.push_back(temp);
          temp = 0;
          bidx = 0;
        }
      }
      row.push_back(temp);
      png_write_row(png_ptr, row.data());
    }
  } else if (is_dithered_color_) {
    // using 3-bit palette in 4-bit color depth
    std::vector<unsigned char> row;
    unsigned char temp;
    unsigned int bidx;
    unsigned char red, green, blue;
    for (unsigned int y = 0; y < height_; ++y) {
      row.clear();
      temp = 0;
      bidx = 0;
      for (unsigned int x = 0; x < width_; ++x) {
        red = data_.at(x * 4 + y * 4 * width_);
        green = data_.at(x * 4 + y * 4 * width_ + 1);
        blue = data_.at(x * 4 + y * 4 * width_ + 2);
        if (red == 0 && green == 0 && blue == 0) {
          // noop
        } else if (red != 0 && green != 0 && blue != 0) {
          temp |= 0x10 >> (bidx * 4);
        } else if (red != 0 && green == 0 && blue == 0) {
          temp |= 0x20 >> (bidx * 4);
        } else if (red == 0 && green != 0 && blue == 0) {
          temp |= 0x30 >> (bidx * 4);
        } else if (red == 0 && green == 0 && blue != 0) {
          temp |= 0x40 >> (bidx * 4);
        } else if (red != 0 && green != 0 && blue == 0) {
          temp |= 0x50 >> (bidx * 4);
        } else if (red != 0 && green == 0 && blue != 0) {
          temp |= 0x60 >> (bidx * 4);
        } else if (red == 0 && green != 0 && blue != 0) {
          temp |= 0x70 >> (bidx * 4);
        } else {
          assert(!"Unreachable");
        }
        ++bidx;
        if (bidx >= 2) {
          bidx = 0;
          row.push_back(temp);
          temp = 0;
        }
      }
      row.push_back(temp);
      png_write_row(png_ptr, row.data());
    }
  } else {
    // using full 8-bit per-channel colors
    for (unsigned int y = 0; y < height_; ++y) {
      if (is_grayscale_) {
        png_write_row(png_ptr, &data_.at(y * width_));
      } else {
        png_write_row(png_ptr, &data_.at(y * width_ * 4));
      }
    }
  }

  // finish writing image data
  png_write_end(png_ptr, png_info_ptr);

  // cleanup
  png_destroy_write_struct(&png_ptr, &png_info_ptr);
  fclose(file);
  return true;
}

bool Image::SaveAsPNG(const char *filename, bool overwrite) {
  return SaveAsPNG(std::string(filename), overwrite);
}

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

uint8_t Image::ColorToGray(uint8_t red, uint8_t green, uint8_t blue) {
  // values taken from Wikipedia article about conversion of color to grayscale
  double y_linear = 0.2126 * (red / 255.0) + 0.7152 * (green / 255.0) +
                    0.0722 * (blue / 255.0);

  if (y_linear <= 0.0031308) {
    return std::round((12.92 * y_linear) * 255.0);
  } else {
    return std::round((1.055 * std::pow(y_linear, 1 / 2.4) - 0.055) * 255.0);
  }
}

std::unique_ptr<Image> Image::ToGrayscale() const {
  if (IsGrayscale()) {
    return std::unique_ptr<Image>(new Image(*this));
  }

  std::unique_ptr<Image> grayscale_image = std::unique_ptr<Image>(new Image{});
  grayscale_image->width_ = this->width_;
  grayscale_image->height_ = this->height_;
  grayscale_image->data_.resize(width_ * height_);

  for (unsigned int i = 0; i < width_ * height_; ++i) {
    grayscale_image->data_.at(i) =
        ColorToGray(this->data_.at(i * 4), this->data_.at(i * 4 + 1),
                    this->data_.at(i * 4 + 2));
  }

  return grayscale_image;
}

std::unique_ptr<Image> Image::ToGrayscaleDitheredWithBlueNoise(
    Image *blue_noise) {
  if (!blue_noise->IsGrayscale()) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: blue_noise is not grayscale"
        << std::endl;
    return {};
  }

  auto grayscale_image = ToGrayscale();
  if (!grayscale_image) {
    std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to get "
                 "grayscale Image"
              << std::endl;
    return {};
  }
  grayscale_image->is_dithered_grayscale_ = true;
  auto opencl_handle = GetOpenCLHandle();
  if (!opencl_handle) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to get OpenCLHandle"
        << std::endl;
    return {};
  }

  // first check if existing kernel/buffers can be used
  if (opencl_handle->HasKernel(kGrayscaleKernelName) &&
      !VerifyOpenCLBuffers(
          kGrayscaleKernelName,
          {kBufferInputName, kBufferOutputName, kBufferBlueNoiseName},
          grayscale_image.get(), blue_noise)) {
    opencl_handle->CleanupKernel(kGrayscaleKernelName);
  }

  // set up kernel and buffers
  const std::string &grayscale_kernel_name = GetGrayscaleKernelName();
  if (grayscale_kernel_name.empty() ||
      !opencl_handle->HasKernel(grayscale_kernel_name)) {
    std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to init kernel"
              << std::endl;
    return {};
  }

  if (!opencl_handle->HasBuffer(grayscale_kernel_name, kBufferInputName)) {
    if (!opencl_handle->CreateKernelBuffer(
            grayscale_kernel_name, CL_MEM_READ_ONLY,
            grayscale_image->data_.size(), nullptr, kBufferInputName)) {
      std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to alloc "
                   "input buffer"
                << std::endl;
      opencl_handle->CleanupKernel(grayscale_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->SetKernelBufferData(
          grayscale_kernel_name, kBufferInputName,
          grayscale_image->data_.size(), grayscale_image->data_.data())) {
    std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to init "
                 "input buffer"
              << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  if (!opencl_handle->HasBuffer(grayscale_kernel_name, kBufferOutputName)) {
    if (!opencl_handle->CreateKernelBuffer(
            grayscale_kernel_name, CL_MEM_WRITE_ONLY,
            grayscale_image->data_.size(), nullptr, kBufferOutputName)) {
      std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set "
                   "output buffer"
                << std::endl;
      opencl_handle->CleanupKernel(grayscale_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->HasBuffer(grayscale_kernel_name, kBufferBlueNoiseName)) {
    if (!opencl_handle->CreateKernelBuffer(
            grayscale_kernel_name, CL_MEM_READ_ONLY, blue_noise->data_.size(),
            nullptr, kBufferBlueNoiseName)) {
      std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to alloc "
                   "blue-noise buffer"
                << std::endl;
      opencl_handle->CleanupKernel(grayscale_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->SetKernelBufferData(
          grayscale_kernel_name, kBufferBlueNoiseName, blue_noise->data_.size(),
          blue_noise->data_.data())) {
    std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to init "
                 "blue-noise buffer"
              << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  // assign buffers/data to kernel parameters
  if (!opencl_handle->AssignKernelBuffer(grayscale_kernel_name, 0,
                                         kBufferInputName)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 0"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  if (!opencl_handle->AssignKernelBuffer(grayscale_kernel_name, 1,
                                         kBufferBlueNoiseName)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 1"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  if (!opencl_handle->AssignKernelBuffer(grayscale_kernel_name, 2,
                                         kBufferOutputName)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 2"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  unsigned int width = grayscale_image->GetWidth();
  if (!opencl_handle->AssignKernelArgument(grayscale_kernel_name, 3,
                                           sizeof(unsigned int), &width)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 3"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  unsigned int height = grayscale_image->GetHeight();
  if (!opencl_handle->AssignKernelArgument(grayscale_kernel_name, 4,
                                           sizeof(unsigned int), &height)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 4"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  unsigned int blue_noise_width = blue_noise->GetWidth();
  if (!opencl_handle->AssignKernelArgument(
          grayscale_kernel_name, 5, sizeof(unsigned int), &blue_noise_width)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 5"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }
  unsigned int blue_noise_height = blue_noise->GetHeight();
  if (!opencl_handle->AssignKernelArgument(
          grayscale_kernel_name, 6, sizeof(unsigned int), &blue_noise_height)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 6"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  if (!is_preserving_blue_noise_offsets_) {
    GenerateBlueNoiseOffsets();
  }
  if (!opencl_handle->AssignKernelArgument(grayscale_kernel_name, 7,
                                           sizeof(unsigned int),
                                           &blue_noise_offsets_.at(0))) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to set parameter 7"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  auto work_group_size = opencl_handle->GetWorkGroupSize(grayscale_kernel_name);
  // DEBUG
  // std::cout << "Got work_group_size == " << work_group_size << std::endl;

  std::size_t work_group_size_0 = std::sqrt(work_group_size);
  std::size_t work_group_size_1 = work_group_size_0;

  while (work_group_size_0 > 1 && width % work_group_size_0 != 0) {
    --work_group_size_0;
  }
  while (work_group_size_1 > 1 && height % work_group_size_1 != 0) {
    --work_group_size_1;
  }

  // DEBUG
  // std::cout << "Using WIDTHxHEIGHT: " << width << "x" << height
  //          << " with work_group_sizes: " << work_group_size_0 << "x"
  //          << work_group_size_1 << std::endl;

  if (!opencl_handle->ExecuteKernel2D(grayscale_kernel_name, width, height,
                                      work_group_size_0, work_group_size_1,
                                      true)) {
    std::cout
        << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to execute Kernel"
        << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  if (!opencl_handle->GetBufferData(grayscale_kernel_name, kBufferOutputName,
                                    grayscale_image->GetSize(),
                                    grayscale_image->data_.data())) {
    std::cout << "ERROR ToGrayscaleDitheredWithBlueNoise: Failed to get output "
                 "buffer data"
              << std::endl;
    opencl_handle->CleanupKernel(grayscale_kernel_name);
    return {};
  }

  return grayscale_image;
}

std::unique_ptr<Image> Image::ToColorDitheredWithBlueNoise(Image *blue_noise) {
  if (!blue_noise->IsGrayscale()) {
    std::cout
        << "ERROR ToColorDitheredWithBlueNoise: blue_noise is not grayscale"
        << std::endl;
    return {};
  }

  if (this->IsGrayscale()) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: current Image is not "
                 "non-grayscale"
              << std::endl;
    return {};
  }

  auto opencl_handle = GetOpenCLHandle();
  if (!opencl_handle) {
    std::cout
        << "ERROR ToColorDitheredWithBlueNoise: Failed to get OpenCLHandle"
        << std::endl;
    return {};
  }

  // first check if existing kernel/buffers can be used
  if (opencl_handle->HasKernel(kColorKernelName) &&
      !VerifyOpenCLBuffers(
          kColorKernelName,
          {kBufferInputName, kBufferOutputName, kBufferBlueNoiseName}, this,
          blue_noise)) {
    opencl_handle->CleanupKernel(kColorKernelName);
  }

  // set up kernel and buffers
  const std::string &color_kernel_name = GetColorKernelName();
  if (color_kernel_name.empty() ||
      !opencl_handle->HasKernel(color_kernel_name)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to init "
                 "OpenCL Kernel"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  if (!opencl_handle->HasBuffer(color_kernel_name, kBufferInputName)) {
    if (!opencl_handle->CreateKernelBuffer(color_kernel_name, CL_MEM_READ_ONLY,
                                           this->data_.size(), nullptr,
                                           kBufferInputName)) {
      std::cout
          << "ERROR ToColorDitheredWithBlueNoise: Failed to alloc input buffer"
          << std::endl;
      opencl_handle->CleanupKernel(color_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->SetKernelBufferData(color_kernel_name, kBufferInputName,
                                          this->data_.size(),
                                          this->data_.data())) {
    std::cout
        << "ERROR ToColorDitheredWithBlueNoise: Failed to init input buffer"
        << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  if (!opencl_handle->HasBuffer(color_kernel_name, kBufferOutputName)) {
    if (!opencl_handle->CreateKernelBuffer(color_kernel_name, CL_MEM_WRITE_ONLY,
                                           this->data_.size(), nullptr,
                                           kBufferOutputName)) {
      std::cout
          << "ERROR ToColorDitheredWithBlueNoise: Failed to set output buffer"
          << std::endl;
      opencl_handle->CleanupKernel(color_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->HasBuffer(color_kernel_name, kBufferBlueNoiseName)) {
    if (!opencl_handle->CreateKernelBuffer(color_kernel_name, CL_MEM_READ_ONLY,
                                           blue_noise->data_.size(), nullptr,
                                           kBufferBlueNoiseName)) {
      std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to alloc "
                   "blue-noise buffer"
                << std::endl;
      opencl_handle->CleanupKernel(color_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->SetKernelBufferData(
          color_kernel_name, kBufferBlueNoiseName, blue_noise->data_.size(),
          blue_noise->data_.data())) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to init "
                 "blue-noise buffer"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  if (!opencl_handle->HasBuffer(color_kernel_name,
                                kBufferBlueNoiseOffsetsName)) {
    if (!is_preserving_blue_noise_offsets_) {
      GenerateBlueNoiseOffsets();
    }

    if (!opencl_handle->CreateKernelBuffer(
            color_kernel_name, CL_MEM_READ_ONLY,
            sizeof(unsigned int) * blue_noise_offsets_.size(), nullptr,
            kBufferBlueNoiseOffsetsName)) {
      std::cout
          << "ERROR ToColorDitheredWithBlueNoise: Failed to alloc blue-noise "
             "offsets buffer"
          << std::endl;
      opencl_handle->CleanupKernel(color_kernel_name);
      return {};
    }
  }

  if (!opencl_handle->SetKernelBufferData(
          color_kernel_name, kBufferBlueNoiseOffsetsName,
          blue_noise_offsets_.size() * sizeof(unsigned int),
          blue_noise_offsets_.data())) {
    std::cout
        << "ERROR ToColorDitheredWithBlueNoise: Failed to init blue-noise "
           "offsets buffer"
        << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  // assign buffers/data to kernel parameters
  if (!opencl_handle->AssignKernelBuffer(color_kernel_name, 0,
                                         kBufferInputName)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 0"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  if (!opencl_handle->AssignKernelBuffer(color_kernel_name, 1,
                                         kBufferBlueNoiseName)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 1"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  if (!opencl_handle->AssignKernelBuffer(color_kernel_name, 2,
                                         kBufferOutputName)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 2"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  unsigned int input_width = this->GetWidth();
  if (!opencl_handle->AssignKernelArgument(
          color_kernel_name, 3, sizeof(unsigned int), &input_width)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 3"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  unsigned int input_height = this->GetHeight();
  if (!opencl_handle->AssignKernelArgument(
          color_kernel_name, 4, sizeof(unsigned int), &input_height)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 4"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  unsigned int blue_noise_width = blue_noise->GetWidth();
  if (!opencl_handle->AssignKernelArgument(
          color_kernel_name, 5, sizeof(unsigned int), &blue_noise_width)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 5"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  unsigned int blue_noise_height = blue_noise->GetHeight();
  if (!opencl_handle->AssignKernelArgument(
          color_kernel_name, 6, sizeof(unsigned int), &blue_noise_height)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 6"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }
  if (!opencl_handle->AssignKernelBuffer(color_kernel_name, 7,
                                         kBufferBlueNoiseOffsetsName)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to set parameter 7"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  auto work_group_size = opencl_handle->GetWorkGroupSize(color_kernel_name);
  // DEBUG
  // std::cout << "Got work_group_size == " << work_group_size << std::endl;

  std::size_t work_group_size_0 = std::sqrt(work_group_size);
  std::size_t work_group_size_1 = work_group_size_0;

  while (work_group_size_0 > 1 && input_width % work_group_size_0 != 0) {
    --work_group_size_0;
  }
  while (work_group_size_1 > 1 && input_height % work_group_size_1 != 0) {
    --work_group_size_1;
  }

  // DEBUG
  // std::cout << "Using WIDTHxHEIGHT: " << input_width << "x" << input_height
  //          << " with work_group_sizes: " << work_group_size_0 << "x"
  //          << work_group_size_1 << std::endl;

  if (!opencl_handle->ExecuteKernel2D(color_kernel_name, input_width,
                                      input_height, work_group_size_0,
                                      work_group_size_1, true)) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to execute Kernel"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  std::unique_ptr<Image> result_image =
      std::unique_ptr<Image>(new Image(*this));
  result_image->is_dithered_color_ = true;

  if (!opencl_handle->GetBufferData(color_kernel_name, kBufferOutputName,
                                    result_image->GetSize(),
                                    result_image->data_.data())) {
    std::cout << "ERROR ToColorDitheredWithBlueNoise: Failed to get output "
                 "buffer data"
              << std::endl;
    opencl_handle->CleanupKernel(color_kernel_name);
    return {};
  }

  return result_image;
}

const char *Image::GetGrayscaleDitheringKernel() {
  if (kOpenCLGrayscaleKernel == nullptr) {
    kOpenCLGrayscaleKernel =
        "unsigned int BN_INDEX(\n"
        "unsigned int x,\n"
        "unsigned int y,\n"
        "unsigned int o,\n"
        "unsigned int bn_width,\n"
        "unsigned int bn_height) {\n"
        "unsigned int offset_x = (o % bn_width + x) % bn_width;\n"
        "unsigned int offset_y = (o / bn_width + y) % bn_height;\n"
        "return offset_x + offset_y * bn_width;\n"
        "}\n"
        "\n"
        "__kernel void " IGPUP_PROJECT_GRAYSCALE_KERNEL_NAME_
        "(\n"
        "__global const unsigned char *input,\n"
        "__global const unsigned char *blue_noise,\n"
        "__global unsigned char *output,\n"
        "const unsigned int input_width,\n"
        "const unsigned int input_height,\n"
        "const unsigned int blue_noise_width,\n"
        "const unsigned int blue_noise_height,\n"
        "const unsigned int blue_noise_offset) {\n"
        "unsigned int idx = get_global_id(0);\n"
        "unsigned int idy = get_global_id(1);\n"
        "unsigned int b_i = BN_INDEX(idx, idy, blue_noise_offset,\n"
        "blue_noise_width, blue_noise_height);\n"
        "unsigned int input_index = idx + idy * input_width;\n"
        "output[input_index] = input[input_index] > blue_noise[b_i] ? 255 : "
        "0;\n"
        "}\n";
  }

  return kOpenCLGrayscaleKernel;
}

const char *Image::GetColorDitheringKernel() {
  if (kOpenCLColorKernel == nullptr) {
    kOpenCLColorKernel =
        "unsigned int BN_INDEX(\n"
        "unsigned int x,\n"
        "unsigned int y,\n"
        "unsigned int o,\n"
        "unsigned int bn_width,\n"
        "unsigned int bn_height) {\n"
        "unsigned int offset_x = (o % bn_width + x) % bn_width;\n"
        "unsigned int offset_y = (o / bn_width + y) % bn_height;\n"
        "return offset_x + offset_y * bn_width;\n"
        "}\n"
        "\n"
        "__kernel void " IGPUP_PROJECT_COLOR_KERNEL_NAME_
        "(\n"
        "__global const unsigned char *input,\n"
        "__global const unsigned char *blue_noise,\n"
        "__global unsigned char *output,\n"
        "const unsigned int input_width,\n"
        "const unsigned int input_height,\n"
        "const unsigned int blue_noise_width,\n"
        "const unsigned int blue_noise_height,\n"
        "__global const unsigned int *blue_noise_offsets) {\n"
        "unsigned int idx = get_global_id(0);\n"
        "unsigned int idy = get_global_id(1);\n"
        "  unsigned int b_i[3] = {\n"
        "    BN_INDEX(idx, idy, blue_noise_offsets[0], blue_noise_width,\n"
        "      blue_noise_height),\n"
        "    BN_INDEX(idx, idy, blue_noise_offsets[1], blue_noise_width,\n"
        "      blue_noise_height),\n"
        "    BN_INDEX(idx, idy, blue_noise_offsets[2], blue_noise_width,\n"
        "      blue_noise_height)\n"
        "  };\n"
        // input is 4 bytes per pixel, alpha channel is merely copied
        "  for (unsigned int i = 0; i < 4; ++i) {\n"
        "    unsigned int input_index = idx * 4 + idy * input_width * 4 + i;\n"
        "    if (i < 3) {\n"
        "      output[input_index] = input[input_index] > blue_noise[b_i[i]] ? "
        "      255 : 0;\n"
        "    } else {\n"
        "      output[input_index] = input[input_index];\n"
        "    }\n"
        "  }\n"
        "}\n";
  }

  return kOpenCLColorKernel;
}

OpenCLHandle::Ptr Image::GetOpenCLHandle() {
  if (!opencl_handle_) {
    opencl_handle_ = OpenCLContext::GetHandle();
  }

  return opencl_handle_;
}

void Image::DecodePNG(const std::string &filename) {
  FILE *file = std::fopen(filename.c_str(), "rb");
  if (!file) {
    std::cout << "ERROR: Failed to open \"" << filename << '"' << std::endl;
    return;
  }

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
  fclose(file);

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
  is_grayscale_ = true;

  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    std::cout << "ERROR: Failed to open file \"" << filename << '"'
              << std::endl;
    return;
  }

  std::string str_input;
  int int_input;
  ifs >> str_input;
  if (!ifs.good()) {
    std::cout << "ERROR: Failed to parse file (PGM first identifier) \""
              << filename << '"' << std::endl;
    return;
  }

  if (str_input.compare("P2") == 0) {
    // data stored in ascii format

    // get width
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM width) \"" << filename
                << '"' << std::endl;
      return;
    }
    width_ = int_input;

    // get height
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM height) \"" << filename
                << '"' << std::endl;
      return;
    }
    height_ = int_input;

    // get max_value
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM max) \"" << filename << '"'
                << std::endl;
      return;
    }
    float max_value = int_input;

    // parse data
    data_.clear();
    data_.reserve(width_ * height_);
    float value;
    for (unsigned int i = 0; i < width_ * height_; ++i) {
      ifs >> int_input;
      if (!ifs.good()) {
        std::cout << "ERROR: Failed to parse file (PGM data) \"" << filename
                  << '"' << std::endl;
        return;
      }
      value = static_cast<float>(int_input) / max_value;
      data_.push_back(std::round(value * 255.0F));
    }
  } else if (str_input.compare("P5") == 0) {
    // data stored in raw format

    // get width
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM width) \"" << filename
                << '"' << std::endl;
      return;
    }
    width_ = int_input;

    // get height
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM height) \"" << filename
                << '"' << std::endl;
      return;
    }
    height_ = int_input;

    // get max_value
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PGM max) \"" << filename << '"'
                << std::endl;
      return;
    }
    int max_value_int = int_input;
    float max_value = int_input;

    // validate max_value
    if (max_value_int != 255 && max_value_int != 65535) {
      std::cout << "ERROR: Invalid max value for PGM (should be 255 or 65535) "
                   "(filename \""
                << filename << "\")" << std::endl;
      return;
    }

    // extract whitespace before data
    {
      int c = ifs.get();
      if (c != '\n' && c != ' ') {
        std::cout << "WARNING: File data after PGM max is not whitespace "
                     "(filename \""
                  << filename << "\")"
                  << " value is " << c << std::endl;
      }

      if (!ifs.good()) {
        std::cout << "ERROR: Failed to parse file (PGM after whitespace) \""
                  << filename << '"' << std::endl;
        return;
      }
    }

    // parse raw data
    data_.clear();
    data_.reserve(width_ * height_);
    float value;
    for (unsigned int i = 0; i < width_ * height_; ++i) {
      if (max_value_int == 255) {
        value = ifs.get() / max_value;
        data_.push_back(std::round(value * 255.0F));
        if (!ifs.good()) {
          std::cout << "ERROR: Failed to parse file (PGM data) \"" << filename
                    << '"' << std::endl;
          return;
        }
      } else /* if (max_value_int == 65535) */ {
        value = (ifs.get() & 0xFF) | ((ifs.get() << 8) & 0xFF00);
        value /= max_value;
        data_.push_back(std::round(value * 255.0F));
        if (!ifs.good()) {
          std::cout << "ERROR: Failed to parse file (PGM data 16-bit) \""
                    << filename << '"' << std::endl;
          return;
        }
      }
    }

    if (ifs.get() != decltype(ifs)::traits_type::eof()) {
      std::cout << "WARNING: Trailing data in PGM file \"" << filename << '"'
                << std::endl;
    }
  } else {
    std::cout << "ERROR: Invalid \"magic number\" in header of file \""
              << filename << '"' << std::endl;
  }
}

void Image::DecodePPM(const std::string &filename) {
  is_grayscale_ = false;
  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    std::cout << "ERROR: Failed to open file \"" << filename << '"'
              << std::endl;
    return;
  }

  std::string str_input;
  int int_input;
  ifs >> str_input;
  if (!ifs.good()) {
    std::cout << "ERROR: Failed to parse file (PPM first identifier) \""
              << filename << '"' << std::endl;
    return;
  }

  if (str_input.compare("P3") == 0) {
    // data stored in ascii format

    // get width
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM width) \"" << filename
                << '"' << std::endl;
      return;
    }
    width_ = int_input;

    // get height
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM height) \"" << filename
                << '"' << std::endl;
      return;
    }
    height_ = int_input;

    // get max_value
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM max) \"" << filename << '"'
                << std::endl;
      return;
    }
    float max_value = int_input;

    // parse data
    data_.clear();
    data_.reserve(width_ * height_ * 4);
    float value;
    for (unsigned int i = 0; i < width_ * height_ * 3; ++i) {
      ifs >> int_input;
      if (!ifs.good()) {
        std::cout << "ERROR: Failed to parse file (PPM data) \"" << filename
                  << '"' << std::endl;
        return;
      }
      value = static_cast<float>(int_input) / max_value;
      data_.push_back(std::round(value * 255.0F));
      if (i % 3 == 2) {
        // PPM is RGB but Image stores as RGBA
        data_.push_back(255);
      }
    }
  } else if (str_input.compare("P6") == 0) {
    // data stored in raw format

    // get width
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM width) \"" << filename
                << '"' << std::endl;
      return;
    }
    width_ = int_input;

    // get height
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM height) \"" << filename
                << '"' << std::endl;
      return;
    }
    height_ = int_input;

    // get max_value
    ifs >> int_input;
    if (!ifs.good() || int_input <= 0) {
      std::cout << "ERROR: Failed to parse file (PPM max) \"" << filename << '"'
                << std::endl;
      return;
    }
    int max_value_int = int_input;
    float max_value = int_input;

    // validate max_value
    if (max_value_int != 255 && max_value_int != 65535) {
      std::cout << "ERROR: Invalid max value for PPM (should be 255 or 65535) "
                   "(filename \""
                << filename << "\")" << std::endl;
      return;
    }

    // extract whitespace before data
    {
      int c = ifs.get();
      if (c != '\n' && c != ' ') {
        std::cout
            << "WARNING: File data after PPM max is not whitespace (filename \""
            << filename << "\") value is " << c << std::endl;
      }

      if (!ifs.good()) {
        std::cout << "ERROR: Failed to parse file (PPM after whitespace) \""
                  << filename << '"' << std::endl;
        return;
      }
    }

    // parse raw data
    data_.clear();
    data_.reserve(width_ * height_ * 4);
    float value;
    for (unsigned int i = 0; i < width_ * height_ * 3; ++i) {
      if (max_value_int == 255) {
        value = ifs.get() / max_value;
        data_.push_back(std::round(value * 255.0F));
        if (!ifs.good()) {
          std::cout << "ERROR: Failed to parse file (PPM data) \"" << filename
                    << '"' << std::endl;
          return;
        }
      } else /* if (max_value_int == 65535) */ {
        value = (ifs.get() & 0xFF) | ((ifs.get() << 8) & 0xFF00);
        value /= max_value;
        data_.push_back(std::round(value * 255.0F));
        if (!ifs.good()) {
          std::cout << "ERROR: Failed to parse file (PPM data 16-bit) \""
                    << filename << '"' << std::endl;
          return;
        }
      }

      if (i % 3 == 2) {
        // PPM is RGB but Image stores as RGBA
        data_.push_back(255);
      }
    }

    if (ifs.get() != decltype(ifs)::traits_type::eof()) {
      std::cout << "WARNING: Trailing data in PPM file \"" << filename << '"'
                << std::endl;
    }
  } else {
    std::cout << "ERROR: Invalid \"magic number\" in header of file \""
              << filename << '"' << std::endl;
  }
}

const std::string &Image::GetGrayscaleKernelName() {
  if (!GetOpenCLHandle()) {
    return kEmptyString;
  } else if (!opencl_handle_->HasKernel(kGrayscaleKernelName)) {
    if (!opencl_handle_->CreateKernelFromSource(GetGrayscaleDitheringKernel(),
                                                kGrayscaleKernelName)) {
      std::cout << "ERROR: Failed to create " << kGrayscaleKernelName
                << " OpenCL Kernel" << std::endl;
      return kEmptyString;
    }
  }

  return kGrayscaleKernelName;
}

const std::string &Image::GetColorKernelName() {
  if (!GetOpenCLHandle()) {
    return kEmptyString;
  } else if (!opencl_handle_->HasKernel(kColorKernelName)) {
    if (!opencl_handle_->CreateKernelFromSource(GetColorDitheringKernel(),
                                                kColorKernelName)) {
      std::cout << "ERROR: Failed to create " << kColorKernelName
                << " OpenCL Kernel" << std::endl;
      return kEmptyString;
    }
  }

  return kColorKernelName;
}

void Image::GenerateBlueNoiseOffsets() {
  std::srand(std::time(nullptr));
  while (DuplicateBlueNoiseOffsetExists()) {
    for (unsigned int i = 0; i < blue_noise_offsets_.size(); ++i) {
      blue_noise_offsets_.at(i) = rand() % kBlueNoiseOffsetMax;
    }
  }
}

bool Image::DuplicateBlueNoiseOffsetExists() const {
  for (unsigned int i = 1; i < blue_noise_offsets_.size(); ++i) {
    if (blue_noise_offsets_.at(i - 1) == blue_noise_offsets_.at(i)) {
      return true;
    }
  }
  if (blue_noise_offsets_.size() > 1 &&
      blue_noise_offsets_.at(0) ==
          blue_noise_offsets_.at(blue_noise_offsets_.size() - 1)) {
    return true;
  }

  return false;
}

bool Image::VerifyOpenCLBuffers(const std::string &kernel_name,
                                const std::vector<std::string> &buffer_names,
                                const Image *input_image,
                                const Image *blue_noise_image) const {
  std::size_t size;
  for (auto &buffer_name : buffer_names) {
    size = opencl_handle_->GetBufferSize(kernel_name, buffer_name);
    if (size == 0) {
      return false;
    }
    if (buffer_name == kBufferInputName || buffer_name == kBufferOutputName) {
      if (input_image->is_grayscale_) {
        if (size != input_image->width_ * input_image->height_) {
          return false;
        }
      } else {
        if (size != input_image->width_ * input_image->height_ * 4) {
          return false;
        }
      }
    } else if (buffer_name == kBufferBlueNoiseName) {
      if (size != blue_noise_image->width_ * blue_noise_image->height_) {
        return false;
      }
    }
  }

  return true;
}
