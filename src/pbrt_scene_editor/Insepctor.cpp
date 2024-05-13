//
// Created by 王泽远 on 2024/1/2.
//
#include "Inspector.hpp"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

Inspector::Inspector()
{

}

void Inspector::constructFrame()
{
    if (!is_open) {
        return;
    }

    if (_currentInspects.empty())
    {
        ImGui::Begin("Inspector - Nothing", &is_open);
        ImGui::End();
        return;
    }

    if (_currentInspects.size() == 1)
    {
        ImGui::Begin(("Inspector - " + _currentInspects[0]->InspectedName()).c_str(), &is_open);
        _currentInspects[0]->show();
        ImGui::End();
        return;
    }
    
    ImGui::Begin("Inspector", &is_open);
    for (auto& inspectable : _currentInspects)
    {
        if (ImGui::TreeNodeEx(inspectable->InspectedName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            inspectable->show();
            ImGui::TreePop();
        }
        ImGui::Separator();
    }
    ImGui::End();
    return;
}

void Inspector::init()
{

}

Inspector::~Inspector()
{

}

std::vector<Inspectable*> Inspector::_currentInspects = std::vector<Inspectable*>{};