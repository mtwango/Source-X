#include "../sphere/threads.h"
#include "CServerConfig.h"
#include "CWorld.h"
#include "CWorldGameTime.h"


#define TRAMMEL_SYNODIC_PERIOD		105		// in game world minutes
#define FELUCCA_SYNODIC_PERIOD		840		// in game world minutes


const CServerTime& CWorldGameTime::GetCurrentTime() noexcept // static
{
	//ADDTOCALLSTACK_DEBUG("CWorldGameTime::GetCurrentTime");
	return g_World._GameClock.GetCurrentTime();  // Time in milliseconds
}


int64 CWorldGameTime::GetCurrentTimeInGameMinutes(const int64 basetime ) noexcept // static
{
	// Get the time of the day in GameWorld minutes.
    // basetime = ticks.
	// 8 real world seconds = 1 game minute.
	// 1 real minute = 7.5 game minutes
	// 3.2 hours = 1 game day.

	return ( basetime / g_Cfg.m_iGameMinuteLength );
}

int64 CWorldGameTime::GetCurrentTimeInGameMinutes() noexcept // static
{
	return GetCurrentTimeInGameMinutes(GetCurrentTime().GetTimeRaw());
}


int64 CWorldGameTime::GetNextNewMoon(const bool fMoonIndex ) // static
{
	ADDTOCALLSTACK("CWorldGameTime::GetNextNewMoon");
	// "Predict" the next new moon for this moon
	// Get the period
	int64 const iSynodic = fMoonIndex ? FELUCCA_SYNODIC_PERIOD : TRAMMEL_SYNODIC_PERIOD;

	// Add a "month" to the current game time
	int64 const iNextMonth = GetCurrentTimeInGameMinutes() + iSynodic;

	// Get the game time when this cycle will start
    const int64 iNewStart = static_cast<int64>(iNextMonth - static_cast<double>(iNextMonth % iSynodic));
	return iNewStart * g_Cfg.m_iGameMinuteLength;

}

uint CWorldGameTime::GetMoonPhase(const bool fMoonIndex) // static
{
	ADDTOCALLSTACK("CWorldGameTime::GetMoonPhase");
	// bMoonIndex is FALSE if we are looking for the phase of Trammel,
	// TRUE if we are looking for the phase of Felucca.

	// There are 8 distinct moon phases:  New, Waxing Crescent, First Quarter, Waxing Gibbous,
	// Full, Waning Gibbous, Third Quarter, and Waning Crescent

	// To calculate the phase, we use the following formula:
	//				CurrentTime % SynodicPeriod
	//	Phase = 	-----------------------------------------     * 8
	//			              SynodicPeriod
	//

	int64 const iCurrentTime = GetCurrentTimeInGameMinutes();	// game world time in minutes

	if (!fMoonIndex)	// Trammel
		return IMulDiv( iCurrentTime % TRAMMEL_SYNODIC_PERIOD, 8, TRAMMEL_SYNODIC_PERIOD );
    // Luna2
    return IMulDiv(iCurrentTime % FELUCCA_SYNODIC_PERIOD, 8, FELUCCA_SYNODIC_PERIOD);
}
