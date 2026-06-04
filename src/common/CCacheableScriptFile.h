/**
* @file CCacheableScriptFile.h
*
*/

#ifndef _INC_CACHEABLESCRIPTFILE_H
#define _INC_CACHEABLESCRIPTFILE_H

#include <string>
#include <vector>
#include "sphere_library/CSFileText.h"


class CCacheableScriptFile : public CSFileText
{
public:
	CCacheableScriptFile();
	~CCacheableScriptFile() override;

    CCacheableScriptFile(const CCacheableScriptFile& copy) = delete;
    CCacheableScriptFile& operator=(const CCacheableScriptFile& other) = delete;

protected:
    bool _Open(lpctstr ptcFilename = nullptr, uint uiModeFlags = OF_READ|OF_SHARE_DENY_NONE) override;
public:
    bool Open(lpctstr ptcFilename = nullptr, uint uiModeFlags = OF_READ|OF_SHARE_DENY_NONE) override;
protected:
    void _Close() override;
public:
    void Close() override;
    bool _IsFileOpen() const override;
    bool IsFileOpen() const override;
protected:
            int _Seek(int iOffset = 0, int iOrigin = SEEK_SET) override;
public:
    int Seek(int iOffset = 0, int iOrigin = SEEK_SET) override;

protected:
    bool _IsEOF() const override;
public:
    bool IsEOF() const override;
protected:
    int _GetPosition() const override;
public:
    int GetPosition() const override;

protected:
    tchar * _ReadString(tchar *pBuffer, int sizemax) override;
public:
    tchar * ReadString(tchar *pBuffer, int sizemax) override;

protected:
    void _dupeFrom(const CCacheableScriptFile *other);
    void dupeFrom(CCacheableScriptFile *other);

    bool _HasCache() const;
public:
    bool HasCache() const;

	bool _fClosed;
	bool _fRealFile;
	int _iCurrentLine;

protected:
	std::vector<std::string>* _fileContent; // It's better to have a pointer so that CResourceLock can access to this

private:    bool _useDefaultFile() const;
//public:     bool useDefaultFile() const;
};

#endif // _INC_CACHEABLESCRIPTFILE_H
