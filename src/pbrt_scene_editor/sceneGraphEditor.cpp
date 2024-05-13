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

void SceneGraphNode::select()
{
    m_is_selected = true;
    graph->selectNode(this);
}

void SceneGraphNode::unselect()
{
    m_is_selected = false;
    graph->unSelectNode(this);
}

void SceneGraphEditor::constructFrame()
{
    if (!is_open) {
        return;
    }

    ImGui::Begin("SceneGraph",&is_open);

   if(_sceneGraph == nullptr){
        ImGui::End();
        return;
   }

   std::vector<SceneGraphNode*> currentInspectingNodes{};

   static auto singleSelectionPreVisitor = [&](SceneGraphNode* node)
   {
       if(!node->children.empty()){
           int nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
           if(node->is_selected())
           {
               nodeFlags |= ImGuiTreeNodeFlags_Selected;
           }
           bool is_node_open = ImGui::TreeNodeEx(node->name.c_str(), nodeFlags);
           if(ImGui::IsItemClicked())
           {
               if (node->is_selected())
               {
                   node->unselect();
               }
               else
               {
                   node->select();
               }
           } //node->is_selected ^= 1;
           //rightClickMenu2(node->is_selected);
           return std::make_pair(is_node_open, is_node_open);
       }else{
           // For leaf node
           bool selected = node->is_selected();
           if (ImGui::Selectable(node->name.c_str(), &selected))
           {
//                if (!ImGui::GetIO().KeyCtrl)    // Clear selection when CTRL is not held
//                    node->is_selected = false;//memset(selection, 0, sizeof(selection));
//                node->is_selected ^= 1;
           }
           if (selected)
           {
               node->select();
           }
           else
           {
               node->unselect();
           }
           //rightClickMenu2(node->is_selected);
           return std::make_pair(false, false);
       }
   };

   static auto singleSelectionPostVisitor = [](SceneGraphNode* node)->void{
        ImGui::TreePop();
   };

   static auto collectSelectedPreVisitor = [&](SceneGraphNode* node){
       if (node->is_selected())
       {
           currentInspectingNodes.push_back(node);
       }

       return std::make_pair(!node->children.empty(), false);
   };

   static auto collectSelectedPostVisitor = [](SceneGraphNode* node)->void {};

    _sceneGraph->root->visit(singleSelectionPreVisitor,singleSelectionPostVisitor);
    _sceneGraph->root->visit(collectSelectedPreVisitor, collectSelectedPostVisitor);

    if(currentInspectingNodes.empty())
    {
        Inspector::inspectDummy();
    }else{
        Inspector::inspect(currentInspectingNodes);
    }

    ImGui::End();
}

void SceneGraphEditor::init()
{
    _sceneGraph = nullptr;
}

SceneGraph* SceneGraphEditor::parsePBRTSceneFile(const std::filesystem::path & path, AssetManager& assetManager)
{
    PBRTSceneBuilder builder{};
    assetManager.setWorkDir(path.parent_path());
    auto res = _parser.parse(builder,path,assetManager);
    // do something to current scene
    _sceneGraph.reset(builder.sceneGraph);
    return builder.sceneGraph;
}

SceneGraphEditor::~SceneGraphEditor()
{

}