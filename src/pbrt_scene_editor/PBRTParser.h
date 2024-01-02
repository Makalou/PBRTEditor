#pragma once

#include <string>
#include "LockFreeCircleQueue.hpp"
#include <vector>

#include "MappedFile.hpp"

#include <filesystem>

struct AssetLoader;
struct PBRTScene;

struct Token
{
	char* str;
	int pos;
	int len;
};


struct PBRTParser
{
	enum class ParseResult
	{
		SUCESS,
		NO_FILE,
		INCORRECT_FORMAT
	};

	PBRTParser::ParseResult parse(PBRTScene& targetScene, const std::filesystem::path& path, const AssetLoader& assetLoader);

	bool g_use_mmap = true;

private:
	LockFreeCircleQueue<Token> token_queue{ 10000 };
	void nextToken(const char* text, int text_len, int* seek, int* tok_loc, int* tok_len);
	void tokenize(const std::filesystem::path& path, LockFreeCircleQueue<Token>& token_queue);
	void tokenizeMMAP(const std::filesystem::path& path, LockFreeCircleQueue<Token>& token_queue);
	void parseToken(PBRTScene& targetScene, LockFreeCircleQueue<Token>& token_queue, const AssetLoader& assetLoader);
	std::vector<MappedFile> openedMappedFile;
};