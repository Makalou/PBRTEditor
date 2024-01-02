#pragma once

#include "editorComponent.hpp"

#include <string>

#include "PBRTParser.h"

#include <filesystem>

struct AssetLoader;
struct PBRTScene;

struct SceneGraphNode;

struct SceneGraphEditor : EditorComponentGUI
{
	
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;

	// path is the absolute path to the file
	PBRTParser::ParseResult parsePBRTSceneFile(const std::filesystem::path& path, const AssetLoader& assetLoader);
	PBRTParser _parser;
	std::shared_ptr<PBRTScene> _currentScene;
	SceneGraphNode* _sceneGraphRootNode;
};