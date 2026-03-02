#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dc {

std::string base64Encode(const std::vector<uint8_t>& data);
std::string base64Encode(const uint8_t* data, size_t size);
std::vector<uint8_t> base64Decode(std::string_view encoded);

} // namespace dc
