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

   static SceneGraphNode * currentSingleSelectedNode = nullptr;

   static auto singleSelectionPreVisitor = [](SceneGraphNode* node)
   {
       if(!node->children.empty()){
           int nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
           if(node == currentSingleSelectedNode)
           {
               nodeFlags |= ImGuiTreeNodeFlags_Selected;
           }
           bool is_node_open = ImGui::TreeNodeEx(node->name.c_str(), nodeFlags);
           if(ImGui::IsItemClicked())
           {
               if (node == currentSingleSelectedNode)
               {
                   currentSingleSelectedNode = nullptr;
                   node->unSelectedSignal(node);
               }
               else
               {
                   currentSingleSelectedNode = node;
                   node->selectedSignal(node);
               }
           } //node->is_selected ^= 1;
           //rightClickMenu2(node->is_selected);
           return std::make_pair(is_node_open, is_node_open);
       }else{
           // For leaf node
           bool selected = (currentSingleSelectedNode == node);
           if (ImGui::Selectable(node->name.c_str(), &selected))
           {
//                if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
//                    node->is_selected = false;//memset(selection, 0, sizeof(selection));
//                node->is_selected ^= 1;
           }
           if (selected)
           {
               currentSingleSelectedNode = node;
               node->selectedSignal(node);
           }
           else
           {
               node->unSelectedSignal(node);
           }
           //rightClickMenu2(node->is_selected);
           return std::make_pair(false, false);
       }
   };

   static auto singleSelectionPostVisitor = [](SceneGraphNode* node)->void{
        ImGui::TreePop();
   };

    _sceneGraphRootNode->visit(singleSelectionPreVisitor,singleSelectionPostVisitor);

    if(currentSingleSelectedNode == nullptr)
    {
        Inspector::inspectDummy();
    }else{
        Inspector::inspect(currentSingleSelectedNode);
    }

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