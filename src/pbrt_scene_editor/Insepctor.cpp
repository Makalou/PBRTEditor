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

    ImGui::Begin("Inspector - nothing",&is_open);

    ImGui::End();
}

void Inspector::init()
{

}



Inspector::~Inspector()
{

}