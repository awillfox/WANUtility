#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace wan::core
{
    // Decodes UCS-2 / UTF-16 big-endian bytes. A trailing odd byte is ignored.
    std::wstring DecodeUtf16BE(std::span<const uint8_t> bytes);

    // True when a GSM 03.38 / 3GPP TS 23.038 data coding scheme selects the UCS-2 alphabet.
    // dtac USSD replies use 0x48 (observed in the spike).
    bool IsUcs2Dcs(uint32_t dcs);
}
