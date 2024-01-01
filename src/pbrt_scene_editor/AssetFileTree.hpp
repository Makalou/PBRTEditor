#pragma once

#include "editorComponent.hpp"
#include "AssetLoader.hpp"

struct AssetFileTree : EditorComponentGUI
{
	void constructFrame() override;
	void init();
	~AssetFileTree() override;

	AssetLoader assetLoader;
};

