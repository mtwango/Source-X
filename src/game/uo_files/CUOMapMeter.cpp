#include "CUOMapMeter.h"

bool CUOMapMeter::IsTerrainNull(const word wTerrainIndex )
{
    switch ( wTerrainIndex )
    {
        case 0x244: //580
            return true;
    }
    return false;
}
