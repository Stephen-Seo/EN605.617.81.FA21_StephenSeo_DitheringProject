#ifndef IGPUP_DITHERING_PROJECT_IMAGE_H_
#define IGPUP_DITHERING_PROJECT_IMAGE_H_

#include <cstdint>
#include <string>
#include <vector>

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
  Image(const char *filename);

  /// Same constructor as Image(const char *filename)
  Image(const std::string &filename);

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

 private:
  std::vector<uint8_t> data_;
  unsigned int width_;
  unsigned int height_;
  bool is_grayscale_;

  void DecodePNG(const std::string &filename);
  void DecodePGM(const std::string &filename);
  void DecodePPM(const std::string &filename);
};

#endif
