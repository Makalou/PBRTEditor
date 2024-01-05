//
// Created by 王泽远 on 2024/1/2.
//

#include "SceneBuilder.hpp"
#include "scene.h"
#include "sceneGraphEditor.hpp"
#include <cassert>

void PBRTSceneBuilder::AttributeBegin() {
    static int visitedNodeCount = 0;
    auto* newAttributeNode = new SceneGraphNode;
    newAttributeNode->name = "node-" + std::to_string(visitedNodeCount++);
    _currentVisitNode->children.push_back(newAttributeNode);
    newAttributeNode->parent = _currentVisitNode;
    _currentVisitNode = newAttributeNode;
}

void PBRTSceneBuilder::AttributeEnd() {
    _currentVisitNode = _currentVisitNode->parent;
}

void PBRTSceneBuilder::WordlBegin() {
    auto* worldRootNode = new SceneGraphNode;
    worldRootNode->name = "world_root";
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
void PBRTSceneBuilder::ConcatTransform(){}
void PBRTSceneBuilder::CoordinateSystem(){}
void PBRTSceneBuilder::CoordSysTransform(){}
void PBRTSceneBuilder::ColorSpace(){}
void PBRTSceneBuilder::Indentity(){}
void PBRTSceneBuilder::LookAt(){

}
void PBRTSceneBuilder::NamedMaterial(){

}

void PBRTSceneBuilder::ObjectBegin(){

}

void PBRTSceneBuilder::ObjectInstance(const std::string & instanceName){

}

void PBRTSceneBuilder::Option(){}

void PBRTSceneBuilder::ReverseOrientation(){}

void PBRTSceneBuilder::Rotate(){}

void PBRTSceneBuilder::Scale(){}

void PBRTSceneBuilder::Transform(){}

void PBRTSceneBuilder::Translate(){}

void PBRTSceneBuilder::TransformTimes(){}