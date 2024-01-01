#pragma once

#include <string>

struct AssetLoader;

struct PBRTParser
{
	enum class ParseResult
	{
		SUCESS,
		NO_FILE,
		INCORRECT_FORMAT
	};

	PBRTParser::ParseResult parse(const std::string& path, const AssetLoader& assetLoader);
};