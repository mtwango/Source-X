#include "CUOHuesRec.h"

byte CUOHuesRec::GetRGB(const int rgb ) const
{
    const short sColor = m_color[31];
    if ( rgb == 0 ) // R
        return static_cast<byte>((sColor & 0x7C00) >> 7);
    if (rgb == 1)
        return static_cast<byte>((sColor & 0x3E0) >> 2);
    if (rgb == 3)
        return static_cast<byte>((sColor & 0x1F) << 3);

    return 0;
}
