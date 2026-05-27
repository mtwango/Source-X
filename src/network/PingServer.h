/**
* @file PingServer.h
*
*/

#ifndef _INC_PINGSERVER_H
#define _INC_PINGSERVER_H

#include "../sphere/threads.h"
#include "CSocket.h"


#define PINGSERVER_PORT		12000	// listen on this port for client pings (clients normally uses 12000)
#define PINGSERVER_BUFFER	64		// number of bytes to receive from clients (client normally sends 40)


class PingServer : public AbstractSphereThread
{
private:
	CSocket m_socket;

public:
	PingServer(void);
	~PingServer(void) override;

    PingServer(const PingServer& copy) = delete;
    PingServer& operator=(const PingServer& other) = delete;

	void onStart() override;
	void tick() override;
	bool shouldExit() noexcept override;
	void waitForClose() override;
};


#endif // _INC_PINGSERVER_H
