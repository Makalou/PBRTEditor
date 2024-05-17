#pragma once

#include "editorComponent.hpp"
#include <string>
#include "imgui.h"
#include <vector>

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

    static void AddInspect(Inspectable * ins)
    {
        _currentInspects.push_back(ins);
    }

    template<typename T>
    //std::enable_if_t<std::is_base_of<Inspectable,T>::value>
    static void AddInspect(const std::vector<T*> & ins)
    {
        for(auto i : ins)
        {
            _currentInspects.push_back(i);
        }
    }

private:
    static std::vector<Inspectable*> _currentInspects;
};

static bool watchField(const std::string& label,bool* field)
{
    return ImGui::Checkbox(label.c_str(),field);
}

static bool watchField(const std::string& label, float* field)
{
    return ImGui::SliderFloat(label.c_str(),field,-1000.0f,1000.0f);
}

static bool watchField(const std::string& label, int* field)
{
    return ImGui::SliderInt(label.c_str(),field,-1000.0f,1000.0f);
}

static bool watchFieldFloat2(const std::string& label, float* field)
{
    return ImGui::SliderFloat2(label.c_str(),field,-1000.0f,1000.0f);
}

static bool watchFieldFloat3(const std::string& label, float* field, float vmin, float vmax)
{
    return ImGui::SliderFloat3(label.c_str(),field,vmin,vmax);
}

static bool watchFieldCombo(const std::string& label, int* current,const char* const items[], int items_count)
{
    return ImGui::Combo(label.c_str(),current,items,items_count);
}

static void watchField(const std::string& label,std::string* str)
{
    return ImGui::LabelText(label.c_str(),"%s", str->c_str());
}

template <typename Callback>
static void watchFieldNotify(const std::string& label,bool* field,Callback callback)
{
    if(ImGui::Checkbox(label.c_str(),field))
    {
        callback(*field);
    }
}

template <typename Callback>
static void watchFieldNotify(const std::string& label, float* field,Callback callback)
{
    if(ImGui::SliderFloat(label.c_str(),field,-1000.0f,1000.0f))
    {
        callback(*field);
    }
}

template <typename Callback>
static void watchFieldNotify(const std::string& label, int* field,Callback callback)
{
    if(ImGui::SliderInt(label.c_str(),field,-10.0f,10.0f))
    {
        callback(*field);
    }
}

template <typename Callback>
static void watchFieldFloat2Notify(const std::string& label, float* field,Callback callback)
{
    if(ImGui::SliderFloat2(label.c_str(),field,-10.0f,10.0f))
    {
        callback(field[0],field[1]);
    }
}

template <typename Callback>
static void watchFieldFloat3Notify(const std::string& label, float* field,Callback callback)
{
    if(ImGui::SliderFloat3(label.c_str(),field,-10.0f,10.0f))
    {
        callback(field[0],field[1],field[2]);
    }
}

template <typename Callback>
static void watchFieldComboNotify(const std::string& label, int* current,const char* const items[], int items_count,Callback callback)
{
    if(ImGui::Combo(label.c_str(),current,items,items_count))
    {
        callback(items[*current]);
    }
}

#define WATCH_FIELD(field_name) watchField(#field_name,&field_name)

#define WATCH_FILED_FlOAT2(field_name) watchFieldFloat2(#field_name,field_name)

#define WATCH_FILED_FlOAT3(field_name,vmin,vmax) watchFieldFloat3(#field_name,field_name,vmin, vmax)

#define WATCH_FIELD_NOTIFY(field_name,callback) watchFieldNotify(#field_name,&field_name,callback)

#define WATCH_FILED_FlOAT2_NOTIFY(field_name,callback) watchFieldFloat2Notify(#field_name,field_name,callback)

#define WATCH_FILED_FlOAT3_NOTIFY(field_name,callback) watchFieldFloat3Notify(#field_name,field_name,callback)