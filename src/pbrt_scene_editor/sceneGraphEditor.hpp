#pragma once

#include "editorComponent.hpp"

#include <string>

#include "PBRTParser.h"

struct AssetLoader;

struct SceneGraphEditor : EditorComponentGUI
{
	
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;

	// path is the absolute path to the file
	PBRTParser::ParseResult parsePBRTSceneFile(const std::string& path, const AssetLoader& assetLoader);
	PBRTParser _parser;
};