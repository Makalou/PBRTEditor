#pragma once

#include "editorComponent.hpp"

struct AssetFileTree : EditorComponentGUI
{
	void constructFrame() override;
	void init();
	~AssetFileTree() override;
};

