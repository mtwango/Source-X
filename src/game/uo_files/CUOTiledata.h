/**
* @file CUOTiledata.h
*
*/

#ifndef _INC_CUOTILEDATA_H
#define _INC_CUOTILEDATA_H

#include "CUOItemTypeRec.h"
#include "CUOTerrainTypeRec.h"
#include "uofiles_enums_itemid.h"
#include "uofiles_types.h"
#include <vector>

class CUOTiledata
{
    std::vector<CUOItemTypeRec_HS> _tiledataItemEntries;
    std::vector<CUOTerrainTypeRec_HS> _tiledataTerrainEntries;

public:
    void Load();
    uint GetItemMaxIndex() const {
        ASSERT(!_tiledataItemEntries.empty());
        return static_cast<uint>(_tiledataItemEntries.size() - 1);
    }
    const CUOItemTypeRec_HS* GetItemEntry(const ITEMID_TYPE id) const {
        return &(_tiledataItemEntries[id]);
    }
    const CUOTerrainTypeRec_HS* GetTerrainEntry(const TERRAIN_TYPE id) const {
        return &(_tiledataTerrainEntries[id]);
    }
};


#endif // _INC_CUOTILEDATA_H
