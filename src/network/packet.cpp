
#include "../common/CLog.h"
#include "../game/clients/CClient.h"
#include "CNetState.h"
#include "CNetworkThread.h"
#include "net_datatypes.h"
#include "packet.h"


#define PACKET_BUFFERDEFAULT 4
#define PACKET_BUFFERGROWTH 4

// on windows we can use the win32 api for converting between unicode<->ascii,
// otherwise we need to convert with our own functions (gcc uses utf32 instead
// of utf16)
// win32 api seems to fail to convert a lot of characters properly, so it is
// better to leave this #define disabled.
#ifdef _WIN32
//#define USE_UNICODE_LIB
#else
#undef USE_UNICODE_LIB
#endif

//
// Packet logging
//
#if defined(_PACKETDUMP) || defined(_DUMPSUPPORT)

void xRecordPacketData(const CClient* client, const byte* data, const uint length, const lpctstr heading)
{
#ifdef _DUMPSUPPORT
    if (client->GetAccount() != nullptr && strnicmp(client->GetAccount()->GetName(), (lpctstr)g_Cfg.m_sDumpAccPackets, strlen(client->GetAccount()->GetName())))
        return;
#else
    if (!(g_Cfg.m_iDebugFlags & DEBUGF_PACKETS))
        return;
#endif

    const Packet packet(data, length);
    xRecordPacket(client, &packet, heading);
}

void xRecordPacket(const CClient* client, const Packet * packet, const lpctstr heading)
{
#ifdef _DUMPSUPPORT
    if (client->GetAccount() != nullptr && strnicmp(client->GetAccount()->GetName(), (lpctstr)g_Cfg.m_sDumpAccPackets, strlen(client->GetAccount()->GetName())))
        return;
#else
    if (!(g_Cfg.m_iDebugFlags & DEBUGF_PACKETS))
        return;
#endif

    TemporaryString tsDump;
    packet->dump(tsDump);

#ifdef _DEBUG
    // write to console
    g_Log.EventDebug("%x:%s %s\n", client->GetSocketID(), heading, static_cast<lpctstr>(tsDump));
#endif

    // build file name
    tchar fname[64];
	if (client->GetAccount())
	{
		snprintf(fname, sizeof(fname), "packets_%s.log", client->GetAccount()->GetName());
	}
    else
    {
		snprintf(fname, sizeof(fname), "packets_(%s).log", client->GetPeerStr());
    }

    const CSString sFullFileName = CSFile::GetMergedFileName(g_Log.GetLogDir(), fname);

    // write to file
    if (CSFileText out; out.Open(sFullFileName, OF_READWRITE | OF_TEXT))
    {
        out.Printf("%s %s\n\n", heading, static_cast<lpctstr>(tsDump));
        out.Close();
    }
}

#endif	//defined(_PACKETDUMP) || defined(_DUMPSUPPORT)


Packet::Packet(const uint size) : m_buffer(nullptr)
{
	m_expectedLength = size;
	clear();
	resize(size > 0 ? size : PACKET_BUFFERDEFAULT);
}

Packet::Packet(const Packet& other) : m_buffer(nullptr)
{
	clear();
	copy(other);
}

Packet::Packet(const byte* data, const uint size) : m_buffer(nullptr)
{
	clear();
	m_expectedLength = 0;
	resize(size);
    ASSERT(m_buffer);
	memcpy(m_buffer, data, size);
}

Packet::~Packet()
{
	clear();
}

bool Packet::isValid() const
{
	return m_buffer != nullptr && m_length > 0;
}

uint Packet::getLength() const
{
	return m_length;
}

uint Packet::getPosition() const
{
	return m_position;
}

byte* Packet::getData() const
{
	return m_buffer;
}

byte* Packet::getRemainingData() const
{
	if (m_position >= m_length)
		return nullptr;
	return &(m_buffer[m_position]);
}

uint Packet::getRemainingLength() const
{
	return m_length - m_position;
}

void Packet::clear()
{
	if (m_buffer != nullptr)
	{
		delete[] m_buffer;
		m_buffer = nullptr;
	}

	m_bufferSize = 0;
	m_position = 0;
}

void Packet::copy(const Packet& other)
{
	resize(other.getLength());
	m_expectedLength = other.m_expectedLength;
	memcpy(m_buffer, other.getData(), other.getLength());
	seek();
}

void Packet::expand(uint size)
{
	if (size < PACKET_BUFFERGROWTH)
		size = PACKET_BUFFERGROWTH;

    const uint oldPosition = m_position;
	resize(maximum(m_bufferSize, m_position) + size);
	m_position = oldPosition;
}

void Packet::resize(const uint newsize)
{
	ASSERT(newsize > 0);
	if ( newsize > m_bufferSize )		// increase buffer, copying the contents
	{
        const auto buffer = new byte[newsize];
		if (m_buffer != nullptr)
		{
			memcpy(buffer, m_buffer, m_bufferSize);
			delete[] m_buffer;
		}

		m_buffer = buffer;
		m_bufferSize = newsize;
		m_length = m_bufferSize;
	}
	else							// just change the length
		m_length = newsize;

	seek();
}

void Packet::seek(const uint pos)
{
	ASSERT(pos <= m_length);
	m_position = pos;
}

void Packet::skip(const int count)
{
	// ensure we can't go lower than 0
    if (count < 0 && static_cast<uint>(abs(count)) > m_position)
    {
		m_position = 0;
        return;
    }

    ASSERT(static_cast<int64>(m_position) + count < UINT32_MAX);
	m_position = static_cast<uint>(static_cast<int64>(m_position) + count);
}

byte &Packet::operator[](const uint index)
{
	ASSERT(index <= m_length);
	return m_buffer[index];
}

const byte &Packet::operator[](const uint index) const
{
	ASSERT(index <= m_length);
	return m_buffer[index];
}

void Packet::writeBool(const bool value)
{
	if ((m_position + sizeof(byte)) > m_bufferSize)
		expand(sizeof(byte));

	ASSERT(m_position + sizeof(byte) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value ? 1 : 0);
}

void Packet::writeCharASCII(const char value)
{
	if ((m_position + sizeof(char)) > m_bufferSize)
		expand(sizeof(char));

	ASSERT(m_position + sizeof(char) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value);
}

void Packet::writeCharUTF16(const wchar value)
{
	if ((m_position + sizeof(wchar)) > m_bufferSize)
		expand(sizeof(wchar));

	ASSERT(m_position + sizeof(wchar) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value);
	m_buffer[m_position++] = static_cast<byte>(value >> 8);
}

void Packet::writeCharNETUTF16(const wchar value)
{
	if ((m_position + sizeof(wchar)) > m_bufferSize)
		expand(sizeof(wchar));

	ASSERT(m_position + sizeof(wchar) <= m_bufferSize);
	// Big endian
	m_buffer[m_position++] = static_cast<byte>(value >> 8);
	m_buffer[m_position++] = static_cast<byte>(value);
}

void Packet::writeByte(const byte value)
{
	if ((m_position + sizeof(byte)) > m_bufferSize)
		expand(sizeof(byte));

	ASSERT((m_position + sizeof(byte)) <= m_bufferSize);
	m_buffer[m_position++] = value;
}

void Packet::writeData(const byte* buffer, const uint size)
{
	if ((m_position + (sizeof(byte) * size)) > m_bufferSize)
		expand((sizeof(byte) * size));

	ASSERT((m_position + (sizeof(byte) * size)) <= m_bufferSize);
	memcpy(&m_buffer[m_position], buffer, sizeof(byte) * size);
	m_position += size;
}

void Packet::writeInt16(const word value)
{
	if ((m_position + sizeof(word)) > m_bufferSize)
		expand(sizeof(word));

	ASSERT(m_position + sizeof(word) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value >> 8);
	m_buffer[m_position++] = static_cast<byte>(value);
}

void Packet::writeInt32(const dword value)
{
	if ((m_position + sizeof(dword)) > m_bufferSize)
		expand(sizeof(dword));

	ASSERT(m_position + sizeof(dword) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value >> 24);
	m_buffer[m_position++] = static_cast<byte>(value >> 16);
	m_buffer[m_position++] = static_cast<byte>(value >> 8);
	m_buffer[m_position++] = static_cast<byte>(value);
}

void Packet::writeInt64(const int64 value)
{
	if ((m_position + sizeof(int64)) > m_bufferSize)
		expand(sizeof(int64));

	ASSERT(m_position + sizeof(int64) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(value >> 56);
	m_buffer[m_position++] = static_cast<byte>(value >> 48);
	m_buffer[m_position++] = static_cast<byte>(value >> 40);
	m_buffer[m_position++] = static_cast<byte>(value >> 32);
	m_buffer[m_position++] = static_cast<byte>(value >> 24);
	m_buffer[m_position++] = static_cast<byte>(value >> 16);
	m_buffer[m_position++] = static_cast<byte>(value >> 8);
	m_buffer[m_position++] = static_cast<byte>(value);
}

void Packet::writeInt64(const dword hi, const dword lo)
{
	if ((m_position + sizeof(int64)) > m_bufferSize)
		expand(sizeof(int64));

	ASSERT(m_position + sizeof(int64) <= m_bufferSize);
	m_buffer[m_position++] = static_cast<byte>(hi);
	m_buffer[m_position++] = static_cast<byte>(hi >> 8);
	m_buffer[m_position++] = static_cast<byte>(hi >> 16);
	m_buffer[m_position++] = static_cast<byte>(hi >> 24);
	m_buffer[m_position++] = static_cast<byte>(lo);
	m_buffer[m_position++] = static_cast<byte>(lo >> 8);
	m_buffer[m_position++] = static_cast<byte>(lo >> 16);
	m_buffer[m_position++] = static_cast<byte>(lo >> 24);
}

void Packet::writeStringASCII(const char* value, const bool terminate)
{
	while ((value != nullptr) && *value)
	{
		writeCharASCII(*value);
		++value;
	}

	if (terminate)
		writeCharASCII('\0');
}

void Packet::writeStringFixedASCII(const char* value, const uint size, const bool terminate)
{
	if (size <= 0)
		return;

	uint valueLength = (value != nullptr) ? static_cast<uint>(strlen(value)) : 0;
	if (terminate && valueLength >= size)
		valueLength = size - 1;

	for (uint i = 0; i < size; ++i)
	{
		if (i >= valueLength)
			writeCharASCII('\0');
		else
			writeCharASCII(value[i]);
	}
}

void Packet::writeStringASCII(const wchar* value, const bool terminate)
{
#ifdef USE_UNICODE_LIB

	char* buffer = new char[MB_CUR_MAX];
	while (value != nullptr && *value)
	{
		int len = wctomb(buffer, *value);
		for (int i = 0; i < len; i++)
			writeCharASCII(buffer[i]);

		value++;
	}
	delete[] buffer;

	if (terminate)
		writeCharASCII('\0');
#else

	ASSERT(value != nullptr);
	char* buffer = Str_GetTemp();

	// need to flip byte order to convert UNICODE to ASCII
	{
		uint i;
		for (i = 0; value[i]; ++i)
			reinterpret_cast<wchar *>(buffer)[i] = reinterpret_cast<const nachar*>(value)[i];
		reinterpret_cast<wchar *>(buffer)[i] = '\0';
	}

	CvtNETUTF16ToSystem(buffer, THREAD_STRING_LENGTH, reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH);

	writeStringASCII(buffer, terminate);
#endif
}

void Packet::writeStringFixedASCII(const wchar* value, const uint size, const bool terminate)
{
#ifdef USE_UNICODE_LIB
	if (size <= 0)
		return;

	char* buffer = new char[MB_CUR_MAX];
	uint valueLength = value != nullptr ? wcslen(value) : 0;
	if (terminate && valueLength >= size)
		valueLength = size - 1;

	for (uint l = 0; l < size; ++l)
	{
		if (l >= valueLength)
			writeCharASCII('\0');
		else
		{
			int len = wctomb(buffer, value[l]);
			for (int i = 0; i < len; i++)
				writeCharASCII(buffer[i]);
		}
	}

	delete[] buffer;
#else

	ASSERT(value != nullptr);
	char* buffer = Str_GetTemp();

	// need to flip byte order to convert UNICODE to ASCII
	{
		uint i;
		for (i = 0; value[i] != '\0'; ++i)
			reinterpret_cast<wchar *>(buffer)[i] = reinterpret_cast<const nachar*>(value)[i];
		reinterpret_cast<wchar *>(buffer)[i] = '\0';
	}

	CvtNETUTF16ToSystem(buffer, THREAD_STRING_LENGTH, reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH);

	writeStringFixedASCII(buffer, size, terminate);
#endif
}

void Packet::writeStringUTF16(const char* value, const bool terminate)
{
#ifdef USE_UNICODE_LIB

	wchar c;
	while (value != nullptr && *value)
	{
		mbtowc(&c, value, MB_CUR_MAX);
		writeCharUTF16(c);
		value++;
	}

	if (terminate)
		writeCharUTF16('\0');
#else

	ASSERT(value != nullptr);

    const auto buffer = reinterpret_cast<wchar *>(Str_GetTemp());
	CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH / sizeof(wchar), value, static_cast<int>(strlen(value)));

	writeStringNETUTF16(buffer, terminate);
#endif
}

void Packet::writeStringFixedUTF16(const char* value, const uint size, const bool terminate)
{
#ifdef USE_UNICODE_LIB
	if (size <= 0)
		return;

	wchar c;
	uint valueLength = (value != nullptr) ? strlen(value) : 0;
	if (terminate && valueLength >= size)
		valueLength = size - 1;

	for (uint i = 0; i < size; ++i)
	{
		if (i >= valueLength)
			writeCharUTF16('\0');
		else
		{
			mbtowc(&c, &value[i], MB_CUR_MAX);
			writeCharUTF16(c);
		}
	}
#else

	ASSERT(value != nullptr);

    const auto buffer = reinterpret_cast<wchar *>(Str_GetTemp());
	CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH / sizeof(wchar), value, static_cast<int>(strlen(value)));

	writeStringFixedNETUTF16(buffer, size, terminate);
#endif
}

void Packet::writeStringUTF16(const wchar* value, const bool terminate)
{
	while ((value != nullptr) && *value)
	{
		writeCharUTF16(*value);
		++value;
	}

	if (terminate)
		writeCharUTF16('\0');
}

void Packet::writeStringFixedUTF16(const wchar* value, uint size, const bool terminate)
{
#ifdef USE_UNICODE_LIB
	if (size <= 0)
		return;

	uint valueLength = value != nullptr ? wcslen(value) : 0;
	if (terminate && valueLength >= size)
		valueLength = size - 1;

	for (uint i = 0; i < size; ++i)
	{
		if (i >= valueLength)
			writeCharUTF16('\0');
		else
			writeCharUTF16(value[i]);
	}
#else

	ASSERT(value != nullptr);

	if (size <= 0)
		return;
    if (terminate)
        --size;

    bool zero = false;
	for (uint i = 0; i < size; ++i)
	{
		if (zero == false)
		{
			if (value[i] == '\0')
				zero = true;

			writeCharUTF16(value[i]);
		}
		else
			writeCharUTF16('\0');
	}

	if (terminate)
		writeCharUTF16('\0');
#endif
}

void Packet::writeStringNETUTF16(const char* value, const bool terminate)
{
#ifdef USE_UNICODE_LIB

	wchar c;
	while ((value != nullptr) && *value)
	{
		mbtowc(&c, value, MB_CUR_MAX);
		writeCharNETUTF16(c);
		++value;
	}

	if (terminate)
		writeCharNETUTF16('\0');
#else

	ASSERT(value != nullptr);

    const auto buffer = reinterpret_cast<wchar *>(Str_GetTemp());
	CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH / sizeof(wchar), value, static_cast<int>(strlen(value)));

	writeStringUTF16(buffer, terminate);
#endif
}

void Packet::writeStringFixedNETUTF16(const char* value, const uint size, const bool terminate)
{
#ifdef USE_UNICODE_LIB
	if (size <= 0)
		return;

	wchar c;
	uint valueLength = (value != nullptr) ? strlen(value) : 0;
	if (terminate && valueLength >= size)
		valueLength = size - 1;

	for (uint i = 0; i < size; ++i)
	{
		if (i >= valueLength)
			writeCharNETUTF16('\0');
		else
		{
			mbtowc(&c, &value[i], MB_CUR_MAX);
			writeCharNETUTF16(c);
		}
	}
#else

	ASSERT(value != nullptr);

    auto *const buffer = reinterpret_cast<wchar *>(Str_GetTemp());
	CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), THREAD_STRING_LENGTH / sizeof(wchar), value, static_cast<int>(strlen(value)));

	writeStringFixedUTF16(buffer, size, terminate);
#endif
}

void Packet::writeStringNETUTF16(const wchar* value, const bool terminate)
{
	while ((value != nullptr) && *value)
	{
		writeCharNETUTF16(*value);
		++value;
	}

	if (terminate)
		writeCharNETUTF16('\0');
}

void Packet::writeStringFixedNETUTF16(const wchar* value, uint size, const bool terminate)
{
#ifdef USE_UNICODE_LIB
	if (size <= 0)
		return;

	uint valueLength = (value != nullptr) ? wcslen(value) : 0;
	if (terminate && (valueLength >= size))
		valueLength = size - 1;

	for (uint i = 0; i < size; ++i)
	{
		if (i >= valueLength)
			writeCharNETUTF16('\0');
		else
			writeCharNETUTF16(value[i]);
	}
#else

	ASSERT(value != nullptr);

	if (size <= 0)
		return;
    if (terminate)
        --size;

    bool zero = false;
	for (uint i = 0; i < size; ++i)
	{
		if (zero == false)
		{
			if (value[i] == '\0')
				zero = true;

			writeCharNETUTF16(value[i]);
		}
		else
			writeCharNETUTF16('\0');
	}

	if (terminate)
		writeCharNETUTF16('\0');
#endif
}

void Packet::fill()
{
	while (m_position < m_bufferSize)
		writeByte(0);
}

uint Packet::sync()
{
	if (m_length < m_position)
		m_length = m_position;

	return m_length;
}

void Packet::trim()
{
	if (m_length > m_position)
		m_length = m_position;
}

bool Packet::readBool()
{
	if ((m_position + sizeof(byte)) > m_length)
		return false;

	return (m_buffer[m_position++] != 0);
}

char Packet::readCharASCII()
{
	if ((m_position + sizeof(char)) > m_length)
		return '\0';

	return m_buffer[m_position++];
}

wchar Packet::readCharUTF16()
{
	if ((m_position + sizeof(wchar)) > m_length)
		return '\0';

    const wchar wc = ((m_buffer[m_position + 1] << 8) |
			   (m_buffer[m_position]));

	m_position += 2;
	return wc;
}

wchar Packet::readCharNETUTF16()
{
	if ((m_position + sizeof(wchar)) > m_length)
		return '\0';

    const wchar wc = ((m_buffer[m_position] << 8) |
			   (m_buffer[m_position + 1]));

	m_position += 2;
	return wc;
}

byte Packet::readByte()
{
	if ((m_position + sizeof(byte)) > m_length)
		return 0;

	return m_buffer[m_position++];
}

word Packet::readInt16()
{
	if ((m_position + sizeof(word)) > m_length)
		return 0;

    const word w = (static_cast<word>(m_buffer[m_position]) << 8u) | static_cast<word>(m_buffer[m_position + 1u]);

	m_position += 2;
	return w;
}

dword Packet::readInt32()
{
	if ((m_position + sizeof(dword)) > m_length)
		return 0;

    const dword dw = (static_cast<dword>(m_buffer[m_position]) << 24u) | (static_cast<dword>(m_buffer[m_position + 1u]) << 16u) |
               (static_cast<dword>(m_buffer[m_position + 2u]) << 8u) | static_cast<dword>(m_buffer[m_position + 3u]);

	m_position += 4;
	return dw;
}

int64 Packet::readInt64()
{
	if ((m_position + sizeof(int64)) > m_length)
		return 0;

    const dword dwHigh = readInt32();
    const dword dwLow = readInt32();
    const int64 qw = (static_cast<int64>(dwHigh) << 32) + dwLow;
	return qw;
}

void Packet::readStringASCII(char* buffer, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

	uint i;
	for (i = 0; i < length; ++i)
		buffer[i] = readCharASCII();

	// ensure text is null-terminated
	if (includeNull)
		buffer[i-1] = '\0';
	else
		buffer[i] = '\0';
}

void Packet::readStringASCII(wchar* buffer, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

#ifdef USE_UNICODE_LIB

	char* bufferReal = new char[(size_t)length + 1]();
	readStringASCII(bufferReal, length, includeNull);
#   ifdef MSVC_RUNTIME
    size_t aux;
    mbstowcs_s(&aux, buffer, length + 1, bufferReal, length);
#   else
    mbstowcs(buffer, bufferReal, length);
#   endif
	delete[] bufferReal;
#else

    const auto bufferReal = new char[static_cast<size_t>(length) + 1]();
	readStringASCII(bufferReal, length, includeNull);
	CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), static_cast<int>(length), bufferReal, static_cast<int>(length));
	delete[] bufferReal;

	// need to flip byte order to convert NETUTF16 to UTF16 UNICODE
	{
		uint i;
		for (i = 0; buffer[i]; ++i)
			buffer[i] = reinterpret_cast<nachar*>(buffer)[i];
		buffer[i] = '\0';
	}
#endif
}

void Packet::readStringUTF16(wchar* buffer, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

	uint i;
	for (i = 0; i < length; ++i)
		buffer[i] = readCharUTF16();

	// ensure text is null-terminated
	if (includeNull)
		buffer[i-1] = '\0';
	else
		buffer[i] = '\0';
}

void Packet::readStringUTF16(char* buffer, const uint bufferSize, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

#ifdef USE_UNICODE_LIB

	wchar* bufferReal = new wchar[length + 1];
	readStringUTF16(bufferReal, length, includeNull);
	wcstombs(buffer, bufferReal, bufferSize);
	delete[] bufferReal;
#else

    const auto bufferReal = new wchar[static_cast<size_t>(length) + 1];
	readStringNETUTF16(bufferReal, length, includeNull);
	CvtNETUTF16ToSystem(buffer, static_cast<int>(bufferSize), reinterpret_cast<nachar*>(bufferReal), static_cast<int>(length) + 1);
	delete[] bufferReal;
#endif
}

void Packet::readStringNETUTF16(wchar* buffer, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

	uint i;
	for (i = 0; i < length; ++i)
		buffer[i] = readCharNETUTF16();

	// ensure text is null-terminated
	if (includeNull)
		buffer[i-1] = '\0';
	else
		buffer[i] = '\0';
}

void Packet::readStringNETUTF16(char* buffer, const uint bufferSize, const uint length, const bool includeNull)
{
	ASSERT(buffer != nullptr);

	if (length < 1)
	{
		buffer[0] = '\0';
		return;
	}

#ifdef USE_UNICODE_LIB

	wchar* bufferReal = new wchar[(size_t)length + 1];
	readStringNETUTF16(bufferReal, length, includeNull);
	wcstombs(buffer, bufferReal, bufferSize);
	delete[] bufferReal;
#else

    auto *const bufferReal = new wchar[static_cast<size_t>(length) + 1];
	readStringUTF16(bufferReal, length, includeNull);
	CvtNETUTF16ToSystem(buffer, static_cast<int>(bufferSize), reinterpret_cast<nachar*>(bufferReal), static_cast<int>(length) + 1);
	delete[] bufferReal;
#endif
}

uint Packet::readStringNullASCII(char* buffer, const uint maxlength)
{
	ASSERT(buffer != nullptr);

	uint i;
	for (i = 0; i < maxlength; i++)
	{
		buffer[i] = readCharASCII();
		if (buffer[i] == '\0')
			return i;
	}

	// ensure text is null-terminated
	buffer[i] = '\0';
	return i;
}

uint Packet::readStringNullASCII(wchar* buffer, const uint maxlength)
{
	ASSERT(buffer != nullptr);

#ifdef USE_UNICODE_LIB

	char* bufferReal = new char[(size_t)maxlength + 1];
	readStringNullASCII(bufferReal, maxlength);
	int length = mbstowcs(buffer, bufferReal, maxlength + 1);
	delete[] bufferReal;
#else

    auto *const bufferReal = new char[static_cast<size_t>(maxlength) + 1];
	readStringNullASCII(bufferReal, maxlength);
    const int length = CvtSystemToNETUTF16(reinterpret_cast<nachar*>(buffer), static_cast<int>(maxlength), bufferReal, static_cast<int>(maxlength) + 1);
	delete[] bufferReal;

	// need to flip byte order to convert NETUTF16 to UTF16 UNICODE
	{
		uint i;
		for (i = 0; buffer[i]; ++i)
			buffer[i] = reinterpret_cast<nachar*>(buffer)[i];
		buffer[i] = '\0';
	}
#endif

	if (length < 0)
		return 0;
	return length;
}

uint Packet::readStringNullUTF16(wchar* buffer, const uint maxlength)
{
	ASSERT(buffer != nullptr);

	uint i;
	for (i = 0; i < maxlength; ++i)
	{
		buffer[i] = readCharUTF16();
		if (buffer[i] == '\0')
			return i;
	}

	// ensure text is null-terminated
	buffer[i] = '\0';
	return i;
}

uint Packet::readStringNullUTF16(char* buffer, const uint bufferSize, const uint maxlength)
{
	ASSERT(buffer != nullptr);

#ifdef USE_UNICODE_LIB

	wchar* bufferReal = new wchar[(size_t)maxlength + 1];
	readStringNullUTF16(bufferReal, maxlength);
	int length = wcstombs(buffer, bufferReal, bufferSize);
	delete[] bufferReal;
#else

    auto *bufferReal = new wchar[static_cast<size_t>(maxlength) + 1];
	readStringNullNETUTF16(bufferReal, maxlength);
    const int length = CvtNETUTF16ToSystem(buffer, static_cast<int>(bufferSize), reinterpret_cast<nachar*>(bufferReal), static_cast<int>(maxlength) + 1);
	delete[] bufferReal;
#endif

	if (length < 0)
		return 0;
	return length;
}

uint Packet::readStringNullNETUTF16(wchar* buffer, const uint maxlength)
{
	ASSERT(buffer != nullptr);

	uint i;
	for (i = 0; i < maxlength; i++)
	{
		buffer[i] = readCharNETUTF16();
		if (buffer[i] == '\0')
			return i;
	}

	// ensure text is null-terminated
	buffer[i] = '\0';
	return i;
}

uint Packet::readStringNullNETUTF16(char* buffer, const uint bufferSize, const uint maxlength)
{
	ASSERT(buffer != nullptr);

#ifdef USE_UNICODE_LIB

	wchar* bufferReal = new wchar[(size_t)maxlength + 1];
	readStringNullNETUTF16(bufferReal, maxlength);
	int length = wcstombs(buffer, bufferReal, bufferSize);
	delete[] bufferReal;
#else

    auto *const bufferReal = new wchar[static_cast<size_t>(maxlength) + 1];
	readStringNullUTF16(bufferReal, maxlength);
    const int length = CvtNETUTF16ToSystem(buffer, static_cast<int>(bufferSize), reinterpret_cast<nachar*>(bufferReal), static_cast<int>(maxlength) + 1);
	delete[] bufferReal;
#endif

	if (length < 0)
		return 0;
	return length;
}

void Packet::dump(AbstractString& output) const
{
	// macro to hide password bytes in release mode
#ifndef _DEBUG
#	define PROTECT_BYTE(_b_)  if ((m_buffer[0] == XCMD_ServersReq && idx >= 32 && idx <= 61) || \
						       (m_buffer[0] == XCMD_CharListReq && idx >= 36 && idx <= 65)) _b_ = '*'
#else
#	define PROTECT_BYTE(_b_)
#endif

	TemporaryString ts;

	snprintf(ts.buffer(), ts.capacity(), "Packet len=%u id=0x%02x [%s]\n", m_length, m_buffer[0], CSTime::GetCurrentTime().Format(nullptr));
	output.append(ts);
	output.append("        0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F\n");
	output.append("       -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --\n");

	uint byteIndex = 0;
    const uint whole = m_length >> 4;
    const uint rem = m_length & 0x0f;
	uint idx = 0;

	tchar bytes[50];
	tchar chars[17];

	for (uint i = 0; i < whole; ++i, byteIndex += 16 )
	{
		memset(bytes, 0, sizeof(bytes));
		memset(chars, 0, sizeof(chars));

		for (uint j = 0; j < 16; ++j)
		{
			byte c = m_buffer[idx++];
			PROTECT_BYTE(c);

			snprintf(ts.buffer(), ts.capacity(), "%02x", static_cast<int>(c));
			Str_ConcatLimitNull(bytes, ts, sizeof(bytes));
			Str_ConcatLimitNull(bytes, (j == 7) ? "  " : " ", sizeof(bytes));

			if ((c >= 0x20) && (c <= 0x80))
			{
				ts.buffer()[0] = c;
				ts.buffer()[1] = '\0';
				Str_ConcatLimitNull(chars, ts.buffer(), sizeof(chars));
			}
			else
				Str_ConcatLimitNull(chars, ".", sizeof(chars));
		}

		snprintf(ts.buffer(), ts.capacity(), "%04x   ", byteIndex);
		output.append(ts);
		output.append(bytes);
		output.append("  ");
		output.append(chars);
		output.append("\n");
	}

	if (rem != 0)
	{
		memset(bytes, 0, sizeof(bytes));
		memset(chars, 0, sizeof(chars));

		for (uint j = 0; j < 16; ++j)
		{
			if (j < rem)
			{
                byte c = m_buffer[idx++];
				PROTECT_BYTE(c);

				snprintf(ts.buffer(), ts.capacity(), "%02x", static_cast<int>(c));
				Str_ConcatLimitNull(bytes, ts.buffer(), sizeof(bytes));
				Str_ConcatLimitNull(bytes, (j == 7) ? "  " : " ", sizeof(bytes));

				if ((c >= 0x20) && (c <= 0x80))
				{
					ts.buffer()[0] = c;
					ts.buffer()[1] = 0;
					Str_ConcatLimitNull(chars, ts.buffer(), sizeof(chars));
				}
				else
					Str_ConcatLimitNull(chars, ".", sizeof(chars));
			}
			else
				Str_ConcatLimitNull(bytes, "   ", sizeof(bytes));
		}

		snprintf(ts.buffer(), ts.capacity(), "%04x   ", byteIndex);
		output.append(ts);
		output.append(bytes);
		output.append("  ");
		output.append(chars);
		output.append("\n");
	}
#undef PROTECT_BYTE
}

uint Packet::checkLength(CNetState* client, Packet* packet)
{
	ASSERT(client != nullptr);
	ASSERT(packet != nullptr);

	uint packetLength = getExpectedLength(client, packet);

	if (packetLength <= 0)
	{
		// dynamic length
		if (packet->getLength() < 3)
			return 0;

        const uint pos = packet->getPosition();
		packet->skip(1);
		packetLength = packet->readInt16();
		packet->seek(pos);

		if ( packetLength < 3 )
			return 0;
	}

	if ((packet->getRemainingLength()) < packetLength)
		return 0;

	return packetLength;
}

uint Packet::getExpectedLength(CNetState* client, Packet* packet)
{
	UnreferencedParameter(client);
	UnreferencedParameter(packet);
	return m_expectedLength;
}

bool Packet::onReceive(CNetState* client)
{
	UnreferencedParameter(client);
	return true;
}


/***************************************************************************
 *
 *
 *	class SendPacket			Send-type packet with priority and target client
 *
 *
 ***************************************************************************/
PacketSend::PacketSend(const byte id, const uint len, const Priority priority)
	: m_priority(priority), m_target(nullptr), m_lengthPosition(0)
{
	if (len > 0)
		resize(len);

	writeByte(id);
}

PacketSend::PacketSend(const PacketSend *other)
{
	copy(*other);
	m_target = other->m_target;
	m_priority = other->m_priority;
	m_lengthPosition = other->m_lengthPosition;
	m_position = other->m_position;
}

PacketSend::~PacketSend() = default;

void PacketSend::initLength()
{
//	DEBUGNETWORK(("Packet %x starts dynamic with pos %d.\n", m_buffer[0], m_position));

	m_lengthPosition = m_position;
	writeInt16(static_cast<word>(m_lengthPosition));
}

void PacketSend::fixLength()
{
	if (m_lengthPosition > 0)
	{
//		DEBUGNETWORK(("Packet %x closes dynamic data writing %d as length to pos %d.\n", m_buffer[0], m_position, m_lengthPosition));

        const uint oldPosition = m_position;
		m_position = m_lengthPosition;
		writeInt16(static_cast<word>(oldPosition));
		m_position = oldPosition;
		m_lengthPosition = 0;
		m_length = m_position;
	}
}

PacketSend* PacketSend::clone() const
{
	return new PacketSend(this);
}

void PacketSend::send(const CClient *client, const bool appendTransaction)
{
	ADDTOCALLSTACK("PacketSend::send");

	fixLength();
	if (client != nullptr)
		target(client);

	// check target is set and can receive this packet
	if (m_target == nullptr || canSendTo(m_target) == false)
		return;

	if (sync() > NETWORK_MAXPACKETLEN)
		return;

	m_target->getParentThread()->queuePacket(this->clone(), appendTransaction);
}

void PacketSend::push(const CClient *client, const bool appendTransaction)
{
	ADDTOCALLSTACK("PacketSend::push");

	fixLength();
	if (client != nullptr)
		target(client);

	// check target is set and can receive this packet
	if (m_target == nullptr || canSendTo(m_target) == false)
	{
		delete this;
		return;
	}

	if (sync() > NETWORK_MAXPACKETLEN)
	{
		// since we are pushing this packet, we should clear the memory
		// instantly if this packet will never be sent
		DEBUGNETWORK(("Packet deleted due to exceeding maximum packet size (%u/%u).\n", m_length, NETWORK_MAXPACKETLEN));
		delete this;
		return;
	}

	m_target->getParentThread()->queuePacket(this, appendTransaction);
}

void PacketSend::target(const CClient* client)
{
	ADDTOCALLSTACK("PacketSend::target");

	m_target = nullptr;

	//	validate that the current slot is still taken by this client
	if (client != nullptr && client->GetNetState()->isInUse(client))
		m_target = client->GetNetState();
}

bool PacketSend::onSend(const CClient* client)
{
	UnreferencedParameter(client);
	return true;
}

void PacketSend::onSent(CClient* client)
{
	UnreferencedParameter(client);
}

bool PacketSend::canSendTo(const CNetState* state) const
{
	UnreferencedParameter(state);
	return true;
}

/***************************************************************************
 *
 *
 *	class SimplePacketTransaction		Interface for defining a packet transaction
 *
 *
 ***************************************************************************/

PacketTransaction::~PacketTransaction() = default;

/***************************************************************************
 *
 *
 *	class SimplePacketTransaction		Class for defining a single packet to be sent
 *
 *
 ***************************************************************************/
SimplePacketTransaction::~SimplePacketTransaction()
{
	if (m_packet != nullptr)
		delete m_packet;
}


/***************************************************************************
 *
 *
 *	class ExtendedPacketTransaction		Class for defining a set of packets to be sent together
 *
 *
 ***************************************************************************/
ExtendedPacketTransaction::~ExtendedPacketTransaction()
{
	for (auto it = m_packets.begin(), end = m_packets.end(); it != end; ++it)
		delete *it;

	m_packets.clear();
}


/***************************************************************************
 *
 *
 *	class OpenPacketTransaction		Class to automatically begin and end a transaction
 *
 *
 ***************************************************************************/
OpenPacketTransaction::OpenPacketTransaction(const CClient* client, const int priority)
{
	ASSERT(client != nullptr);

	m_client = client->GetNetState();
	if (m_client != nullptr)
		m_client->beginTransaction(priority);
}

OpenPacketTransaction::~OpenPacketTransaction()
{
	if (m_client != nullptr)
		m_client->endTransaction();
}
