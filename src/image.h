#ifndef IGPUP_DITHERING_PROJECT_IMAGE_H_
#define IGPUP_DITHERING_PROJECT_IMAGE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <png.h>

#include "opencl_handle.h"

class Image {
 public:
  Image();

  /*!
   * \brief Decodes the given file's data and stores the pixels internally.
   *
   * Use IsValid() to check if the file was successfully decoded.
   *
   * Image supports decoding .png, .pgm, and .ppm . Decoding only checks the
   * filename suffix as a guide on which file-type to expect.
   */
  explicit Image(const char *filename);

  /// Same constructor as Image(const char *filename)
  explicit Image(const std::string &filename);

  // allow copy
  Image(const Image &other) = default;
  Image &operator=(const Image &other) = default;

  // allow move
  Image(Image &&other) = default;
  Image &operator=(Image &&other) = default;

  /*!
   * \brief Is true if the Image instance is valid.
   *
   * This will return false on an Image instance that failed to decode an input
   * image when constructed.
   */
  bool IsValid() const;

  /// Returns a raw pointer to the image's data.
  uint8_t *GetData();
  /// Returns a const raw pointer to the image's data.
  const uint8_t *GetData() const;

  /*!
   * \brief Returns the number of bytes in the image.
   *
   * For grayscale images, each pixel is one byte.
   * For colored images, each pixel is 4 bytes: R, G, B, and Alpha.
   */
  unsigned int GetSize() const;
  /// Returns the width of the image.
  unsigned int GetWidth() const;
  /// Returns the height of the image.
  unsigned int GetHeight() const;

  /// Returns true if the image is grayscale. If false, then the image is RGBA.
  bool IsGrayscale() const;

  /*!
   * \brief Saves the current image data as a PNG file.
   *
   * Returns false if the filename already exists and overwrite is false, or if
   * saving failed.
   */
  bool SaveAsPNG(const std::string &filename, bool overwrite);
  /// Same as SaveAsPNG()
  bool SaveAsPNG(const char *filename, bool overwrite);

  /*!
   * \brief Saves the current image data as a PPM file.
   *
   * Returns false if the filename already exists and overwrite is false, or if
   * saving failed.
   *
   * If packed is true, then the data is stored in binary format, otherwise the
   * data is stored as ascii format.
   */
  bool SaveAsPPM(const std::string &filename, bool overwrite,
                 bool packed = true);
  /// Same as SaveAsPPM()
  bool SaveAsPPM(const char *filename, bool overwrite, bool packed = true);

  /// Converts rgb to gray with luminance-preserving algorithm
  static uint8_t ColorToGray(uint8_t red, uint8_t green, uint8_t blue);

  /*!
   * \brief Returns a grayscale version of the Image.
   *
   * Using std::optional would be ideal here, but this program is aiming for
   * compatibility up to C++11, and std::optional was made available in C++17.
   *
   * \return A std::unique_ptr holding an Image on success, empty otherwise.
   */
  std::unique_ptr<Image> ToGrayscale() const;

  /*!
   * \brief Returns a grayscaled and dithered version of the current Image.
   *
   * \return A std::unique_ptr holding an Image on success, empty otherwise.
   */
  std::unique_ptr<Image> ToGrayscaleDitheredWithBlueNoise(Image *blue_noise);

  /*!
   * \brief Returns a colored dithered version of the current Image.
   *
   * Unlike the grayscaled version, this dithers the red, green, and blue
   * channels. There may be mixed pixels as a result, such as yellow, cyan,
   * magenta, or white.
   *
   * \return A std::unique_ptr holding an Image on success, empty otherwise.
   */
  std::unique_ptr<Image> ToColorDitheredWithBlueNoise(Image *blue_noise);

  /// Returns the grayscale Dithering Kernel function as a C string
  static const char *GetGrayscaleDitheringKernel();

  /// Returns the color Dithering Kernel function as a C string
  static const char *GetColorDitheringKernel();

  /// Returns the OpenCLHandle::Ptr instance
  OpenCLHandle::Ptr GetOpenCLHandle();

 private:
  friend class Video;

  static const char *opencl_grayscale_kernel_;
  static const char *opencl_color_kernel_;
  static const std::array<png_color, 2> dither_bw_palette_;
  static const std::array<png_color, 8> dither_color_palette_;
  OpenCLHandle::Ptr opencl_handle_;
  /// Internally holds rgba or grayscale (1 channel)
  std::vector<uint8_t> data_;
  unsigned int width_;
  unsigned int height_;
  bool is_grayscale_;
  bool is_dithered_grayscale_;
  bool is_dithered_color_;

  void DecodePNG(const std::string &filename);
  void DecodePGM(const std::string &filename);
  void DecodePPM(const std::string &filename);
};

#endif
