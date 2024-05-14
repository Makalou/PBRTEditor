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

void SceneGraphNode::visit(const SceneGraphVisitor& visitor)
{
    visitor(this);
    for (auto* child : children) {
        child->visit(visitor);
    }
}

void SceneGraphNode::visit(const SceneGraphPreVisitor& pre_visitor, const SceneGraphPostVisitor& post_visitor)
{
    auto pair = pre_visitor(this);
    if (pair.first) {
        for (auto* child : children) {
            child->visit(pre_visitor, post_visitor);
        }
    }
    if (pair.second)
        post_visitor(this);
}

void SceneGraphNode::show()
{
    float translate[3];
    translate[0] = _selfTransform[3].x;
    translate[1] = _selfTransform[3].y;
    translate[2] = _selfTransform[3].z;

    if (WATCH_FILED_FlOAT3(translate,-10.0,10.0))
    {
        _selfTransform[3].x = translate[0];
        _selfTransform[3].y = translate[1];
        _selfTransform[3].z = translate[2];
        //printf("%s change translate to [%f, %f, %f]\n", this->name.c_str(),x0,x1,x2);
        graph->nodeSelfTranslateChangeSignal(this);
        updateSelfTransform();
    }

    float rotation[3];
    glm::extractEulerAngleXYZ(_selfTransform, rotation[0], rotation[1], rotation[2]);
    if (WATCH_FILED_FlOAT3(rotation, -10.0, 10.0))
    {

    }
    float scale[3];
    scale[0] = _selfTransform[0][0];
    scale[1] = _selfTransform[1][1];
    scale[2] = _selfTransform[2][2];
    if (WATCH_FILED_FlOAT3(scale, -10.0, 10.0))
    {
        _selfTransform[0][0] = scale[0];
        _selfTransform[1][1] = scale[1];
        _selfTransform[2][2] = scale[2];
        //printf("%s change scale to [%f, %f, %f]\n", this->name.c_str(),x0,x1,x2);
        graph->nodeSelfScaleChangeSignal(this);
        updateSelfTransform();
    }
        
    if (!shapes.empty())
    {
        ImGui::Separator();
        for (auto shape : shapes)
        {
            shape->show();
        }
    }

    if (!materials.empty())
    {
        ImGui::Separator();
        for (auto material : materials)
        {
            material->show();
        }
    }

    if (!lights.empty())
    {
        ImGui::Separator();
        for (auto light : lights)
        {
            light->show();
        }
    }

    if (!areaLights.empty())
    {
        ImGui::Separator();
        for (auto areaLight : areaLights)
        {
            areaLight->show();
        }
    }
}

void SceneGraphNode::updateSelfTransform()
{
    graph->selfTransformChange(this);
    updateFinalTransform();
}

void SceneGraphNode::updateFinalTransform()
{
    if (parent != nullptr && !is_transform_detached)
    {
        _finalTransform = _selfTransform * parent->_finalTransform;
    }
    else {
        _finalTransform = _selfTransform;
    }
    graph->finalTransformChange(this);
    for (auto& child : children)
    {
        child->updateFinalTransform();
    }
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