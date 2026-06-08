#include "../../common/CLog.h"
#include "../items/CItemMulti.h"
#include "../uo_files/CUOMultiItemRec.h"
#include "../uo_files/CUOStaticItemRec.h"
#include "../uo_files/CUOTerrainInfo.h"
#include "../CWorldMap.h"
#include "../CWorldSearch.h"
#include "CChar.h"
#include <cmath>


bool CChar::CanSeeLOS( const CPointMap &ptDst, CPointMap *pptBlock, const int iMaxDist, const word wFlags, const bool fCombatCheck ) const
{
	ADDTOCALLSTACK("CChar::CanSeeLOS");
	// WARNING: CanSeeLOS is an expensive function (lot of calculations but	most importantly it has to read the UO files, and file I/O is slow).

	if ( (m_pPlayer && (g_Cfg.m_iAdvancedLos & ADVANCEDLOS_PLAYER)) || (m_pNPC && (g_Cfg.m_iAdvancedLos & ADVANCEDLOS_NPC)) )
		return CanSeeLOS_New(ptDst, pptBlock, iMaxDist, wFlags);

	// Max distance of iMaxDist
	// Line of sight check
	// NOTE: if not blocked. pptBlock is undefined.
	// 3D LOS later - real LOS, i.e. we can't shoot through the floor, but can shoot through the hole in it

	if ( !fCombatCheck && IsPriv(PRIV_GM) )	// If i'm checking the LOS during a combat, i don't want to shoot through the walls even if i'm a GM
		return true;

	CPointMap ptSrc(GetTopPoint());
	int iDist = ptSrc.GetDist(ptDst);
	if ( iDist > iMaxDist )
	{
	blocked:
		if ( pptBlock )
			*pptBlock = ptSrc;
		return false;	// blocked
	}

	// Walk towards the object. If any spot is too far above our heads then we can not see what's behind it.
	int iDistTry = 0;
	while ( --iDist >= 0 )
	{
		const DIR_TYPE dir = ptSrc.GetDir(ptDst);
		uint64 uiBlockFlags;
		if ( dir % 2 && !IsSetEF(EF_NoDiagonalCheckLOS) )	// test only diagonal dirs
		{
			CPointMap ptTest(ptSrc);
            const auto dirTest1 = static_cast<DIR_TYPE>(dir - 1);	// get 1st ortogonal
            auto dirTest2 = static_cast<DIR_TYPE>(dir + 1);	// get 2nd ortogonal
			if ( dirTest2 == DIR_QTY )		// roll over
				dirTest2 = DIR_N;

			ptTest.Move(dirTest1);
			uiBlockFlags = CAN_C_SWIM|CAN_C_WALK|CAN_C_FLY;
			char z = CWorldMap::GetHeightPoint2(ptTest, uiBlockFlags, true);
			short zDiff = static_cast<short>(abs(z - ptTest.m_z));

			if ( (zDiff > PLAYER_HEIGHT) || (uiBlockFlags & (CAN_I_BLOCK|CAN_I_DOOR)) )		// blocked
			{
				ptTest = ptSrc;
				ptTest.Move(dirTest2);
				{
					uiBlockFlags = CAN_C_SWIM|CAN_C_WALK|CAN_C_FLY;
					z = CWorldMap::GetHeightPoint2(ptTest, uiBlockFlags, true);
					zDiff = static_cast<short>(abs(z - ptTest.m_z));
					if ( zDiff > PLAYER_HEIGHT )
						goto blocked;

					if (uiBlockFlags & (CAN_I_BLOCK|CAN_I_DOOR))
					{
						ptSrc = ptTest;
						goto blocked;
					}
				}
			}
			ptTest.m_z = z;
		}

		if ( iDist )
		{
			ptSrc.Move(dir);	// NOTE: The dir is very coarse and can change slightly.
			uiBlockFlags = CAN_C_SWIM|CAN_C_WALK|CAN_C_FLY;
            const char z = CWorldMap::GetHeightPoint2(ptSrc, uiBlockFlags, true);

            if (const short zDiff = static_cast<short>(abs(z - ptSrc.m_z)); (zDiff > PLAYER_HEIGHT) || (uiBlockFlags & (CAN_I_BLOCK|CAN_I_DOOR)) || (iDistTry > iMaxDist) )
				goto blocked;

			ptSrc.m_z = z;
			++iDistTry;
		}
	}

	if ( abs(static_cast<int>(ptSrc.m_z) - static_cast<int>(ptDst.m_z)) >= 20 )
		return false;
	return true;	// made it all the way to the object with no obstructions.
}

// a - gradient < x < b + gradient
#define BETWEENPOINT(coord, coordt, coords) ( (coord > ((float)minimum(coordt, coords) - 0.5)) && (coord < ((float)maximum(coordt, coords) + 0.5)) )
#define APPROX(num) ( (float)((num - floor(num)) > 0.5) ? ceil(num) : floor(num) )
//#define CALCITEMHEIGHT(num) num + ((pItemDef->GetTFlags() & 0x400)? pItemDef->GetHeight() / 2 : pItemDef->GetHeight())
#define WARNLOS(_x_)		if ( g_Cfg.m_iDebugFlags & DEBUGF_LOS ) { g_Log.EventWarn _x_; }

bool CChar::CanSeeLOS_New_Failed( CPointMap *pptBlock, const CPointMap &ptNow ) const
{
	ADDTOCALLSTACK("CChar::CanSeeLOS_New_Failed");
	if ( pptBlock )
		*pptBlock = ptNow;
	return false;
}

bool CChar::CanSeeLOS_New( const CPointMap &ptDst, CPointMap *pptBlock, int iMaxDist, word flags, bool bCombatCheck ) const
{
	ADDTOCALLSTACK("CChar::CanSeeLOS_New");
	// WARNING: CanSeeLOS is an expensive function (lot of calculations but	most importantly it has to read the UO files, and file I/O is slow).

	if ( !bCombatCheck && IsPriv(PRIV_GM) )	// If i'm checking the LOS during a combat, i don't want to shoot through the walls even if i'm a GM
	{
		WARNLOS(("GM Pass\n"));
		return true;
	}

	CPointMap ptSrc(GetTopPoint());
	CPointMap ptNow(ptSrc);

	if ( ptSrc.m_map != ptDst.m_map )	// Different map
		return this->CanSeeLOS_New_Failed(pptBlock, ptNow);

	if ( ptSrc == ptDst )	// Same point ^^
		return true;

	const short iTotalZ = ptSrc.m_z + GetHeightMount(true);
	ptSrc.m_z = static_cast<char>(minimum(iTotalZ, UO_SIZE_Z));	//true - substract one from the height because of eyes height
	WARNLOS(("Total Z: %d\n", ptSrc.m_z));

	int dx, dy, dz;
	dx = ptDst.m_x - ptSrc.m_x;
	dy = ptDst.m_y - ptSrc.m_y;
	dz = ptDst.m_z - ptSrc.m_z;

    float dist2d, dist3d;
	dist2d = sqrt(static_cast<float>(dx * dx + dy * dy));
	if ( dz )
		dist3d = sqrt(dist2d * dist2d + dz * dz);
	else
		dist3d = dist2d;

	if ( APPROX(dist2d) > static_cast<float>(iMaxDist) )
	{
		WARNLOS(("( APPROX(dist2d)(%f) > ((double)iMaxDist)(%f) ) --> NOLOS\n", APPROX(dist2d), static_cast<float>(iMaxDist)));
		return CanSeeLOS_New_Failed(pptBlock, ptNow);
	}

	float dFactorX, dFactorY, dFactorZ;
	dFactorX = dx / dist3d;
	dFactorY = dy / dist3d;
	dFactorZ = dz / dist3d;

    float nPx, nPy, nPz;
	nPx = ptSrc.m_x;
	nPy = ptSrc.m_y;
	nPz = ptSrc.m_z;

	std::vector<CPointMap> path;
	while (BETWEENPOINT(nPx, ptDst.m_x, ptSrc.m_x) && BETWEENPOINT(nPy, ptDst.m_y, ptSrc.m_y) && BETWEENPOINT(nPz, ptDst.m_z, ptSrc.m_z))
	{
        dx = static_cast<int>(APPROX(nPx));
        dy = static_cast<int>(APPROX(nPy));
        dz = static_cast<int>(APPROX(nPz));
        // Add point to vector
        if (!path.empty())
        {
            if (const CPointMap &ptEnd = path.back(); ptEnd.m_x != dx || ptEnd.m_y != dy || ptEnd.m_z != dz)
                path.emplace_back(static_cast<word>(dx), static_cast<word>(dy), static_cast<char>(dz), ptSrc.m_map);
        }
        else
        {
            path.emplace_back(static_cast<word>(dx), static_cast<word>(dy), static_cast<char>(dz), ptSrc.m_map);
        }
        WARNLOS(("PATH X:%d Y:%d Z:%d\n", dx, dy, dz));
        nPx += dFactorX;
        nPy += dFactorY;
        nPz += dFactorZ;
    }

	if ( !path.empty() )
	{
		if ( path.back() != ptDst )
			path.emplace_back(ptDst.m_x, ptDst.m_y, ptDst.m_z, ptDst.m_map);
	}
	else
	{
		path.clear();
		return CanSeeLOS_New_Failed(pptBlock, ptNow);
	}

	WARNLOS(("Path calculated %" PRIuSIZE_T "\n", path.size()));
	// Ok now we should loop through all the points and checking for maptile, staticx, items, multis.
	// If something is in the way and it has the wrong flags LOS return false

	const CServerMapBlock *pBlock			= nullptr;		// Block of the map (for statics)
	const CUOStaticItemRec *pStatic			= nullptr;		// Statics iterator (based on SphereMapBlock)
	const CUOMulti *pMulti 				= nullptr;		// Multi Def (multi check)
	const CUOMultiItemRec_HS *pMultiItem	= nullptr;		// Multi item iterator
	CRegion *pRegion					= nullptr;		// Nulti regions
	CRegionLinks rlinks;								// Links to multi regions
	CItem *pItem						= nullptr;
	CItemBase *pItemDef 				= nullptr;
	CItemBaseDupe *pDupeDef				= nullptr;

	uint64 qwTFlags = 0;
	height_t Height = 0;
	word terrainid = 0;
	bool bPath = true;
	bool bNullTerrain = false;

	CRegion *pSrcRegion = ptSrc.GetRegion(REGION_TYPE_AREA|REGION_TYPE_ROOM|REGION_TYPE_MULTI);
	CRegion *pNowRegion = nullptr;

	int lp_x = 0, lp_y = 0;
	short min_z = 0, max_z = 0;

	for (uint i = 0, pathSize = static_cast<uint>(path.size()); i < pathSize; lp_x = ptNow.m_x, lp_y = ptNow.m_y, pItemDef = nullptr, pStatic = nullptr, pMulti = nullptr, pMultiItem = nullptr, min_z = 0, max_z = 0, ++i )
	{
		ptNow = path[i];
		WARNLOS(("---------------------------------------------\n"));
		WARNLOS(("Point %d,%d,%d \n", ptNow.m_x, ptNow.m_y, ptNow.m_z));

		pNowRegion = ptNow.GetRegion(REGION_TYPE_AREA|REGION_TYPE_ROOM|REGION_TYPE_MULTI);

		if ( (flags & LOS_NO_OTHER_REGION) && (pSrcRegion != pNowRegion) )
		{
			WARNLOS(("flags & 0200 and path is leaving my region - BLOCK\n"));
			bPath = false;
			break;
		}

		if ( (flags & LOS_NC_MULTI) && ptNow.GetRegion(REGION_TYPE_MULTI) && (ptNow.GetRegion(REGION_TYPE_MULTI) != ptSrc.GetRegion(REGION_TYPE_MULTI)) )
		{
			WARNLOS(("flags & 0400 and path is crossing another multi - BLOCK\n"));
			bPath = false;
			break;
		}

		if ( (lp_x != ptNow.m_x) || (lp_y != ptNow.m_y) )
		{
			WARNLOS(("\tLoading new map block.\n"));
			pBlock = CWorldMap::GetMapBlock(ptNow);
		}

		if ( !pBlock ) // something is wrong
		{
			WARNLOS(("GetMapBlock Failed\n"));
			bPath = false;
			break;
		}

		if ( !(flags & LOS_NB_TERRAIN) )
		{
			if ( !((flags & LOS_NB_LOCAL_TERRAIN) && (pSrcRegion == pNowRegion)) )
			{
				// ------ MapX.mul Check ----------
				terrainid = pBlock->GetTerrain(UO_BLOCK_OFFSET(ptNow.m_x), UO_BLOCK_OFFSET(ptNow.m_y))->m_wTerrainIndex;
				WARNLOS(("Terrain %d\n", terrainid));

                if (const IT_TYPE itTerrain = CWorldMap::GetTerrainItemType(terrainid);
                    (flags & LOS_FISHING) && (ptSrc.GetDist(ptNow) >= 2) && (itTerrain != IT_WATER) && (itTerrain != IT_NORMAL) )
				{
					WARNLOS(("Terrain %d blocked - flags & LOS_FISHING, distance >= 2 and type of pItemDef is not IT_WATER\n", terrainid));
					WARNLOS(("ptSrc: %d,%d,%d; ptNow: %d,%d,%d; terrainid: %d; terrainid type: %d\n", ptSrc.m_x, ptSrc.m_y, ptSrc.m_z, ptNow.m_x, ptNow.m_y, ptNow.m_z, terrainid, itTerrain));
					bPath = false;
					break;
				}

				//#define MAPTILEMIN minimum(minimum(minimum(pBlock->GetTerrain(0,0)->m_z, pBlock->GetTerrain(0,1)->m_z), pBlock->GetTerrain(1,0)->m_z), pBlock->GetTerrain(1,1)->m_z)
				//#define MAPTILEMAX maximum(maximum(maximum(pBlock->GetTerrain(0,0)->m_z, pBlock->GetTerrain(0,1)->m_z), pBlock->GetTerrain(1,0)->m_z), pBlock->GetTerrain(1,1)->m_z);
				//#define MAPTILEZ pBlock->GetTerrain(UO_BLOCK_OFFSET(ptNow.m_x), UO_BLOCK_OFFSET(ptNow.m_y))->m_z;

				if ( (terrainid != TERRAIN_HOLE) && (terrainid != 475) )
				{
					if ( terrainid < 430 || terrainid > 437 )
					{
						/*this stuff should do some checking for surrounding items:
						aaa
						aXa
						aaa
						min_z is determined as a minimum of all a/X terrain, where X is ptNow
						*/
						byte pos_x = UO_BLOCK_OFFSET(ptNow.m_x) > 1 ? UO_BLOCK_OFFSET(ptNow.m_x - 1) : 0;
						byte pos_y = UO_BLOCK_OFFSET(ptNow.m_y) > 1 ? UO_BLOCK_OFFSET(ptNow.m_y - 1) : 0;
						const byte defx = UO_BLOCK_OFFSET(ptNow.m_x);
						const byte defy = UO_BLOCK_OFFSET(ptNow.m_y);
						min_z = pBlock->GetTerrain(pos_x, pos_y)->m_z;
						max_z = pBlock->GetTerrain(defx, defy)->m_z;
						for ( byte posy = pos_y; (abs(defx - UO_BLOCK_OFFSET(pos_x)) <= 1 && pos_x <= 7); ++pos_x )
						{
							for ( pos_y = posy; (abs(defy - UO_BLOCK_OFFSET(pos_y)) <= 1 && pos_y <= 7); ++pos_y )
							{
								char terrain_z = pBlock->GetTerrain(pos_x, pos_y)->m_z;
								min_z = minimum(min_z, terrain_z);
							}
						}
						//min_z = MAPTILEZ;
						//max_z = MAPTILEZ;
						WARNLOS(("Terrain %d - m:%d M:%d\n", terrainid, min_z, max_z));
						if ( CUOMapMeter::IsTerrainNull(terrainid) )
							bNullTerrain = true; //what if there are some items on that hole?
						if ( (min_z <= ptNow.m_z && max_z >= ptNow.m_z) && (ptNow.m_x != ptDst.m_x || ptNow.m_y != ptDst.m_y || min_z > ptDst.m_z || max_z < ptDst.m_z) )
						{
							WARNLOS(("Terrain %d - m:%d M:%d - block\n", terrainid, min_z, max_z));
							bPath = false;
							break;
						}
                        if (const CUOTerrainInfo land(terrainid); (land.m_flags & UFLAG1_WATER) && (flags & LOS_NC_WATER) )
							bNullTerrain = true;
					}
				}
				//#undef MAPTILEMIN
				//#undef MAPTILEMAX
				//#undef MAPTILEZ
			}
		}

		// ------ StaticsX.mul Check --------
		if ( !(flags & LOS_NB_STATIC) )
		{
			if ( !((flags & LOS_NB_LOCAL_STATIC) && (pSrcRegion == pNowRegion)) )
			{
                const uint uiStaticMaxQty = pBlock->m_Statics.GetStaticQty();
				for ( uint s = 0; s < uiStaticMaxQty; pStatic = nullptr, pItemDef = nullptr, ++s )
				{
					pStatic = pBlock->m_Statics.GetStatic(s);
					if ( (pStatic->m_x + pBlock->m_x != ptNow.m_x) || (pStatic->m_y + pBlock->m_y != ptNow.m_y) )
						continue;

					//Fix for Stacked items blocking view
					if ( (pStatic->m_x == ptDst.m_x) && (pStatic->m_y == ptDst.m_y) && (pStatic->m_z >= GetTopZ()) && (pStatic->m_z <= ptSrc.m_z) )
						continue;

					pItemDef = CItemBase::FindItemBase(pStatic->GetDispID());
					qwTFlags = 0;
					Height = 0;
					bNullTerrain = false;

					if ( !pItemDef )
					{
						WARNLOS(("STATIC - Cannot get pItemDef for item (0%x)\n", pStatic->GetDispID()));
					}
					else
					{
                        if (pItemDef->Can(CAN_I_BLOCKLOS))
                        {
                            WARNLOS(("pStatic blocked by CAN_I_BLOCKLOS"));
                            bPath = false;
                            break;
                        }

						if ( (flags & LOS_FISHING) && (ptSrc.GetDist(ptNow) >= 2) && (pItemDef->GetType() != IT_WATER) &&
							( pItemDef->Can(CAN_I_DOOR | CAN_I_PLATFORM | CAN_I_BLOCK | CAN_I_CLIMB | CAN_I_FIRE | CAN_I_ROOF | CAN_I_BLOCKLOS | CAN_I_BLOCKLOS_HEIGHT)) )
						{
							WARNLOS(("pStatic blocked - flags & 0800, distance >= 2 and type of pItemDef is not IT_WATER\n"));
							bPath = false;
							break;
						}

						qwTFlags = pItemDef->GetTFlags();
						Height = pItemDef->GetHeight();

						const ITEMID_TYPE iStaticDispID = pStatic->GetDispID();
						if ( pItemDef->GetID() != iStaticDispID) //not a parent item
						{
							WARNLOS(("Not a parent item (STATIC)\n"));
							pDupeDef = CItemBaseDupe::GetDupeRef(iStaticDispID);
							if ( !pDupeDef )
							{
								g_Log.EventDebug("AdvancedLoS: Failed to get non-parent reference (static) (DispID 0%x) (X: %d Y: %d Z: %hhd M: %hhu)\n", iStaticDispID, ptNow.m_x, ptNow.m_y, pStatic->m_z, ptNow.m_map);
							}
							else
							{
								qwTFlags = pDupeDef->GetTFlags();
								Height = pDupeDef->GetHeight();
							}
						}
						else
						{
							WARNLOS(("Parent item (STATIC)\n"));
						}

						Height = (qwTFlags & UFLAG2_CLIMBABLE) ? Height / 2 : Height;

						if ( ((qwTFlags & (UFLAG1_WALL|UFLAG1_BLOCK|UFLAG2_PLATFORM)) || (pItemDef->Can(CAN_I_BLOCKLOS_HEIGHT))) && !((qwTFlags & UFLAG2_WINDOW) && (flags & LOS_NB_WINDOWS)) )
						{
							WARNLOS(("pStatic %0x %d,%d,%d - %d\n", iStaticDispID, pStatic->m_x, pStatic->m_y, pStatic->m_z, Height));
							min_z = pStatic->m_z;
							max_z = minimum(Height + min_z, UO_SIZE_Z);
							WARNLOS(("qwTFlags(0%" PRIx64 ")\n", qwTFlags));

							WARNLOS(("pStatic %0x Z check: %d,%d (Now: %d) (Dest: %d).\n", iStaticDispID, min_z, max_z, ptNow.m_z, ptDst.m_z));
							if ( (min_z <= ptNow.m_z) && (max_z >= ptNow.m_z) )
							{
								if ( ptNow.m_x != ptDst.m_x || ptNow.m_y != ptDst.m_y || min_z > ptDst.m_z || max_z < ptDst.m_z )
								{
									WARNLOS(("pStatic blocked - m:%d M:%d\n", min_z, max_z));
									bPath = false;
									break;
								}
							}
						}
					}
				}
			}
		}

		if ( !bPath )
			break;

		// --------- In game items ----------
		if ( !(flags & LOS_NB_DYNAMIC) )
		{
			if ( !((flags & LOS_NB_LOCAL_DYNAMIC) && (pSrcRegion == pNowRegion)) )
			{
				auto AreaItems = CWorldSearchHolder::GetInstance(ptNow, 0);
				for (;;)
				{
					pItem = AreaItems->GetItem();
					if ( !pItem )
						break;
					if ( pItem->GetUnkPoint().m_x != ptNow.m_x || pItem->GetUnkPoint().m_y != ptNow.m_y )
						continue;
					if ( !CanSeeItem(pItem) )
						continue;

					//Fix for Stacked items blocking view
					if ( (pItem->GetUnkPoint().m_x == ptDst.m_x) && (pItem->GetUnkPoint().m_y == ptDst.m_y) && (pItem->GetUnkPoint().m_z >= GetTopZ()) && (pItem->GetUnkPoint().m_z <= ptSrc.m_z) )
						continue;

					pItemDef = static_cast<CItemBase*>(pItem->Base_GetDef());
					qwTFlags = 0;
					Height = 0;
					bNullTerrain = false;

					if ( !pItemDef )
					{
						WARNLOS(("DYNAMIC - Cannot get pItemDef for item (0%x)\n", pItem->GetDispID()));
					}
					else
					{
                        if (pItem->Can(CAN_I_BLOCKLOS))
                        {
                            WARNLOS(("pItem blocked by CAN_I_BLOCKLOS"));
                            bPath = false;
                            break;
                        }

						if ( (flags & LOS_FISHING) && (ptSrc.GetDist(ptNow) >= 2) && (pItem->GetType() != IT_WATER) &&
							( pItem->Can(CAN_I_DOOR | CAN_I_PLATFORM | CAN_I_BLOCK | CAN_I_CLIMB | CAN_I_FIRE | CAN_I_ROOF | CAN_I_BLOCKLOS | CAN_I_BLOCKLOS_HEIGHT) ) )
						{
							WARNLOS(("pItem blocked - flags & 0800, distance >= 2 and type of pItemDef is not IT_WATER\n"));
							bPath = false;
							break;
							//return CanSeeLOS_New_Failed(pptBlock, ptNow);
						}

						qwTFlags = pItemDef->GetTFlags();
						Height = pItemDef->GetHeight();

						if ( pItemDef->GetID() != pItem->GetDispID() )	//not a parent item
						{
							WARNLOS(("Not a parent item (DYNAMIC)\n"));
							pDupeDef = CItemBaseDupe::GetDupeRef(pItem->GetDispID());
							if ( !pDupeDef )
							{
                                // Not an error: i have changed the DISPID of the item.
                                if (const CItemBase *pParentDef = CItemBase::FindItemBase(pItem->GetDispID()))
                                {
                                    qwTFlags = pParentDef->GetTFlags();
                                    Height = pParentDef->GetHeight();
                                }
                                else
                                    g_Log.EventDebug("AdvancedLoS: Failed to get reference (dynamic): non-dupe, baseless dispid (DispID 0%x) (X: %d Y: %d Z: %hhd M: %hhu)\n", pItem->GetDispID(), ptNow.m_x, ptNow.m_y, pItem->GetUnkZ(), ptNow.m_map);
							}
							else
							{
                                // It's a dupe item
								qwTFlags = pDupeDef->GetTFlags();
								Height = pDupeDef->GetHeight();
							}
						}
						else
						{
							WARNLOS(("Parent item (DYNAMIC)\n"));
						}

						Height = (qwTFlags & UFLAG2_CLIMBABLE) ? Height / 2 : Height;

						if ( ((qwTFlags & (UFLAG1_WALL|UFLAG1_BLOCK|UFLAG2_PLATFORM)) || pItem->Can(CAN_I_BLOCKLOS_HEIGHT)) && !((qwTFlags & UFLAG2_WINDOW) && (flags & LOS_NB_WINDOWS)) )
						{
							WARNLOS(("pItem %0x(%0x) %d,%d,%d - %d\n", static_cast<dword>(pItem->GetUID()), pItem->GetDispID(), pItem->GetUnkPoint().m_x, pItem->GetUnkPoint().m_y, pItem->GetUnkPoint().m_z, Height));
							min_z = pItem->GetUnkPoint().m_z;
							max_z = minimum(Height + min_z, UO_SIZE_Z);
							WARNLOS(("qwTFlags(0%" PRIx64 ")\n", qwTFlags));

							WARNLOS(("pItem %0x(%0x) Z check: %d,%d (Now: %d) (Dest: %d).\n", static_cast<dword>(pItem->GetUID()), pItem->GetDispID(), min_z, max_z, ptNow.m_z, ptDst.m_z));
							if ( min_z <= ptNow.m_z && max_z >= ptNow.m_z )
							{
								if ( ptNow.m_x != ptDst.m_x || ptNow.m_y != ptDst.m_y || min_z > ptDst.m_z || max_z < ptDst.m_z )
								{
									WARNLOS(("pItem blocked - m:%d M:%d\n", min_z, max_z));
									bPath = false;
									break;
								}
							}
						}
					}
				}
			}
		}

		if ( !bPath )
			break;

		// ----------- Multis ---------------

		if ( !(flags & LOS_NB_MULTI) )
		{
			if ( !((flags & LOS_NB_LOCAL_MULTI) && (pSrcRegion == pNowRegion)) )
			{
                if (size_t iQtyr = ptNow.GetRegions(REGION_TYPE_MULTI, &rlinks); iQtyr > 0 )
				{
					for ( size_t ii = 0; ii < iQtyr; pMulti = nullptr, ++ii, pItem = nullptr, pRegion = nullptr )
					{
						pRegion = rlinks[ii];
						if ( pRegion )
							pItem = pRegion->GetResourceID().ItemFindFromResource();

						if ( !pItem )
							continue;

						pMulti = g_Cfg.GetMultiItemDefs(pItem);
						if ( !pMulti )
							continue;

						uint iQty = pMulti->GetItemCount();
						for ( uint iii = 0; iii < iQty; pItemDef = nullptr, pMultiItem = nullptr, ++iii )
						{
							pMultiItem = pMulti->GetItem(iii);
							if ( !pMultiItem )
								break;
							if ( !pMultiItem->m_visible )
								continue;
							if ( (pMultiItem->m_dx + pItem->GetTopPoint().m_x != ptNow.m_x) || (pMultiItem->m_dy + pItem->GetTopPoint().m_y != ptNow.m_y) )
								continue;

							pItemDef = CItemBase::FindItemBase(pMultiItem->GetDispID());
							qwTFlags = 0;
							Height = 0;
							bNullTerrain = false;

							if ( !pItemDef )
							{
								WARNLOS(("MULTI - Cannot get pItemDef for item (0%x)\n", pMultiItem->GetDispID()));
							}
							else
							{
                                if (pItemDef->Can(CAN_I_BLOCKLOS))
                                {
                                    WARNLOS(("pMultiItem blocked by CAN_I_BLOCKLOS"));
                                    bPath = false;
                                    break;
                                }

								if ( (flags & LOS_FISHING) && (ptSrc.GetDist(ptNow) >= 2) && (pItemDef->GetType() != IT_WATER) &&
									( pItemDef->Can(CAN_I_DOOR | CAN_I_PLATFORM | CAN_I_BLOCK | CAN_I_CLIMB | CAN_I_FIRE | CAN_I_ROOF | CAN_I_BLOCKLOS | CAN_I_BLOCKLOS_HEIGHT) ) )
								{
									WARNLOS(("pMultiItem blocked - flags & 0800, distance >= 2 and type of pItemDef is not IT_WATER\n"));
									bPath = false;
									break;
									//return CanSeeLOS_New_Failed(pptBlock, ptNow);
								}

								qwTFlags = pItemDef->GetTFlags();
								Height = pItemDef->GetHeight();

								if ( pItemDef->GetID() != pMultiItem->GetDispID() ) //not a parent item
								{
									WARNLOS(("Not a parent item (MULTI)\n"));
									pDupeDef = CItemBaseDupe::GetDupeRef(pMultiItem->GetDispID());
									if ( !pDupeDef )
									{
										g_Log.EventDebug("AdvancedLoS: Failed to get non-parent reference (multi) (DispID 0%x) (X: %d Y: %d Z: %hhd M: %hhu)\n", pMultiItem->GetDispID(), ptNow.m_x, ptNow.m_y, pMultiItem->m_dz + pItem->GetTopPoint().m_z, ptNow.m_map);
									}
									else
									{
										qwTFlags = pDupeDef->GetTFlags();
										Height = pDupeDef->GetHeight();
									}
								}
								else
								{
									WARNLOS(("Parent item (MULTI)\n"));
								}

								Height = (qwTFlags & UFLAG2_CLIMBABLE) ? Height / 2 : Height;

								if ( ((qwTFlags & (UFLAG1_WALL|UFLAG1_BLOCK|UFLAG2_PLATFORM) || pItemDef->Can(CAN_I_BLOCKLOS_HEIGHT))) && !((qwTFlags & UFLAG2_WINDOW) && (flags & LOS_NB_WINDOWS)) )
								{
									WARNLOS(("pMultiItem %0x %d,%d,%d - %d\n", pMultiItem->GetDispID(), pMultiItem->m_dx, pMultiItem->m_dy, pMultiItem->m_dz, Height));
									min_z = static_cast<char>(pMultiItem->m_dz) + pItem->GetTopPoint().m_z;
									max_z = minimum(Height + min_z, UO_SIZE_Z);
									WARNLOS(("qwTFlags(0%" PRIx64 ")\n", qwTFlags));

									if ( min_z <= ptNow.m_z && max_z >= ptNow.m_z )
									{
										if ( ptNow.m_x != ptDst.m_x || ptNow.m_y != ptDst.m_y || min_z > ptDst.m_z || max_z < ptDst.m_z )
										{
											WARNLOS(("pMultiItem blocked - m:%d M:%d\n", min_z, max_z));
											bPath = false;
											break;
										}
									}
								}
							}
						}

						if ( !bPath )
							break;
					}
				}
			}
		}

		if ( bNullTerrain )
			bPath = false;

		if ( !bPath )
			break;
	}

	path.clear();

	if ( !bPath )
		return CanSeeLOS_New_Failed(pptBlock, ptNow);

	return true;
}

#undef WARNLOS
#undef APPROX
#undef BETWEENPOINT
//#undef CALCITEMHEIGHT

bool CChar::CanSeeLOS( const CObjBaseTemplate *pObj, const word wFlags, const bool fCombatCheck ) const
{
	ADDTOCALLSTACK("CChar::CanSeeLOS");
	// WARNING: CanSeeLOS is an expensive function (lot of calculations but	most importantly it has to read the UO files, and file I/O is slow).

	if ( !CanSee(pObj) )
		return false;

	pObj = pObj->GetTopLevelObj();
	if ( !pObj )
		return false;

	if ( (m_pPlayer && (g_Cfg.m_iAdvancedLos & ADVANCEDLOS_PLAYER)) || (m_pNPC && (g_Cfg.m_iAdvancedLos & ADVANCEDLOS_NPC)) )
	{
		CPointMap pt = pObj->GetTopPoint();
        if (const auto pChar = dynamic_cast<const CChar *>(pObj) )
		{
            const short iTotalZ = pt.m_z + pChar->GetHeightMount(true);
			pt.m_z = static_cast<char>minimum(iTotalZ, UO_SIZE_Z);
		}
		return CanSeeLOS_New(pt, nullptr, pObj->GetVisualRange(), wFlags, fCombatCheck);
	}

    return CanSeeLOS(pObj->GetTopPoint(), nullptr, pObj->GetVisualRange(), wFlags, fCombatCheck);
}
