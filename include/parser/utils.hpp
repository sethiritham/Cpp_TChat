#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <cmath>


int clamp_value_255(int value);

void write_little_endian_16(std::ofstream& file, uint16_t value);

void write_little_endian_32(std::ofstream& file, uint32_t value);

void write_big_endian_32(std::ofstream& file, uint32_t value);
/**
 * @brief Converts BIG ENDIAN files to LITTLE ENDIAN (CPU expects Little Endian ordering)
 */

uint32_t readBigEndian32(std::ifstream& file);

uint32_t convert_to_little_endian_32(uint32_t v);

uint32_t convert_to_big_endian_32(uint32_t v);

#endif