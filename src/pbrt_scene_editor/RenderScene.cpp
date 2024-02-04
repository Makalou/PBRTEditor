//
// Created by 王泽远 on 2024/1/6.
//

#include "RenderScene.h"

namespace renderScene
{
    MeshRigid *MeshRigidHandle::operator->() const {
        return &scene->meshes[idx].second;
    }

    bool MeshRigidHandle::operator==(const renderScene::MeshRigidHandle &other) const {
        if(scene != other.scene) return false;
        if(idx != other.idx) return false;
        return true;
    }
}
