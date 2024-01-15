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

    ImGui::Begin(("Inspect - " +_currentInspect->InspectedName()).c_str(),&is_open);
    _currentInspect->show();
    ImGui::End();

}

void Inspector::init()
{

}

Inspector::~Inspector()
{

}

Inspectable Inspector::dummy = DummyInspectable{};
Inspectable* Inspector::_currentInspect = &dummy;