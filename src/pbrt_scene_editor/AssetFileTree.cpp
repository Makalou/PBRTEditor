#include "AssetFileTree.hpp"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

#include <cstdio>

void AssetFileTree::constructFrame()
{
	
}

void AssetFileTree::init(DeviceExtended * device)
{
    assetManager.setBackendDevice(device);
}

AssetFileTree::~AssetFileTree()
{

}