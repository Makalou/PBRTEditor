#include "sceneGraphEditor.hpp"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

#include <cstdio>
#include <vector>

#include "AssetManager.hpp"

#include "scene.h"
#include "SceneBuilder.hpp"

void rightClickMenu(const bool* selections, int count)
{
    bool all_false = true;
    for (int i = 0; i < count; i++)
    {
        if (selections[i]) {
            all_false = false;
            break;
        }
    }

    if (all_false) return;

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
        }else if(selected_idices.size() >1) {
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

void rightClickMenu2(bool selected){

}

void SceneGraphEditor::constructFrame()
{
    if (!is_open) {
        return;
    }

    ImGui::Begin("SceneGraph",&is_open);

   if(_sceneGraphRootNode == nullptr){
        ImGui::End();
        return;
   }

    static auto nodePreVisitor = [](SceneGraphNode* node){
        if(node->children.empty()){
            if (ImGui::Selectable(node->name.c_str(), &node->is_selected))
            {
//                if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
//                    node->is_selected = false;//memset(selection, 0, sizeof(selection));
//                node->is_selected ^= 1;
            }
            if(node->is_selected)
                Inspector::inspect(node);
            rightClickMenu2(node->is_selected);
            return std::make_pair(false, false);
        }else{
            auto nodeFlags = node->is_selected? ImGuiTreeNodeFlags_OpenOnArrow| ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_OpenOnArrow;
            bool is_node_open = ImGui::TreeNodeEx(node->name.c_str(), nodeFlags);
            if(ImGui::IsItemClicked())
                node->is_selected ^= 1;
            if(node->is_selected)
                Inspector::inspect(node);
            rightClickMenu2(node->is_selected);
            return std::make_pair(is_node_open, is_node_open);
        }
    };

    static auto nodePostVisitor = [](SceneGraphNode* node)->void{
        ImGui::TreePop();
    };

    _sceneGraphRootNode->visit(nodePreVisitor,nodePostVisitor);

    ImGui::End();
}

void SceneGraphEditor::init()
{
    _sceneGraphRootNode = nullptr;
}

SceneGraph* SceneGraphEditor::parsePBRTSceneFile(const std::filesystem::path & path, AssetManager& assetManager)
{
    PBRTSceneBuilder builder{};
    assetManager.setWorkDir(path.parent_path());
    auto res = _parser.parse(builder,path,assetManager);
    // do something to current scene
    _sceneGraphRootNode = builder._currentVisitNode;
    return builder.sceneGraph;
}

SceneGraphEditor::~SceneGraphEditor()
{

}