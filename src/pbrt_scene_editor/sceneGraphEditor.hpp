#pragma once

#include "editorComponent.hpp"
#include <string>
#include "PBRTParser.h"
#include <filesystem>
#include "Inspector.hpp"
#include <functional>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "scene.h"

struct AssetManager;
struct PBRTScene;

struct SceneGraphNode;

using SceneGraphVisitor = std::function<void(SceneGraphNode*)>;

using SceneGraphPreVisitor = std::function<std::pair<bool,bool>(SceneGraphNode*)>;

using SceneGraphPostVisitor = std::function<void(SceneGraphNode*)>;

template<typename Payload>
using SceneGraphVisitorPayloaded = std::function<void(SceneGraphNode*,Payload*)>;

template<typename Payload>
using SceneGraphPreVisitorPayloaded = std::function<std::pair<bool,bool>(SceneGraphNode*,Payload*)>;

template<typename Payload>
using SceneGraphPostVisitorPayloaded = std::function<void(SceneGraphNode*,Payload*)>;

//template<typename T>
//struct SceneGraphExVisitor
//{
//    //using visitExData = std::pair<bool,T>;
//public:
//    static auto makeVisitor(std::function<void(SceneGraphNode*,T &)> visit_action){
//        SceneGraphExVisitor<T> visitor;
//        visitor._visit = visit_action;
//        return visitor;
//    }
//
//    T& getExData(SceneGraphNode* node){
//        auto it = _exData.find(node);
//        if(it == _exData.end()){
//            T _data;
//            return _exData.emplace(node,_data).first->second;
//        }else{
//            return *it->second;
//        }
//    }
//
//    void visit(SceneGraphNode* node){
//        _visit(node,this->getExData(node));
//    }
//
//private:
//    std::function<void(SceneGraphExVisitor<T> & , SceneGraphNode*)> _visit;
//    std::unordered_map<SceneGraphNode*,T> _exData;
//
//    void sweep_all_unvisited()
//    {
//
//    }
//};

struct SceneGraphNode : Inspectable
{
    std::string name;
    std::vector<SceneGraphNode*> children;
    SceneGraphNode* parent;

    std::vector<Shape*> shapes;
    std::vector<Light*> lights;
    std::vector<AreaLight*> areaLights;

    bool is_empty;
    bool is_transform_detached;//transform not be affected by its parent
    bool is_selected;

    glm::mat4x4 _selfTransform;
    glm::mat4x4 _finalTransform;

    SceneGraphNode()
    {
        _selfTransform = glm::identity<glm::mat4x4>();
        _finalTransform = _selfTransform;
    }

    void visit(SceneGraphVisitor& visitor){
        visitor(this);
        for(auto* child : children){
            child->visit(visitor);
        }
    }

    void visit(const SceneGraphPreVisitor& pre_visitor,const SceneGraphPostVisitor& post_visitor){
        auto pair =  pre_visitor(this);
        if(pair.first){
            for(auto* child : children){
                child->visit(pre_visitor,post_visitor);
            }
        }
        if(pair.second)
            post_visitor(this);
    }

    template<class Payload>
    void visit(SceneGraphVisitorPayloaded<Payload>& visitor){
        auto* payload = new Payload;
        visitor(this,payload);
        delete payload;
        for(auto* child : children){
            child->visit(visitor);
        }
    }

    template<class Payload>
    void visit(const SceneGraphPreVisitorPayloaded<Payload>& pre_visitor,
               const SceneGraphPostVisitorPayloaded<Payload>& post_visitor){
        auto* payload = new Payload;
        auto pair =  pre_visitor(this,payload);
        if(pair.first){
            for(auto* child : children){
                child->visit(pre_visitor,post_visitor);
            }
        }
        if(pair.second)
            post_visitor(this,payload);
        delete payload;
    }

//    void modify(int new_val){
//        intermediate_val = new_val;
//        reaccumulate(parent->accumulated_val);
//    }
//
//    void reaccumulate(int val)
//    {
//        accumulated_val = accumulate(val);
//        for (auto* child : children) { //todo maybe paralleized by omp task ...
//            child->reaccumulate(accumulated_val);
//        }
//    }
//
//    int accumulate(int val)
//    {
//
//    }
    void show() override
    {
        float translate[3];
        translate[0] = glm::vec3{_selfTransform[3]}.x;
        translate[1] = glm::vec3{_selfTransform[3]}.y;
        translate[2] = glm::vec3{_selfTransform[3]}.z;
        SHOW_FILED_FlOAT3(translate)
        float rotation[3];
        glm::extractEulerAngleXYZ(_selfTransform,rotation[0],rotation[1],rotation[2]);
        SHOW_FILED_FlOAT3(rotation);
        float scale[3];
        scale[0] = _selfTransform[0][0];
        scale[1] = _selfTransform[1][1];
        scale[2] = _selfTransform[2][2];
        SHOW_FILED_FlOAT3(scale);
        ImGui::Separator();
        for(auto shape : shapes)
        {
            shape->show();
        }
        for(auto light : lights)
        {
            light->show();
        }
        for(auto areaLight : lights)
        {
            areaLight->show();
        }
    }

    std::string InspectedName() override
    {
        return name;
    }
};

struct SceneGraphEditor : EditorComponentGUI
{
	
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;

	// path is the absolute path to the file
	SceneGraphNode* parsePBRTSceneFile(const std::filesystem::path& path, AssetManager& assetLoader);
	PBRTParser _parser;
	std::shared_ptr<PBRTScene> _currentScene;
	SceneGraphNode* _sceneGraphRootNode;
};