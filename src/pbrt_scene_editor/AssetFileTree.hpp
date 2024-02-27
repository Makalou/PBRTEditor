#pragma once

#include "editorComponent.hpp"
#include "AssetManager.hpp"

struct AssetFileTree : EditorComponentGUI
{
	void constructFrame() override;
	void init(DeviceExtended * device);
	~AssetFileTree() override;

	AssetManager assetManager;
};

