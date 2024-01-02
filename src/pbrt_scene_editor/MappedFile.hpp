#pragma once 

#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#include <atomic>
#include <filesystem>
#include <exception>

struct MappedFile
{
	MappedFile(const std::filesystem::path& path)
	{
#ifdef WIN32
		fileSize = 0;
		LPCWSTR fileNameLPCTSTR = path.c_str();
		hFile = CreateFileW(fileNameLPCTSTR,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("open failed.");
		}
		fileSize = GetFileSize(hFile, NULL);
		hMapping = CreateFileMapping(hFile,NULL,PAGE_READWRITE,0,fileSize,NULL);
		if (hMapping == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("mapping failed.");
		}
		pMapped = (char*)MapViewOfFile(hMapping,FILE_MAP_ALL_ACCESS,0,0,0); //0 means the entire file
		if (pMapped == nullptr) {
			throw std::runtime_error("mapping failed.");
		}
#elif __APPLE__
		
#endif
		ref_counter = new std::atomic<int>(0);
	}
	char* raw() const {
		return pMapped;
	}

	int size() const {
		return fileSize;
	}

	~MappedFile()
	{
		if (ref_counter == nullptr) return;
		if (ref_counter->load() == 0) {
			destroy();
		}
		else {
			*ref_counter -= 1;
		}
	}

	//Too lazy to handle self-assignment case for now...
	MappedFile& operator=(MappedFile&&) = delete;
	MappedFile& operator=(const MappedFile&) = delete;
	
	MappedFile(MappedFile&& other)
	{
		this->pMapped = other.pMapped;
		this->ref_counter = other.ref_counter;
		this->fileSize = other.fileSize;

		other.pMapped = nullptr;
		other.ref_counter = nullptr;
		other.fileSize = 0;

#ifdef WIN32
		this->hFile = other.hFile;
		this->hMapping = other.hMapping;
		
		other.hFile = INVALID_HANDLE_VALUE;
		other.hMapping = INVALID_HANDLE_VALUE;
#elif __APPLE__

#endif
	}

	MappedFile(const MappedFile& other)
	{
		this->pMapped = other.pMapped;
		this->ref_counter = other.ref_counter;
		this->fileSize = other.fileSize;
#ifdef WIN32
		this->hFile = other.hFile;
		this->hMapping = other.hMapping;
#elif __APPLE__

#endif
		* ref_counter += 1;
	}

private:
	void destroy()
	{
#ifdef WIN32
		if(pMapped!=nullptr)
			UnmapViewOfFile((LPVOID)pMapped);
		if(hMapping != INVALID_HANDLE_VALUE)
			CloseHandle(hMapping);
		if(hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);
#elif __APPLE__

#endif
		delete ref_counter;
	}

	char* pMapped;
	std::atomic<int>* ref_counter;
	int fileSize;
#ifdef WIN32
	HANDLE hFile;
	HANDLE hMapping;
#elif __APPLE__
	int fd;
	struct stat file_info{};
#endif
};
