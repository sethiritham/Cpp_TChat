#include "PNGloader.h"

static const uint8_t PNG_SIGNATURE[8] = {
    0x89, 0x50, 0x4E, 0x47,
    0x0D, 0x0A, 0x1A, 0x0A}; // FIxed signature at top of every PNG file

/**
 * @brief Calls all of the PNGLoader functions to procesess pixels from PNG.
 */
bool PNGloader::Load(const char *filename, Image &image) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return false;
  }

  if (!validateSignature(file)) {
    std::cerr << "Invalid PNG signature: " << filename << std::endl;
    return false;
  }

  std::cout << "PNG signature is valid." << std::endl;
  processChunks(file, image);

  reconstructImage(image);

  return true;
}

/**
 * @brief checks for valid PNG signature
 */
bool PNGloader::validateSignature(std::ifstream &file) {
  uint8_t buffer[8];
  file.read(reinterpret_cast<char *>(buffer), 8);

  if (file.gcount() != 8) {
    std::cerr << "Failed to read PNG signature. (FILE SIZE TOO SMALL)"
              << std::endl;
    return false;
  }

  for (int i = 0; i < 8; i++) {
    if (buffer[i] != PNG_SIGNATURE[i]) {
      std::cerr << "Invalid PNG signature." << std::endl;
      return false;
    }
  }

  return true;
}

/**
 * @brief Determines the closest neighbour to current pixel
 */
static uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
  int p = a + b - c;
  int pa = std::abs(p - a);
  int pb = std::abs(p - b);
  int pc = std::abs(p - c);
  if (pa <= pb && pa <= pc)
    return a;
  if (pb <= pa && pb <= pc)
    return b;
  return c;
}

bool PNGloader::processChunks(std::ifstream &file, Image &image) {
  while (file.good()) {
    uint32_t length = readBigEndian32(file);

    char type[5] = {0};
    file.read(type, 4);

    std::cout << "Detected chunk: " << type << "Length: " << length
              << std::endl;

    if (std::string(type) == "IHDR") {
      image.width = readBigEndian32(file);
      image.height = readBigEndian32(file);

      uint8_t bitDepth = file.get();

      if (bitDepth != 8) // Parser only works on 8bit images for now
      {
        std::cerr << "Only 8-bit PNG supported\n";
        return false;
      }

      std::cout << "BIT DEPTH: " << bitDepth << std::endl;
      uint8_t colorType = file.get();

      file.ignore(3);

      if (colorType == 2)
        image.channels = 3;
      else if (colorType == 6)
        image.channels = 4;
      else {
        std::cerr << "Unsupported color type: " << colorType << std::endl;
        return false;
      }

      std::cout << " -> Dimensions: " << image.width << "x" << image.height
                << std::endl;
      std::cout << " -> Channels: " << image.channels << std::endl;

      readBigEndian32(file);

    }

    else if (std::string(type) == "IDAT") {
      std::cout << "Processing IDAT chunk." << std::endl;

      std::vector<uint8_t> data(length);
      file.read(reinterpret_cast<char *>(data.data()), length);

      image.zlibStream.insert(image.zlibStream.end(), data.begin(), data.end());

      readBigEndian32(file);

    }

    else if (std::string(type) == "IEND") // ENDING CHUNK
    {
      std::cout << "End of PNG file." << std::endl;
      break;
    } else {
      file.seekg(length + 4, std::ios::cur);
    }
  }

  if (image.zlibStream.empty()) {
    std::cerr << "Error: No IDAT chunks found!" << std::endl;
    return false;
  }

  std::cout << "Total Compressed Data Size: " << image.zlibStream.size()
            << " bytes" << std::endl;

  // DECOMPRESSION - ZLIB FOR NOW AUTONOMOUS LATER

  unsigned long bytesPerScanline = 1 + (image.width * image.channels);
  unsigned long expectedStreamSize = image.height * bytesPerScanline;

  image.data.resize(expectedStreamSize);

  unsigned long destLen = expectedStreamSize;
  int result = uncompress(image.data.data(), &destLen, image.zlibStream.data(),
                          image.zlibStream.size());

  if (result != Z_OK) {
    std::cerr << "Failed to decompress data." << std::endl;
    return false;
  }

  std::cout << "Decompression successful." << std::endl;
  std::cout << "Raw filtered data size: " << image.data.size() << " bytes"
            << std::endl;

  return true;
}

/**
 * @brief unfilters the PNG after DEFLATE decompression
 */
void PNGloader::reconstructImage(Image &image) {
  image.pixels.resize(image.width * image.height * image.channels);

  int stride = image.width * image.channels;

  unsigned long rawIndex = 0;

  for (uint32_t y = 0; y < image.height; y++) {
    uint8_t filterType = image.data[rawIndex++];

    for (int c = 0; c < stride; c++) {
      int pixelIndex = y * stride + c;
      uint8_t raw = image.data[rawIndex++];

      uint8_t left = (c >= (int)image.channels)
                         ? image.pixels[pixelIndex - image.channels]
                         : 0;
      uint8_t up = (y > 0) ? image.pixels[pixelIndex - stride] : 0;
      uint8_t upLeft = (y > 0 && c >= (int)image.channels)
                           ? image.pixels[pixelIndex - stride - image.channels]
                           : 0;

      uint8_t val = 0;
      switch (filterType) {
      case 0:
        val = raw;
        break;
      case 1:
        val = (raw + left) & 0xFF;
        break; //(200 + 100) % 256 (PNG explicitly requires modulo 256
               //arithmetic for filters.)
      case 2:
        val = (raw + up) & 0xFF;
        break;
      case 3:
        val = (raw + ((left + up) / 2)) & 0xFF;
        break;
      case 4:
        val = (raw + paethPredictor(left, up, upLeft)) & 0xFF;
        break;
      default:
        val = raw;
        break;
      }
      image.pixels[pixelIndex] = val;
    }
  }

  std::cout << "DEBUG: Reconstruction complete. Pixels: " << image.pixels.size()
            << std::endl;
}

PNGloader::PNGloader() {}
PNGloader::~PNGloader() {}
