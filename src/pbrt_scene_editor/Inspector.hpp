#pragma once

#include "editorComponent.hpp"
#include <string>
#include "imgui.h"

struct Inspectable
{
    //layout
    virtual void show(){

    }

    virtual std::string InspectedName() {
        return "<anonymous>";
    }
};

struct DummyInspectable : Inspectable
{
    std::string InspectedName() override{
        return "nothing";
    }
};

struct Inspector : EditorComponentGUI
{
    Inspector();
	virtual void constructFrame() override;
    void init();
    ~Inspector();

    static void inspect(Inspectable * ins)
    {
        _currentInspect = ins;
    }
    static void inspectDummy()
    {
        _currentInspect = &dummy;
    }

private:
    static Inspectable dummy;
	static Inspectable * _currentInspect;
};

static void showField(const std::string& label,bool* field)
{
    ImGui::Checkbox(label.c_str(),field);
}

static void showField(const std::string& label, float* field)
{
    ImGui::SliderFloat(label.c_str(),field,-1000.0f,1000.0f);
}

static void showField(const std::string& label, int* field)
{
    ImGui::SliderInt(label.c_str(),field,-1000.0f,1000.0f);
}

static void showFieldFloat2(const std::string& label, float* field)
{
    ImGui::SliderFloat2(label.c_str(),field,-1000.0f,1000.0f);
}

static void showFieldFloat3(const std::string& label, float* field)
{
    ImGui::SliderFloat3(label.c_str(),field,-1000.0f,1000.0f);
}

static void showFieldCombo(const std::string& label, int* current,const char* const items[], int items_count)
{
    ImGui::Combo(label.c_str(),current,items,items_count);
}

static void showField(const std::string& label,std::string* str)
{
    ImGui::Text("%s", str->c_str());
}

#define SHOW_FIELD(field_name) showField(#field_name,&field_name);

#define SHOW_FILED_FlOAT2(field_name) showFieldFloat2(#field_name,field_name);

#define SHOW_FILED_FlOAT3(field_name) showFieldFloat3(#field_name,field_name);