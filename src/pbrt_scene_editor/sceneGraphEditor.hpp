#pragma once

#include "editorComponent.hpp"

#include <string>

#include "PBRTParser.h"

#include <filesystem>

#include "Inspector.hpp"

#include <functional>

struct AssetLoader;
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

    bool is_selected;

    int intermediate_val;
    int accumulated_val;

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
};

struct SceneGraphEditor : EditorComponentGUI
{
	
	void constructFrame() override;
	void init();
	~SceneGraphEditor() override;

	// path is the absolute path to the file
	PBRTParser::ParseResult parsePBRTSceneFile(const std::filesystem::path& path, AssetLoader& assetLoader);
	PBRTParser _parser;
	std::shared_ptr<PBRTScene> _currentScene;
	SceneGraphNode* _sceneGraphRootNode;
};