#pragma once

#include "editorComponent.hpp"
#include <filesystem>

struct OfflineRenderGUI : EditorComponentGUI
{
	void init();
	void constructFrame() override;
	void render(const std::string& pbrt_scene_path);
	OfflineRenderGUI();
	~OfflineRenderGUI();
};
