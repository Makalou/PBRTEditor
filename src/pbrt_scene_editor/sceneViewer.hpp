#pragma once

#include "editorComponent.hpp"
#include <memory>

struct DeviceExtended;

struct SceneViewer : EditorComponentGUI
{
	void constructFrame() override;
	void init(std::shared_ptr<DeviceExtended> device);
	~SceneViewer() override;
private:
	std::shared_ptr<DeviceExtended> backendDevice;
};