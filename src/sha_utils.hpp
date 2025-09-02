#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
std::string hex(const uint8_t* p, size_t n);
void sha256(const uint8_t* data, size_t len, uint8_t out[32]);
