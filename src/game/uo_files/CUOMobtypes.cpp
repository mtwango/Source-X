/**
* @file CUOMobtypes.cpp
*
*/

#include "../../sphere/threads.h"
//#include "../../common/CExpression.h" // included in the precompiled header
#include "../../common/CLog.h"
#include "../../common/CUOInstall.h"
#include "../../game/uo_files/uofiles_enums_creid.h"
#include "CUOMobtypes.h"
#include <string_view>

void CUOMobTypes::Load()
{
    ADDTOCALLSTACK("CUOMobTypes::Load");
    g_Log.Event(LOGM_INIT, "Caching mobtypes.txt...\n");

    _vMobTypesEntries.clear();

    if (CSFileText csvMobTypes; g_Install.OpenFile(csvMobTypes, "mobtypes.txt", static_cast<word>((OF_READ | OF_TEXT | OF_DEFAULTMODE))))
    {
        _vMobTypesEntries.resize(CREID_QTY);
        for (size_t i = 0; i < _vMobTypesEntries.size(); ++i)
        {
            _vMobTypesEntries[i] = CUOMobTypesEntry{MOBTE_EQUIPMENT, 0};
        }

        tchar* ptcTemp = Str_GetTemp();
        size_t uiLineCount = 0;
        CUOMobTypesEntry mobTypesRow {};
        while (!csvMobTypes.IsEOF())
        {
            csvMobTypes.ReadString(ptcTemp, 200);
            if (*ptcTemp)
            {
                ++ uiLineCount;

                int iLineLength = static_cast<int>(strnlen(ptcTemp, Str_TempLength()));
                iLineLength = Str_TrimEndWhitespace(ptcTemp, iLineLength);

                //Empty line or commented
                if (iLineLength == 0 || ptcTemp[0] == '#')
                    continue;
                GETNONWHITESPACE(ptcTemp);
                if (ptcTemp[0] == '#')
                    continue;

                //Split the string
                tchar* pptcSplitArray[4]; // Bigger on purpose, put possible trailing garbage in the fourth element to keep clean the third.
                if (const int iQty = Str_ParseCmds(ptcTemp, pptcSplitArray, std::size(pptcSplitArray), " \t#"); iQty < 3)
                {
                    g_Log.EventError("Mobtypes.txt: not enough parameters on line %" PRIuSIZE_T " \n", uiLineCount);
                    continue;
                }

                std::optional<dword> iconv = Str_ToU(pptcSplitArray[0], 10);
                if (!iconv.has_value())
                {
                    g_Log.EventError(
                        "Mobtypes.txt: Invalid char ID on line %" PRIuSIZE_T ".\n", uiLineCount);
                    continue;
                }
                const uint uiAnimIndex = *iconv;

                if (const std::string_view sType(pptcSplitArray[1]); sType == "MONSTER")
                {
                    mobTypesRow.m_uiType = MOBTE_MONSTER;
                }
                else if (sType == "SEA_MONSTER")
                {
                    mobTypesRow.m_uiType = MOBTE_SEA_MONSTER;
                }
                else if (sType == "ANIMAL")
                {
                    mobTypesRow.m_uiType = MOBTE_ANIMAL;
                }
                else if (sType == "HUMAN")
                {
                    mobTypesRow.m_uiType = MOBTE_HUMAN;
                }
                else if (sType == "EQUIPMENT")
                {
                    mobTypesRow.m_uiType = MOBTE_EQUIPMENT;
                }
                else
                {
                    mobTypesRow.m_uiType = MOBTE_QTY;
                    g_Log.EventError("Mobtypes.txt: wrong type found on line %" PRIuSIZE_T " \n", uiLineCount);
                }

                //mobTypesRow.m_flags = (dword)std::strtoul(pptcSplitArray[2], nullptr, 16);
                iconv = Str_ToU(pptcSplitArray[2], 16);
                if (!iconv.has_value())
                {
                    g_Log.EventError(
                        "Mobtypes.txt: Invalid flags '%s' for char ID %" PRIu32 " on line %" PRIuSIZE_T ".\n",
                        pptcSplitArray[2], uiAnimIndex, uiLineCount);
                    continue;
                }
                mobTypesRow.m_uiFlags = *iconv;

                if (uiAnimIndex <= _vMobTypesEntries.size()) //Safety check
                {
                    _vMobTypesEntries[uiAnimIndex] = mobTypesRow;
                }
                else
                {
                    g_Log.EventError(
                        "Mobtypes.txt: trying to load data for an invalid char ID %" PRIu32 " on line %" PRIuSIZE_T ".\n",
                        uiAnimIndex, uiLineCount);
                }
            }
        }

        csvMobTypes.Close();
    }
}

const CUOMobTypesEntry* CUOMobTypes::GetEntry(const uint id) const
{
    ASSERT(id < _vMobTypesEntries.size());
    return &_vMobTypesEntries[id];
}

