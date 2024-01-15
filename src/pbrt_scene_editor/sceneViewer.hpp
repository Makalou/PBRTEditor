#pragma once

#include "editorComponent.hpp"
#include <memory>

struct DeviceExtended;
struct SceneGraphNode;

struct SceneViewer
{
	void init(std::shared_ptr<DeviceExtended> device);
    void setCurrentSceneGraph(SceneGraphNode* root);
	~SceneViewer();
private:
	std::shared_ptr<DeviceExtended> backendDevice;
    SceneGraphNode* currentSceneGraph;
};