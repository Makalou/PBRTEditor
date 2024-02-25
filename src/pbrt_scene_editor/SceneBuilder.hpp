//
// Created by 王泽远 on 2024/1/2.
//

#ifndef PBRTEDITOR_SCENEBUILDER_HPP
#define PBRTEDITOR_SCENEBUILDER_HPP

#include "Inspector.hpp"
#include <iostream>
#include <vector>

struct SceneGraphNode;

struct GlobalRenderSetting : Inspectable
{

};

struct Camera;

struct Shape;
struct Light;
struct AreaLight;
struct Material;
struct Texture;

struct RenderScene;

struct PBRTSceneBuilder
{
    void AttributeBegin();
    void AttributeEnd();
    void ActiveTransformAll();
    void ActiveTransformEndTIme();
    void ActiveTransformTime();
    void ConcatTransform(const float*);
    void CoordinateSystem();
    void CoordSysTransform();
    void ColorSpace();
    void Identity();
    void LookAt(const float*);
    void AddNamedMaterial(Material *);
    void NamedMaterial(const std::string &);
    void ObjectBegin(const std::string &); //assume nested objectBegin is not allowed
    void ObjectEnd();
    void ObjectInstance(const std::string &);
    void Option();
    void ReverseOrientation();
    void Rotate(const float*);
    void Scale(const float*);
    void Transform(const float*);
    void Translate(const float*);
    void TransformTimes();
    void AddShape(Shape*);
    void AddTexture(Texture*);
    void AddMaterial(Material*);
    void AddLightSource(Light*);
    void AddAreaLight(AreaLight*);
    void AddCamera(const Camera* );
    void WorldBegin();
    void WorldEnd();

    SceneGraphNode* _currentVisitNode;
    std::vector<SceneGraphNode*> _objInstances; //todo : note, when initiation node modify the data, a deep copy is preferred.
    GlobalRenderSetting _globalRenderSetting;
    std::unique_ptr<Camera> camera;
    std::vector<Texture*> namedTextures;
    std::vector<Material*> namedMaterials;
};

#endif //PBRTEDITOR_SCENEBUILDER_HPP
