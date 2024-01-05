//
// Created by 王泽远 on 2024/1/2.
//

#ifndef PBRTEDITOR_SCENEBUILDER_HPP
#define PBRTEDITOR_SCENEBUILDER_HPP

#include "Inspector.hpp"
#include "glm/glm.hpp"
#include <iostream>

struct SceneGraphNode;

struct GlobalRenderSetting : Inspectable
{

};

struct PBRTSceneBuilder
{

    void AttributeBegin();
    void AttributeEnd();
    void ActiveTransformAll();
    void ActiveTransformEndTIme();
    void ActiveTransformTime();
    void ConcatTransform();
    void CoordinateSystem();
    void CoordSysTransform();
    void ColorSpace();
    void Indentity();
    void LookAt();
    void NamedMaterial();
    void ObjectBegin();
    void ObjectInstance(const std::string &);
    void Option();
    void ReverseOrientation();
    void Rotate();
    void Scale();
    void Transform();
    void Translate();
    void TransformTimes();
    void WordlBegin();
    void WorldEnd();

    SceneGraphNode* _currentVisitNode;
    GlobalRenderSetting _globalRenderSetting;
    glm::mat4x4 _currentTransformMatrix;
};

#endif //PBRTEDITOR_SCENEBUILDER_HPP
