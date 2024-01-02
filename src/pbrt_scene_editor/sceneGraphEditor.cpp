#include "sceneGraphEditor.hpp"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

#include <cstdio>
#include <vector>

#include "AssetLoader.hpp"

#include "scene.h"

#include "Inspector.hpp"

struct SceneGraphNode : Inspectable
{
    std::string name;
    std::vector<SceneGraphNode*> children;
    SceneGraphNode* parent;

    int intermediate_val;
    int accumulated_val;

    void modify(int new_val){
        intermediate_val = new_val;
        reaccumulate(parent->accumulated_val);
    }

    void reaccumulate(int val)
    {
        accumulated_val = accumulate(val);
        for (auto* child : children) { //todo maybe paralleized by omp task ... 
            child->reaccumulate(accumulated_val);
        }
    }

    int accumulate(int val)
    {

    }
};

void rightClickMenu(bool* selections, int count)
{
    if (ImGui::BeginPopupContextItem())
    {
        std::vector<int> selected_idices;
        for (int i = 0; i < count; i++)
        {
            if (selections[i]) {
                ImGui::Text("This a popup for \"%d\"!", i);
                selected_idices.push_back(i);
            }
        }

        if (selected_idices.size() == 1)
        {
            if (ImGui::MenuItem("Copy"))
            {

            }
            if (ImGui::MenuItem("Paste"))
            {

            }
            if (ImGui::MenuItem("Delete"))
            {

            }   
        }
        else {
            if (ImGui::MenuItem("Copy All"))
            {

            }
            if (ImGui::MenuItem("Paste All"))
            {

            }
            if (ImGui::MenuItem("Delete All"))
            {

            }
        }
        
        ImGui::EndPopup();
    }
}

void SceneGraphEditor::constructFrame()
{
    if (!is_open) {
        return;
    }

    ImGui::Begin("SceneGraph",&is_open);
    if (ImGui::TreeNode("Obj0"))
    {
        //rightClickMenu(0);
        static bool selection[5] = { false, false, false, false, false };

        for (int n = 0; n < 5; n++)
        {
            char buf[32];
            sprintf(buf, "Object %d", n);
            if (ImGui::Selectable(buf, selection[n]))
            {
                if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
                    memset(selection, 0, sizeof(selection));
                selection[n] ^= 1;
            }

            rightClickMenu(selection,5);
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Obj1"))
    {
        //rightClickMenu(0);
        static bool selection[5] = { false, false, false, false, false };

        for (int n = 0; n < 5; n++)
        {
            char buf[32];
            sprintf(buf, "Object %d", n);
            if (ImGui::Selectable(buf, selection[n]))
            {
                if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
                    memset(selection, 0, sizeof(selection));
                selection[n] ^= 1;
            }

            rightClickMenu(selection, 5);
        }
        if (ImGui::TreeNode("Obj1"))
        {
            //rightClickMenu(0);
            static bool selection[5] = { false, false, false, false, false };

            for (int n = 0; n < 5; n++)
            {
                char buf[32];
                sprintf(buf, "Object %d", n);
                if (ImGui::Selectable(buf, selection[n]))
                {
                    if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
                        memset(selection, 0, sizeof(selection));
                    selection[n] ^= 1;
                }

                rightClickMenu(selection, 5);
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

void SceneGraphEditor::init()
{

}

PBRTParser::ParseResult SceneGraphEditor::parsePBRTSceneFile(const std::filesystem::path & path, AssetLoader& assetLoader)
{
    return _parser.parse(*_currentScene,path,assetLoader);
}

SceneGraphEditor::~SceneGraphEditor()
{

}