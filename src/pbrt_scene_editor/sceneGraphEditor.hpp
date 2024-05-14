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
struct SceneGraph;

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
    SceneGraph* graph = nullptr;
    std::string name;
    std::vector<SceneGraphNode*> children;
    SceneGraphNode* parent = nullptr;

    std::vector<Shape*> shapes;
    std::vector<Material*> materials;
    std::vector<Light*> lights;
    std::vector<AreaLight*> areaLights;

    bool is_empty = true;
    bool is_transform_detached = false;//transform not be affected by its parent
    bool m_is_selected = false;
    bool is_instance = false;

    glm::mat4 _selfTransform;
    glm::mat4 _finalTransform;

    SceneGraphNode()
    {
        _selfTransform = glm::identity<glm::mat4>();
        _finalTransform = _selfTransform;
    }

    void visit(const SceneGraphVisitor& visitor);

    void visit(const SceneGraphPreVisitor& pre_visitor, const SceneGraphPostVisitor& post_visitor);

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
    void show() override;
    
    bool is_selected() const {
        return m_is_selected;
    }

    void select();

    void unselect();

    void toggle_select() {
        if (is_selected())
        {
            unselect();
        }
        else {
            select();
        }
    }

    void updateSelfTransform();

    void updateFinalTransform();

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

    rocket::signal<void(SceneGraphNode*)> nodeSelectSignal;
    rocket::signal<void(SceneGraphNode*)> nodeUnSelectSignal;

    rocket::signal<void(SceneGraphNode*)> nodeSelfTranslateChangeSignal;
    rocket::signal<void(SceneGraphNode*)> nodeSelfRotationChangeSignal;
    rocket::signal<void(SceneGraphNode*)> nodeSelfScaleChangeSignal;
    rocket::signal<void(SceneGraphNode*)> nodeSelfTransformChangeSignal;

    rocket::signal<void(SceneGraphNode*)> nodeFinalTranslateChangeSignal;
    rocket::signal<void(SceneGraphNode*)> nodeFinalRotationChangeSignal;
    rocket::signal<void(SceneGraphNode*)> nodeFinalScaleChangeSignal;
    rocket::thread_safe_signal<void(SceneGraphNode*)> nodeFinalTransformChangeSignal;

    void selectNode(SceneGraphNode* node)
    {
        nodeSelectSignal(node);
    }

    void unSelectNode(SceneGraphNode* node)
    {
        nodeUnSelectSignal(node);
    }

    void selfTranslateChange(SceneGraphNode* node)
    {
        nodeSelfTranslateChangeSignal(node);
    }

    void selfRotationChange(SceneGraphNode* node)
    {
        nodeSelfRotationChangeSignal(node);
    }

    void selfScaleChange(SceneGraphNode* node)
    {
        nodeSelfScaleChangeSignal(node);
    }

    void finalTranslateChange(SceneGraphNode* node)
    {
        nodeFinalTranslateChangeSignal(node);
    }

    void finalRotationChange(SceneGraphNode* node)
    {
        nodeFinalRotationChangeSignal(node);
    }

    void finalScaleChange(SceneGraphNode* node)
    {
        nodeFinalScaleChangeSignal(node);
    }

    void selfTransformChange(SceneGraphNode* node)
    {
        nodeSelfTransformChangeSignal(node);
    }

    void finalTransformChange(SceneGraphNode* node)
    {
        nodeFinalTransformChangeSignal(node);
    }
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
    //SceneGraphNode* _sceneGraphRootNode;
    std::shared_ptr<SceneGraph> _sceneGraph;
};