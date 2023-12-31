#pragma once

#include "editorComponent.hpp"

struct SceneGraphEditor : EditorComponentGUI
{
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;
};