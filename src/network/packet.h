/**
* @file packet.h
* @brief Basic packet sending/receiving support.
*/

#ifndef _INC_PACKET_H
#define _INC_PACKET_H

#include "../common/common.h"
#include <list>

#define NETWORK_MAXPACKETS		g_Cfg._uiNetMaxPacketsPerTick	// max packets to send per tick (per queue)
#define NETWORK_MAXPACKETLEN	g_Cfg._uiNetMaxLengthPerTick	// max packet length to send per tick (per queue)


class CClient;
class Packet;

#if defined(_PACKETDUMP) || defined(_DUMPSUPPORT)
    void xRecordPacketData(const CClient* client, const byte* data, uint length, lpctstr heading);
    void xRecordPacket(const CClient* client, const Packet * packet, lpctstr heading);
#else
    #define xRecordPacketData(_client_, _data_, _length, _heading_)
    #define xRecordPacket(_client_, _packet_, _heading_)
#endif


class CNetState;
class SimplePacketTransaction;
class AbstractString;

/***************************************************************************
 *
 *
 *	class Packet				Base packet class for both sending/receiving
 *
 *
 ***************************************************************************/
class Packet
{
protected:
	byte* m_buffer;				// raw data
	uint m_bufferSize;		// size of raw data

	uint m_length;			// length of packet
	uint m_position;			// current position in packet
	uint m_expectedLength;	// expected length of this packet (0 = dynamic)

public:
	explicit Packet(uint size = 0);
	Packet(const Packet& other);
	Packet(const byte* data, uint size);
	virtual ~Packet();

    Packet& operator=(const Packet& other) = delete;

	bool isValid() const;
	uint getLength() const; // get total packet length
	uint getPosition() const; // get current position
	byte* getData() const; // get packet data
	byte* getRemainingData() const; // get packet data from current position
	uint getRemainingLength() const; // get length of data from current position
	void dump(AbstractString& output) const; // write packet data to string

	void expand(uint size = 0); // expand packet (resize whilst maintaining position)
	void resize(uint newsize); // resize packet
	void seek(uint pos = 0); // seek to position
	void skip(int count = 1); // skip count bytes

	byte &operator[](uint index);
	const byte &operator[](uint index) const;

	// write
	void writeBool(bool value); // write boolean (1 byte)
	void writeCharASCII(char value); // write ASCII character (1 byte)
	void writeCharUTF16(wchar value); // write UNICODE character (2 bytes)
	void writeCharNETUTF16(wchar value); // write UNICODE character, network order (2 bytes)
	void writeByte(byte value); // write 8-bit integer (1 byte)
	void writeInt16(word value); // write 16-bit integer (2 bytes)
	void writeInt32(dword value); // write 32-bit integer (4 bytes)
	void writeInt64(dword hi, dword lo); // write 64-bit integer (8 bytes)
	void writeInt64(int64 value); // write 64-bit integer (8 bytes)
	void writeStringASCII(const char* value, bool terminate = true); // write ascii string until null terminator found
	void writeStringASCII(const wchar* value, bool terminate = true); // write ascii string until null terminator found
	void writeStringFixedASCII(const char* value, uint size, bool terminate = false); // write fixed-length ascii string
	void writeStringFixedASCII(const wchar* value, uint size, bool terminate = false); // write fixed-length ascii string
	void writeStringUTF16(const char* value, bool terminate = true); // write unicode string until null terminator found
	void writeStringUTF16(const wchar* value, bool terminate = true); // write unicode string until null terminator found
	void writeStringFixedUTF16(const char* value, uint size, bool terminate = false); // write fixed-length unicode string
	void writeStringFixedUTF16(const wchar* value, uint size, bool terminate = false); // write fixed-length unicode string
	void writeStringNETUTF16(const char* value, bool terminate = true); // write unicode string until null terminator found, network order
	void writeStringNETUTF16(const wchar* value, bool terminate = true); // write unicode string until null terminator found, network order
	void writeStringFixedNETUTF16(const char* value, uint size, bool terminate = false); // write fixed-length unicode string, network order
	void writeStringFixedNETUTF16(const wchar* value, uint size, bool terminate = false); // write fixed-length unicode string, network order
	void writeData(const byte* buffer, uint size); // write block of data
	void fill(); // zeroes remaining buffer
	uint sync();
	void trim(); // trim packet length down to current position

	// read
	bool readBool(); // read boolean (1 byte)
	char readCharASCII(); // read ASCII character (1 byte)
	wchar readCharUTF16(); // read UNICODE character (2 bytes)
	wchar readCharNETUTF16(); // read UNICODE character, network order (2 bytes)
	byte readByte(); // read 8-bit integer (1 byte)
	word readInt16(); // read 16-bit integer (2 bytes)
	dword readInt32(); // read 32-bit integer (4 bytes)
	int64 readInt64(); // read 64-bit integer (8 bytes)
	void readStringASCII(char* buffer, uint length, bool includeNull = true); // read fixed-length ascii string
	void readStringASCII(wchar* buffer, uint length, bool includeNull = true); // read fixed-length ascii string
	void readStringUTF16(char* buffer, uint bufferSize, uint length, bool includeNull = true); // read fixed length unicode string
	void readStringUTF16(wchar* buffer, uint length, bool includeNull = true); // read fixed length unicode string
	void readStringNETUTF16(char* buffer, uint bufferSize, uint length, bool includeNull = true); // read fixed length unicode string, network order
	void readStringNETUTF16(wchar* buffer, uint length, bool includeNull = true); // read fixed length unicode string, network order
	uint readStringNullASCII(char* buffer, uint maxlength); // read ascii string until null terminator found
	uint readStringNullASCII(wchar* buffer, uint maxlength); // read ascii string until null terminator found
	uint readStringNullUTF16(char* buffer, uint bufferSize, uint maxlength); // read unicode-string until null terminator found
	uint readStringNullUTF16(wchar* buffer, uint maxlength); // read unicode-string until null terminator found
	uint readStringNullNETUTF16(char* buffer, uint bufferSize, uint maxlength); // read unicode-string until null terminator found, network order
	uint readStringNullNETUTF16(wchar* buffer, uint maxlength); // read unicode-string until null terminator found, network order

	uint checkLength(CNetState* client, Packet* packet);
	virtual uint getExpectedLength(CNetState* client, Packet* packet);
	virtual bool onReceive(CNetState* client);

protected:
	void clear();
	void copy(const Packet& other);
};


/***************************************************************************
 *
 *
 *	class SendPacket			Send-type packet with priority and target client
 *
 *
 ***************************************************************************/
class PacketSend : public Packet
{
public:
	enum Priority
	{
		PRI_IDLE,
		PRI_LOW,
		PRI_NORMAL,
		PRI_HIGH,
		PRI_HIGHEST,
		PRI_QTY,
	};

protected:
	int m_priority; // packet priority
	CNetState* m_target; // selected network target for this packet
	uint m_lengthPosition; // position of length-byte

public:
	explicit PacketSend(byte id, uint len = 0, Priority priority = PRI_NORMAL);
	PacketSend(const PacketSend* other);
    ~PacketSend() override;

    PacketSend& operator=(const PacketSend& other) = delete;

	void initLength(); // write empty length and ensure that it is remembered

	void target(const CClient* client); // sets person to send packet to

	void send(const CClient* client = nullptr, bool appendTransaction = true); // adds the packet to the send queue
	void push(const CClient* client = nullptr, bool appendTransaction = true); // moves the packet to the send queue (will not be used anywhere else)

	int getPriority() const { return m_priority; } // get packet priority
	CNetState* getTarget() const { return m_target; } // get target state

	virtual bool onSend(const CClient* client);
	virtual void onSent(CClient* client);
	virtual bool canSendTo(const CNetState* state) const;

	friend class CNetworkOutput;
	friend class SimplePacketTransaction;

protected:
	void fixLength(); // write correct packet length to it's slot
	virtual PacketSend* clone() const;
};


/***************************************************************************
 *
 *
 *	class PacketTransaction		Class for defining data to be sent
 *
 *
 ***************************************************************************/
class PacketTransaction
{
protected:
	PacketTransaction() = default;

	PacketTransaction(const PacketTransaction& copy) = delete;
	PacketTransaction& operator=(const PacketTransaction& other) = delete;

public:
	virtual ~PacketTransaction();

	virtual PacketSend* front() = 0; // get first packet in the transaction
	virtual void pop() = 0; // remove first packet from the transaction
	virtual bool empty() = 0; // check if any packets are available

	virtual CNetState* getTarget() const = 0; // get target of the transaction
	virtual int getPriority() const = 0; // get priority of the transaction
	virtual void setPriority(int priority) = 0; // set priority of the transaction
};


/***************************************************************************
 *
 *
 *	class SimplePacketTransaction		Class for defining a single packet to be sent
 *
 *
 ***************************************************************************/
class SimplePacketTransaction : public PacketTransaction
{
	PacketSend* m_packet;

public:
	explicit SimplePacketTransaction(PacketSend* packet) : m_packet(packet) { }
	~SimplePacketTransaction() override;

	SimplePacketTransaction(const SimplePacketTransaction& copy) = delete;
	SimplePacketTransaction& operator=(const SimplePacketTransaction& other) = delete;

	CNetState* getTarget() const override { return m_packet->getTarget(); }
	int getPriority() const override { return m_packet->getPriority(); }
	void setPriority(const int priority) override { m_packet->m_priority = priority; }

	PacketSend* front() override { return m_packet; }
	void pop() override { m_packet = nullptr; }
	bool empty() override { return m_packet == nullptr; }
};


/***************************************************************************
 *
 *
 *	class ExtendedPacketTransaction		Class for defining a set of packets to be sent together
 *
 *
 ***************************************************************************/
class ExtendedPacketTransaction : public PacketTransaction
{
	std::list<PacketSend*> m_packets;
	CNetState* m_target;
	int m_priority;

public:
	ExtendedPacketTransaction(CNetState* target, const int priority) : m_target(target), m_priority(priority) { }
	~ExtendedPacketTransaction() override;

private:
	ExtendedPacketTransaction(const ExtendedPacketTransaction& copy);
	ExtendedPacketTransaction& operator=(const ExtendedPacketTransaction& other);

public:
	CNetState* getTarget() const override { return m_target; }
	int getPriority() const override { return m_priority; }
	void setPriority(const int priority) override { m_priority = priority; }

    void push_back(PacketSend* packet) { m_packets.push_back(packet); }
    void emplace_back(PacketSend* packet) { m_packets.emplace_back(packet); }
    PacketSend* front() override { return m_packets.front(); }
    void pop() override { m_packets.pop_front(); }
    bool empty() override { return m_packets.empty(); }
};


/***************************************************************************
 *
 *
 *	class OpenPacketTransaction		Class to automatically begin and end a transaction
 *
 *
 ***************************************************************************/
class OpenPacketTransaction
{
	CNetState* m_client;

public:
	OpenPacketTransaction(const CClient* client, int priority);
	~OpenPacketTransaction();

private:
	OpenPacketTransaction(const OpenPacketTransaction& copy);
	OpenPacketTransaction& operator=(const OpenPacketTransaction& other);
};


#endif // _INC_PACKET_H
