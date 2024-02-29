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
#include "rocket.hpp"

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
    std::vector<Material*> materials;
    std::vector<Light*> lights;
    std::vector<AreaLight*> areaLights;

    bool is_empty = true;
    bool is_transform_detached;//transform not be affected by its parent
    bool is_selected = false;
    bool is_instance = false;

    glm::mat4 _selfTransform;
    glm::mat4 _finalTransform;

    rocket::signal<void(const glm::vec3 &)> selfTranslateChange;
    rocket::signal<void(const glm::vec3 &)> selfRotationChange;
    rocket::signal<void(const glm::vec3 &)> selfScaleChange;
    rocket::signal<void(const glm::mat4 &)> selfTransformChange;

    rocket::signal<void(const glm::vec3 &)> finalTranslateChange;
    rocket::signal<void(const glm::vec3 &)> finalRotationChange;
    rocket::signal<void(const glm::vec3 &)> finalScaleChange;
    rocket::thread_safe_signal<void(const glm::mat4 &)> finalTransformChange;
    rocket::signal<void(SceneGraphNode*)> selectedSignal;

    SceneGraphNode()
    {
        _selfTransform = glm::identity<glm::mat4x4>();
        _finalTransform = _selfTransform;
    }

    void visit(const SceneGraphVisitor& visitor){
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
    void visitPayload(SceneGraphVisitorPayloaded<Payload>& visitor){
        auto* payload = new Payload;
        visitor(this,payload);
        delete payload;
        for(auto* child : children){
            child->visit(visitor);
        }
    }

    template<class Payload>
    void visitPayload(const SceneGraphPreVisitorPayloaded<Payload>& pre_visitor,
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
        translate[0] = _selfTransform[3].x;
        translate[1] = _selfTransform[3].y;
        translate[2] = _selfTransform[3].z;
        WATCH_FILED_FlOAT3_NOTIFY(translate,[this](float x0, float x1, float x2){
            _selfTransform[3].x = x0;
            _selfTransform[3].y = x1;
            _selfTransform[3].z = x2;
            printf("%s change translate to [%f, %f, %f]\n", this->name.c_str(),x0,x1,x2);
            selfTranslateChange({x0,x1,x2});
            updateSelfTransform();
        });
        float rotation[3];
        glm::extractEulerAngleXYZ(_selfTransform,rotation[0],rotation[1],rotation[2]);
        WATCH_FILED_FlOAT3_NOTIFY(rotation,[this](float x0, float x1, float x2){
            printf("%s change rotation to [%f, %f, %f]\n", this->name.c_str(),x0,x1,x2);
        });
        float scale[3];
        scale[0] = _selfTransform[0][0];
        scale[1] = _selfTransform[1][1];
        scale[2] = _selfTransform[2][2];
        WATCH_FILED_FlOAT3_NOTIFY(scale,[this](float x0, float x1, float x2){
            _selfTransform[0][0] = x0;
            _selfTransform[1][1] = x1;
            _selfTransform[2][2] = x2;
            printf("%s change scale to [%f, %f, %f]\n", this->name.c_str(),x0,x1,x2);
            selfScaleChange({x0,x1,x2});
            updateSelfTransform();
        });

        if(!shapes.empty())
        {
            ImGui::Separator();
            for(auto shape : shapes)
            {
                shape->show();
            }
        }

        if(!materials.empty())
        {
            ImGui::Separator();
            for(auto material : materials)
            {
                material->show();
            }
        }

        if(!lights.empty())
        {
            ImGui::Separator();
            for(auto light : lights)
            {
                light->show();
            }
        }

        if(!areaLights.empty())
        {
            ImGui::Separator();
            for(auto areaLight : areaLights)
            {
                areaLight->show();
            }
        }
    }

    void updateSelfTransform()
    {
        selfTransformChange(_selfTransform);
        if(parent!= nullptr)
        {
            _finalTransform = parent->_finalTransform * _selfTransform;
        }else{
            _finalTransform = _selfTransform;
        }
        finalTransformChange(_finalTransform);
        for(auto & child : children)
        {
            child->updateFinalTransform(_finalTransform);
        }
    }

    void updateFinalTransform(const glm::mat4 & parentTransform)
    {
        _finalTransform = _selfTransform * parentTransform;
        finalTransformChange(_finalTransform);
        for(auto & child : children)
        {
            child->updateFinalTransform(_finalTransform);
        }
    }

    std::string InspectedName() override
    {
        return name;
    }
};

struct SceneGraph
{
    SceneGraphNode * root;
    std::vector<Material*> namedMaterials;
    std::vector<Texture*> namedTextures;
    //todo : note, when initiation node modify the data, a deep copy is preferred.
    std::vector<SceneGraphNode*> _objInstances;
    std::unique_ptr<Camera> camera;
};

struct SceneGraphEditor : EditorComponentGUI
{
	
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;

	// path is the absolute path to the file
	SceneGraph* parsePBRTSceneFile(const std::filesystem::path& path, AssetManager& assetLoader);
	PBRTParser _parser;
	std::shared_ptr<PBRTScene> _currentScene;
	SceneGraphNode* _sceneGraphRootNode;
};