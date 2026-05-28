/**
* @file CNetworkManager.h
* @brief Coordinates input/output network threads (handles incoming connections and spawns child threads to process i/o)
*/

#ifndef _INC_CNETWORKMANAGER_H
#define _INC_CNETWORKMANAGER_H

#include "../common/sphere_library/CSObjList.h"
#include "CIPHistoryManager.h"
#include "CPacketManager.h"


class CNetState;
class CNetworkThread;

class CNetworkManager
{
    CNetState** m_states;			// client state pool
    int  m_stateCount;				// client state count
    int  m_lastGivenSlot;			// last slot index assigned
    bool m_isThreaded;

    typedef std::deque<CNetworkThread*> NetworkThreadList;
    NetworkThreadList m_threads;	// list of network threads
    IPHistoryManager m_ips;			// ip history
    CSObjList m_clients;			// current list of clients (CClient)
    PacketManager m_packets;		// packet handlers

public:
    static const char* m_sClassName;
    CNetworkManager();
    ~CNetworkManager();

private:
    CNetworkManager(const CNetworkManager& copy);
    CNetworkManager& operator=(const CNetworkManager& other);

public:
    void start();
    void stop();
    void tick();

    bool checkNewConnection();				// check if a new connection is waiting to be accepted
    void acceptNewConnection();				// accept a new connection

    void processAllInput();					// process network input (NOT THREADSAFE)
    void processAllOutput();				// process network output (NOT THREADSAFE)
    size_t flush(CNetState* state);				// process all output for a client
    void flushAllClients();					// force each thread to flush output

    const PacketManager& getPacketManager() const noexcept { return m_packets; }		// get packet manager
    IPHistoryManager& getIPHistoryManager() noexcept { return m_ips; }	// get ip history manager
    bool isThreaded() const noexcept { return m_isThreaded; } // are threads active
    bool isInputThreaded() const noexcept // is network input handled by thread
    {
        return m_isThreaded;
    }

    bool isOutputThreaded() const noexcept // is network output handled by thread
    {
        return m_isThreaded;
    }

private:
    void createNetworkThreads(size_t count);	// create n threads to handle client i/o
    CNetworkThread* selectBestThread();		// select the most suitable thread for handling a new client
    void assignNetworkState(CNetState* state);	// assign a state to a thread
    CNetState* findFreeSlot(int start = -1);	// find an unused slot for new client

    friend class ClientIterator;
    friend class NetworkThreadStateIterator;
    friend class CNetworkThread;
    friend class CNetworkInput;
    friend class CNetworkOutput;
};

extern CNetworkManager g_NetworkManager;

#endif // _INC_CNETWORKMANAGER_H
