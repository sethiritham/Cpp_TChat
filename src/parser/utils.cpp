#include "utils.hpp"

int clamp_value_255(int value)
{   
    return (value > 255 ? 255 : 
                value < 0 ? 0 : 
                        value);
}

void write_little_endian_16(std::ofstream& file, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (value >> 0) & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    file.write(reinterpret_cast<char*>(bytes), 2);
}

void write_little_endian_32(std::ofstream& file, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (value >> 0) & 0xFF; // Least significant byte first (LITTLE ENDIAN)
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    file.write(reinterpret_cast<char*>(bytes), 4);
}

uint32_t readBigEndian32(std::ifstream& file)
{
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

void write_big_endian_32(std::ofstream& file, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF; // Least significant byte LAST (BIG ENDIAN)
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = (value >> 0) & 0xFF;
    file.write(reinterpret_cast<char*>(bytes), 4);    
}

/**
 * @brief Cannonical swap
 */
uint32_t convert_to_big_endian_32(uint32_t v)
{
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
}

uint32_t convert_to_little_endian_32(uint32_t v)
{
    return ((v & 0x000000FF) >> 24) |
           ((v & 0x0000FF00) >> 8)  |
           ((v & 0x00FF0000) << 8)  |
           ((v & 0xFF000000) << 24);
}

