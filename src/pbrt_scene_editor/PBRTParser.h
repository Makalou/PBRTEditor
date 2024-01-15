#pragma once

#include <string>
#include "LockFreeCircleQueue.hpp"
#include <vector>

#include "MappedFile.hpp"

#include <filesystem>

struct AssetManager;
struct PBRTSceneBuilder;

struct Token
{
	char* str;
	int pos;
	int len;

    auto to_string() const{
        return std::string(str + pos,len);
    }
};


struct PBRTParser
{
	enum class ParseResult
	{
		SUCESS,
		NO_FILE,
		INCORRECT_FORMAT
	};

	PBRTParser::ParseResult parse(PBRTSceneBuilder& targetScene, const std::filesystem::path& path, AssetManager& assetLoader);

	bool g_use_mmap = true;

private:
	LockFreeCircleQueue<Token> token_queue{ 10000 };
	void nextToken(const char* text, int text_len, int* seek, int* tok_loc, int* tok_len);
	void tokenize(const std::filesystem::path& path);
	void tokenizeMMAP(const std::filesystem::path& path);
	void parseToken(PBRTSceneBuilder& builder, AssetManager& assetLoader);
	std::vector<MappedFile> openedMappedFile;
};