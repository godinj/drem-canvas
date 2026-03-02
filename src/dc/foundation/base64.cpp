#include "dc/foundation/base64.h"

namespace dc {

static constexpr char kEncodeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static constexpr uint8_t kDecodeTable[] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,65,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
};

std::string base64Encode(const uint8_t* data, size_t size)
{
    std::string result;
    result.reserve(((size + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < size)
    {
        uint32_t triple = (uint32_t(data[i]) << 16)
                        | (uint32_t(data[i + 1]) << 8)
                        |  uint32_t(data[i + 2]);
        result += kEncodeTable[(triple >> 18) & 0x3f];
        result += kEncodeTable[(triple >> 12) & 0x3f];
        result += kEncodeTable[(triple >> 6)  & 0x3f];
        result += kEncodeTable[triple & 0x3f];
        i += 3;
    }

    if (i + 1 == size)
    {
        uint32_t val = uint32_t(data[i]) << 16;
        result += kEncodeTable[(val >> 18) & 0x3f];
        result += kEncodeTable[(val >> 12) & 0x3f];
        result += '=';
        result += '=';
    }
    else if (i + 2 == size)
    {
        uint32_t val = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        result += kEncodeTable[(val >> 18) & 0x3f];
        result += kEncodeTable[(val >> 12) & 0x3f];
        result += kEncodeTable[(val >> 6)  & 0x3f];
        result += '=';
    }

    return result;
}

std::string base64Encode(const std::vector<uint8_t>& data)
{
    return base64Encode(data.data(), data.size());
}

std::vector<uint8_t> base64Decode(std::string_view encoded)
{
    std::vector<uint8_t> result;
    result.reserve((encoded.size() / 4) * 3);

    uint32_t accum = 0;
    int bits = 0;

    for (char c : encoded)
    {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ')
            continue;

        auto idx = static_cast<unsigned char>(c);
        if (idx >= sizeof(kDecodeTable))
            continue;

        uint8_t val = kDecodeTable[idx];
        if (val >= 64)
            continue;

        accum = (accum << 6) | val;
        bits += 6;

        if (bits >= 8)
        {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((accum >> bits) & 0xff));
        }
    }

    return result;
}

} // namespace dc
