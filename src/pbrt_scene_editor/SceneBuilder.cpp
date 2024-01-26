//
// Created by 王泽远 on 2024/1/2.
//

#include "SceneBuilder.hpp"
#include "scene.h"
#include "sceneGraphEditor.hpp"
#include <cassert>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

struct HashCounter
{
    unsigned int& operator[](const std::string & counter_name) {
        auto it = counters.find(counter_name);
        if(it == counters.end()){
            counters.emplace(counter_name,0);
        }
        return counters[counter_name];
    }
    std::unordered_map<std::string ,unsigned int> counters;
};

static HashCounter NameCounter;

void PBRTSceneBuilder::AttributeBegin() {
    auto* newAttributeNode = new SceneGraphNode;
    newAttributeNode->is_empty = true;
    newAttributeNode->parent = _currentVisitNode;
    newAttributeNode->_finalTransform = _currentVisitNode->_finalTransform;
    _currentVisitNode = newAttributeNode;
}

void PBRTSceneBuilder::AttributeEnd() {

    if(_currentVisitNode->children.size() > 1)
    {
        //group node, just for group multiple node together, keep.
        _currentVisitNode->is_empty = false;
    }

    if(_currentVisitNode->children.size() == 1 && _currentVisitNode->is_empty)
    {
        //meaningless node. should be raked.
        _currentVisitNode->parent->children.push_back(_currentVisitNode->children[0]);
    }

    if(!_currentVisitNode->is_empty)
    {
        if(_currentVisitNode->name.empty()){
            _currentVisitNode->name += " <node-" + std::to_string(NameCounter["node"]++) + ">";
        }
        _currentVisitNode->parent->children.push_back(_currentVisitNode);
        _currentVisitNode = _currentVisitNode->parent;
        return;
    }

    auto* tmp =  _currentVisitNode;
    _currentVisitNode = _currentVisitNode->parent;
    delete tmp;
}

void PBRTSceneBuilder::WorldBegin() {
    NameCounter = HashCounter();
    auto* worldRootNode = new SceneGraphNode;
    worldRootNode->name = "world_root";
    worldRootNode->is_empty = false;
    Identity();
    assert(_currentVisitNode == nullptr);
    _currentVisitNode = worldRootNode;
}

void PBRTSceneBuilder::WorldEnd() {
    //do nothing
    assert(_currentVisitNode->parent== nullptr);
}

void PBRTSceneBuilder::ActiveTransformAll(){}

void PBRTSceneBuilder::ActiveTransformEndTIme(){}

void PBRTSceneBuilder::ActiveTransformTime(){}

void PBRTSceneBuilder::ConcatTransform(const float* m){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        //todo glm stores in column-wise
        auto concatMat = glm::make_mat4x4(m);
        _currentVisitNode->_finalTransform = concatMat * _currentVisitNode->_finalTransform;
        _currentVisitNode->_selfTransform = concatMat * _currentVisitNode->_finalTransform;
    }
}

void PBRTSceneBuilder::CoordinateSystem(){

}

void PBRTSceneBuilder::CoordSysTransform(){

}

void PBRTSceneBuilder::ColorSpace(){

}

void PBRTSceneBuilder::Identity(){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_finalTransform = glm::identity<glm::mat4x4>();
        _currentVisitNode->is_transform_detached = true;
    }
}

void PBRTSceneBuilder::LookAt(const float*){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
    }
}
void PBRTSceneBuilder::NamedMaterial(){

}

void PBRTSceneBuilder::ObjectBegin(const std::string & instanceName){
    auto* newNode = new SceneGraphNode;
    newNode->is_empty = true;
    newNode->name = instanceName;
    newNode->parent = _currentVisitNode;
    _objInstances.push_back(newNode);
    _currentVisitNode = newNode;
}

void PBRTSceneBuilder::ObjectEnd() {
    _currentVisitNode = _currentVisitNode->parent;
}

void PBRTSceneBuilder::ObjectInstance(const std::string & instanceName){
    if(_currentVisitNode!= nullptr)
    {
        for(const auto obj : _objInstances)
        {
            if(obj->name == instanceName)
            {
                _currentVisitNode->children.push_back(obj);
//                auto* instanceCopy = new SceneGraphNode;
//                instanceCopy->name = obj->name;
//                instanceCopy->_selfTransform = obj->_selfTransform;
//                instanceCopy->_finalTransform = _currentVisitNode->_finalTransform * obj->_finalTransform;
//
//                for(auto shape : obj->shapes)
//                {
//                    instanceCopy->shapes.push_back(shape);
//                }
//                for(auto light : obj->lights)
//                {
//                    instanceCopy->lights.push_back(light);
//                }
//                for(auto areaLight : obj->areaLights)
//                {
//                    instanceCopy->areaLights.push_back(areaLight);
//                }
//
//                _currentVisitNode->children.push_back(instanceCopy);
            }

        }
    }
}

void PBRTSceneBuilder::Option(){

}

void PBRTSceneBuilder::ReverseOrientation(){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
    }
}

void PBRTSceneBuilder::Rotate(const float * v){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_finalTransform = glm::rotate(_currentVisitNode->_finalTransform,
                                                         glm::radians(v[0]),
                                                         {v[1],v[2],v[3]});
        _currentVisitNode->_selfTransform = glm::rotate(_currentVisitNode->_selfTransform,
                                                         glm::radians(v[0]),
                                                         {v[1],v[2],v[3]});
    }
}

void PBRTSceneBuilder::Scale(const float* s){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_finalTransform = glm::scale(_currentVisitNode->_finalTransform,
                                                        {s[0],s[1],s[2]});
        _currentVisitNode->_selfTransform= glm::scale(_currentVisitNode->_selfTransform,
                                                        {s[0],s[1],s[2]});
    }

}

void PBRTSceneBuilder::Transform(const float* m){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_finalTransform = glm::make_mat4x4(m);
        _currentVisitNode->is_transform_detached = true;
    }
}

void PBRTSceneBuilder::Translate(const float* t){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_finalTransform = glm::translate(_currentVisitNode->_finalTransform, {t[0], t[1], t[2]});
        _currentVisitNode->_selfTransform = glm::translate(_currentVisitNode->_selfTransform, {t[0], t[1], t[2]});
    }
}

void PBRTSceneBuilder::TransformTimes(){

}

void PBRTSceneBuilder::AddLightSource(Light * light) {
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        //_currentVisitNode->name += " LightSource";
        _currentVisitNode->lights.push_back(light);
    }
}

void PBRTSceneBuilder::AddShape(Shape* shape) {
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        //_currentVisitNode->name += " Shape";
        _currentVisitNode->shapes.push_back(shape);
    }
}

void PBRTSceneBuilder::AddAreaLight(AreaLight * areaLight) {
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        //_currentVisitNode->name += " AreaLight";
        _currentVisitNode->areaLights.push_back(areaLight);
    }
}

void PBRTSceneBuilder::AddCamera(const Camera* cam) {

}