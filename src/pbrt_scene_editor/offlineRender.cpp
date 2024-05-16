#include "offlineRender.hpp"
#include <thread>

OfflineRenderGUI::OfflineRenderGUI()
{

}

void OfflineRenderGUI::constructFrame()
{
    if (!is_open) {
        return;
    }
    return;
}

void OfflineRenderGUI::init()
{

}

void OfflineRenderGUI::render(const std::string& pbrt_scene_path)
{
	std::thread renderingThread([this,pbrt_scene_path] {
		std::string pbrt_v4_path = "C:\\Users\\16921\\Code\\pbrt-v4\\build\\Release\\pbrt.exe";
		auto command = pbrt_v4_path + " --display-server localhost:14158 " + pbrt_scene_path;
		std::system(command.c_str());
		});
	renderingThread.detach();
}

OfflineRenderGUI::~OfflineRenderGUI()
{

}