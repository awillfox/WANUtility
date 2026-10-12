#include "Ucs2.h"

namespace wan::core
{
    std::wstring DecodeUtf16BE(std::span<const uint8_t> bytes)
    {
        std::wstring out;
        out.reserve(bytes.size() / 2);
        for (size_t i = 0; i + 1 < bytes.size(); i += 2)
        {
            out.push_back(static_cast<wchar_t>((bytes[i] << 8) | bytes[i + 1]));
        }
        return out;
    }

    bool IsUcs2Dcs(uint32_t dcs)
    {
        // TS 23.038 (CBS/USSD): 0001 0001 = UCS-2 preceded by language.
        if (dcs == 0x11)
        {
            return true;
        }
        // 01xx xxxx (general data coding) and 1001 xxxx: bits 3..2 select the alphabet, 10 = UCS-2.
        if ((dcs & 0xC0) == 0x40 || (dcs & 0xF0) == 0x90)
        {
            return ((dcs >> 2) & 0x3) == 0x2;
        }
        return false;
    }
}
