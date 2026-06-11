#include "../common/CLog.h"
#include "../common/CUID.h"
#include "CObjBase.h"
#include "CServerConfig.h"
#include "CServerTime.h"
#include "CWorld.h"
#include "CTimedFunctionHandler.h"

#define TF_TICK_MAGIC_NUMBER		99

CTimedFunctionHandler::CTimedFunctionHandler() :
	_strLoadBufferCommand(CTimedFunction::kuiCommandSize, '\0'),
	_strLoadBufferNumbers(CTimedFunction::kuiCommandSize, '\0')
{
}

int64 CTimedFunctionHandler::IsTimer(const CUID& uid, const lpctstr ptcCommand) const
{
	ADDTOCALLSTACK("CTimedFunctionHandler::IsTimer");
	for (CSObjContRec* obj : _timedFunctions.GetIterationSafeCont())	// the end iterator changes at each stl container erase call
	{
        if (const auto *const tfObj = static_cast<CTimedFunction *>(obj); tfObj && tfObj->GetUID() == uid && (Str_Match(ptcCommand, tfObj->GetCommand()) == MATCH_VALID))
		{
			return tfObj->GetTimerAdjusted();
		}
	}
	return 0;
}

void CTimedFunctionHandler::ClearUID( const CUID& uid ) const
{
	ADDTOCALLSTACK("CTimedFunctionHandler::Erase");
	for (CSObjContRec* obj : _timedFunctions.GetIterationSafeCont())	// the end iterator changes at each stl container erase call
	{
        if (auto *const tfObj = static_cast<CTimedFunction *>(obj); tfObj && tfObj->GetUID() == uid)
		{
			g_World.ScheduleObjDeletion(tfObj);
		}
	}
}

void CTimedFunctionHandler::Stop(const CUID& uid, const lpctstr ptcCommand) const
{
	ADDTOCALLSTACK("CTimedFunctionHandler::Stop");
	for (CSObjContRec* obj : _timedFunctions.GetIterationSafeCont())
	{
        if (auto *tfObj = static_cast<CTimedFunction *>(obj); tfObj && tfObj->GetUID() == uid && (Str_Match(ptcCommand, tfObj->GetCommand()) == MATCH_VALID))
		{
			g_World.ScheduleObjDeletion(tfObj);
		}
	}
}

void CTimedFunctionHandler::Clear()
{
    ADDTOCALLSTACK("CTimedFunctionHandler::Clear");

    _timedFunctions.ClearContainer(false);
}

TRIGRET_TYPE CTimedFunctionHandler::Loop(const lpctstr ptcCommand, int iLoopsMade, const CScriptLineContext StartContext,
    CScript &s, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole * pSrc, CSString * pResult) const
{
	ADDTOCALLSTACK("CTimedFunctionHandler::Loop");
	for (CSObjContRec* obj : _timedFunctions.GetIterationSafeCont())
	{
		++iLoopsMade;
		if (g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes))
		{
			g_Log.EventError("Terminating loop cycle since it seems being dead-locked (%d iterations already passed).\n", iLoopsMade);
			return TRIGRET_ENDIF;
		}

        if (const auto *const tfObj = static_cast<CTimedFunction *>(obj); tfObj && !strcmpi(tfObj->GetCommand(), ptcCommand))
		{
			CObjBase* pObj = tfObj->GetUID().ObjFind();
			if (!pObj)
			{
			LoopStop:
				break;
			}

            const TRIGRET_TYPE iRet = pObj->OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pScriptArgs, pSrc, pResult);

			if (iRet == TRIGRET_BREAK)
			{
				goto LoopStop;
			}
			if ((iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE))
				return iRet;
			s.SeekContext(StartContext);
		}
	}

	return TRIGRET_ENDIF;
}

void CTimedFunctionHandler::Add(const CUID& uid, const int64 iTimeout, const char* pcCommand)
{
	ADDTOCALLSTACK("CTimedFunctionHandler::Add");
	ASSERT(pcCommand != nullptr);
	ASSERT(strlen(pcCommand) < CTimedFunction::kuiCommandSize);

	auto* tf = new CTimedFunction(uid, pcCommand);
    tf->_SetAwakeFlagRaw();
	_timedFunctions.InsertContentTail(tf);
	tf->SetTimeout(iTimeout);
}

int CTimedFunctionHandler::Load(const lpctstr ptcKeyword, const bool fQuoted, const lpctstr ptcArg)
{
	ADDTOCALLSTACK("CTimedFunctionHandler::Load");
	UnreferencedParameter(fQuoted);
	if (!ptcKeyword)
		return -1;

	static constexpr auto ptcErrorPair = "Invalid TimerF in %sdata.scp. Each TimerFCall and TimerFNumbers pair must be in that order.\n";
	if (!strnicmp(ptcKeyword, "TimerFCall", 11))
	{
		if (_strLoadBufferCommand[0] != '\0')
		{
			// A TimerFCall wasn't called before this TimerFNumbers, so tf doesn't contain a valid CTimedFunction object.
			g_Log.Event(LOGM_INIT | LOGL_ERROR, ptcErrorPair, SPHERE_FILE);
			return -1;
		}
		ASSERT(_strLoadBufferNumbers[0] == '\0');
		Str_CopyLimitNull(_strLoadBufferCommand.data(), ptcArg, _strLoadBufferCommand.size());
	}
	else if ( !strnicmp(ptcKeyword, "TimerFNumbers", 13) )
	{
		if (_strLoadBufferCommand[0] == '\0')
		{
			// A TimerFCall wasn't called before this TimerFNumbers, so tf doesn't contain a valid CTimedFunction object.
			g_Log.Event(LOGM_INIT | LOGL_ERROR, ptcErrorPair, SPHERE_FILE);
			return -1;
		}

		Str_CopyLimitNull(_strLoadBufferNumbers.data(), ptcArg, _strLoadBufferNumbers.size());	// because ptcArg is constant and Str_ParseCmds wants a non-constant string
		tchar* ppVal[3];
        if (const size_t iArgs = Str_ParseCmds(_strLoadBufferNumbers.data(), ppVal, std::size(ppVal), " ,\t"); iArgs != 3 )
        {
            g_Log.Event( LOGM_INIT|LOGL_ERROR, "Invalid TimerF line in %sdata.scp: %s=%s (arguments mismatch: 3 needed).\n", SPHERE_FILE, ptcKeyword, ptcArg);
            return -1;
        }

        const auto oldErrno = errno;
		errno = 0;

		// The "tick" value was used on the old 56* timerf ticking system. It always was a positive number.
		//  Now X replaces this value in the save files to TF_TICK_MAGIC_NUMBER, so that we know that it comes from a X save file.
		//  We need that info because on X we save the timeout in milliseconds, on 0.56 it was stored in tenths of second.
        const int tick = static_cast<int>(std::strtol(ppVal[0], nullptr, 10));
		if (errno == ERANGE)
		{
			g_Log.Event(LOGM_INIT | LOGL_ERROR, "Invalid TimerFNumbers in %sdata.scp. Invalid legacy tick (first value=%s).\n", SPHERE_FILE, ppVal[0]);
			errno = oldErrno;
			return -1;
		}

		const unsigned long uidTest = std::strtoul(ppVal[1], nullptr, 10);
        if ((errno == ERANGE) || (uidTest > UINT32_MAX))
        {
            g_Log.Event(LOGM_INIT|LOGL_ERROR, "Invalid TimerFNumbers in %sdata.scp. Invalid UID (second value=%s).\n", SPHERE_FILE, ppVal[1]);
            errno = oldErrno;
            return -1;
        }
		const uint uid = static_cast<uint>(uidTest);

        errno = 0;
		int64 elapsed = std::strtoll(ppVal[2], nullptr, 10);
        if (errno == ERANGE)
        {
            g_Log.Event(LOGM_INIT|LOGL_ERROR, "Invalid TimerFNumbers in %sdata.scp. Invalid elapsed time (third value=%s).\n", SPHERE_FILE, ppVal[2]);
            errno = oldErrno;
            return -1;
        }
        errno = oldErrno;

		if (tick < TF_TICK_MAGIC_NUMBER)
		{
			// It's a 0.56 save file. Convert this timeout from ticks (tenths of second) in milliseconds.
			elapsed *= MSECS_PER_TENTH;
		}
		Add(CUID(uid), elapsed, _strLoadBufferCommand.c_str());
		_strLoadBufferCommand[0] = '\0';
		_strLoadBufferNumbers[0] = '\0';
	}

	return 0;
}

void CTimedFunctionHandler::r_Write( CScript & s ) const
{
	ADDTOCALLSTACK("CTimedFunctionHandler::r_Write");
	for (CSObjContRec* obj : _timedFunctions.GetIterationSafeCont())
	{
        const auto *const tfObj = static_cast<CTimedFunction*>(obj);
        ASSERT(tfObj);
        if (const CUID &uid = tfObj->GetUID(); uid.IsValidUID())
		{
			s.WriteKeyFormat("TimerFCall", "%s", tfObj->GetCommand());
			s.WriteKeyFormat("TimerFNumbers", STRINGIFY(TF_TICK_MAGIC_NUMBER) ",%" PRIu32 ",%" PRId64, uid.GetObjUID(), tfObj->GetTimerAdjusted());
		}
	}
}
