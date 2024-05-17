//
// Created by 王泽远 on 2024/1/2.
//

#include "SceneBuilder.hpp"
#include "scene.h"
#include "sceneGraphEditor.hpp"
#include <cassert>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

PBRTSceneBuilder::PBRTSceneBuilder() {
    sceneGraph = new SceneGraph;
}

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
    newAttributeNode->graph = sceneGraph;
    newAttributeNode->is_empty = true;
    newAttributeNode->parent = _currentVisitNode;
    newAttributeNode->is_instance = _currentVisitNode->is_instance;
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
    worldRootNode->graph = sceneGraph;
    worldRootNode->name = "world_root";
    worldRootNode->is_empty = false;
    Identity();
    assert(_currentVisitNode == nullptr);
    _currentVisitNode = worldRootNode;
    sceneGraph->root = _currentVisitNode;
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

void PBRTSceneBuilder::LookAt(const float* lookAt){
    sceneGraph->globalRenderSetting.camera.eye = glm::vec3{ lookAt[0],lookAt[1],lookAt[2] };
    sceneGraph->globalRenderSetting.camera.look = glm::vec3{ lookAt[3],lookAt[4],lookAt[5] };
    sceneGraph->globalRenderSetting.camera.up = glm::vec3{ lookAt[6],lookAt[7],lookAt[8] };
}

void PBRTSceneBuilder::AddNamedMaterial(Material * material){
    for(const auto & mat : sceneGraph->namedMaterials)
    {
        if(mat->name == material->name)
        {
            throw std::runtime_error("Material " + material->name + " has been defined.");
        }
    }
    sceneGraph->namedMaterials.push_back(material);
}

void PBRTSceneBuilder::NamedMaterial(const std::string & name) {
    if(_currentVisitNode!= nullptr)
    {
        for(const auto & mat : sceneGraph->namedMaterials)
        {
            if(mat->name == name)
            {
                _currentVisitNode->materials.push_back(mat);
                return;
            }
        }
        throw std::runtime_error("Couldn't find material " + name);
    }
}

void PBRTSceneBuilder::ObjectBegin(const std::string & instanceName){
    auto* newNode = new SceneGraphNode;
    newNode->graph = sceneGraph;
    newNode->is_empty = true;
    newNode->name = instanceName;
    newNode->is_instance = true;
    newNode->parent = _currentVisitNode;
    sceneGraph->_objInstances.push_back(newNode);
    _currentVisitNode = newNode;
}

void PBRTSceneBuilder::ObjectEnd() {
    _currentVisitNode = _currentVisitNode->parent;
}

void PBRTSceneBuilder::ObjectInstance(const std::string & instanceName){
    if(_currentVisitNode!= nullptr)
    {
        for(const auto obj : sceneGraph->_objInstances)
        {
            if(obj->name == instanceName)
            {
                /*
                 * It's a little tricky here because when use instance, the transform actually
                 * means the transform in "instance space".
                 * */
                _currentVisitNode->children.push_back(obj);
                return;
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
        float v0 = v[0]; float v1 = v[1]; float v2 = v[2]; float v3 = v[3];
        _currentVisitNode->_finalTransform = glm::rotate(_currentVisitNode->_finalTransform,
                                                         glm::radians(v0),
                                                         {v1,v2,v3});
        _currentVisitNode->_selfTransform = glm::rotate(_currentVisitNode->_selfTransform,
                                                         glm::radians(v0),
                                                         {v1,v2,v3});
    }
}

void PBRTSceneBuilder::Scale(const float* s){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        float s0 = s[0]; float s1= s[1]; float s2= s[2];
        _currentVisitNode->_finalTransform = glm::scale(_currentVisitNode->_finalTransform,
                                                        {s0,s1,s2});
        _currentVisitNode->_selfTransform= glm::scale(_currentVisitNode->_selfTransform,
                                                        {s0,s1,s2});
    }

}

void PBRTSceneBuilder::Transform(const float* m){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        _currentVisitNode->_selfTransform = glm::make_mat4x4(m);
        _currentVisitNode->_finalTransform = _currentVisitNode->_selfTransform;
        _currentVisitNode->is_transform_detached = true;
    }
}

void PBRTSceneBuilder::Translate(const float* t){
    if(_currentVisitNode!= nullptr)
    {
        _currentVisitNode->is_empty = false;
        float t0 = t[0]; float t1 = t[1]; float t2 = t[2];
        _currentVisitNode->_finalTransform = glm::translate(_currentVisitNode->_finalTransform, {t0, t1, t2});
        _currentVisitNode->_selfTransform = glm::translate(_currentVisitNode->_selfTransform, {t0, t1, t2});
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

void PBRTSceneBuilder::AddTexture(Texture * texture) {
    sceneGraph->namedTextures.push_back(texture);
}

Texture* PBRTSceneBuilder::GetTexture(const std::string &name) {
    for(auto * tex : sceneGraph->namedTextures)
    {
        if(tex->name == name)
            return tex;
    }
    return nullptr;
}

void PBRTSceneBuilder::AddMaterial(Material * material) {
    if(_currentVisitNode != nullptr)
    {
        _currentVisitNode->materials.push_back(material);
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

void PBRTSceneBuilder::SetCamera(Camera* cam) {
    sceneGraph->globalRenderSetting.camera.camera = cam;
}