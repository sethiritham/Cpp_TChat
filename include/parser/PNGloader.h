#ifndef PNGLOADER_H
#define PNGLOADER_H

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <zlib.h>
#include <iostream>
#include <cmath>

#include "utils.hpp"

struct Image
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> data;
    std::vector<uint8_t> editedPixels;
    std::vector<uint8_t> zlibStream;
    std::vector<uint8_t> pixels;
};

class PNGloader
{
public:
    PNGloader();

    bool Load(const char* filename, Image& image);

    ~PNGloader();

private:
    bool validateSignature(std::ifstream& file);
    
    bool processChunks(std::ifstream& file, Image& image);
    void reconstructImage(Image& image);
};

#endif // PNGLOADER_H