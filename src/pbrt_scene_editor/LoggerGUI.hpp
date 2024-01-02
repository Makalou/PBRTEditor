//
// Created by 王泽远 on 2024/1/1.
//

#ifndef PBRTEDITOR_LOGGERGUI_H
#define PBRTEDITOR_LOGGERGUI_H

#include "editorComponent.hpp"
#include "imgui.h"

struct LoggerGUI : EditorComponentGUI
{
    void constructFrame() override;
    void init();
    ~LoggerGUI() override;
private:
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool                AutoScroll;  // Keep scrolling if already at the bottom.

    void clear();
    void  AddLog(const char* fmt, ...);
};

#endif //PBRTEDITOR_LOGGERGUI_H
