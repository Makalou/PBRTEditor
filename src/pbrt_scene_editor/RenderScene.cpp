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

    RenderScene::RenderScene(const std::shared_ptr<DeviceExtended>& device) : backendDevice(device)
    {
        {
            mainView.camera.data = backendDevice->allocateObservedBufferPull<MainCameraData>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).value();

            mainView.camera.data->position.x = 0;
            mainView.camera.data->position.y = 0;
            mainView.camera.data->position.z = 0;

            mainView.camera.pitch = 0.0f;
            mainView.camera.yaw = 0.0f;

            glm::vec3 direction;
            direction.x = cos(glm::radians(mainView.camera.yaw)) * cos(glm::radians(mainView.camera.pitch));
            direction.y = sin(glm::radians(mainView.camera.pitch));
            direction.z = sin(glm::radians(mainView.camera.yaw)) * cos(glm::radians(mainView.camera.pitch));
            mainView.camera.front = glm::normalize(direction);
            glm::vec3 eye = mainView.camera.data->position;
            mainView.camera.data->view = glm::lookAt(eye, eye + mainView.camera.front, { 0,1,0 });
            mainView.camera.data->proj = glm::perspective(glm::radians(60.0f),1440.0f/810.0f,0.1f,1000.0f);
            mainView.camera.data->proj[1][1] *= 1.0f;

            Window::registerMouseDragCallback([this](int button, double deltaX, double deltaY){
                if(button == GLFW_MOUSE_BUTTON_LEFT)
                {
                    mainView.camera.yaw += 0.1f * deltaX;
                    mainView.camera.pitch += 0.1f * deltaY;

                    glm::vec3 direction;
                    direction.x = cos(glm::radians(mainView.camera.yaw)) * cos(glm::radians(mainView.camera.pitch));
                    direction.y = sin(glm::radians(mainView.camera.pitch));
                    direction.z = sin(glm::radians(mainView.camera.yaw)) * cos(glm::radians(mainView.camera.pitch));

                    mainView.camera.front = glm::normalize(direction);
                    glm::vec3 eye = mainView.camera.data->position;
                    mainView.camera.data->view = glm::lookAt(eye, eye + mainView.camera.front, { 0,1,0 });
                }
            });

            Window::registerKeyCallback([this](int key, int scancode, int action, int mods) {
                bool hold = (action == GLFW_REPEAT || action == GLFW_PRESS);
                if (key == GLFW_KEY_W && hold)
                {
                    mainView.camera.data->position += 0.1f * glm::vec4(mainView.camera.front,0.0);
                }
                if (key == GLFW_KEY_S && hold)
                {
                    mainView.camera.data->position -= 0.1f * glm::vec4(mainView.camera.front, 0.0);
                }
                if (key == GLFW_KEY_A && hold)
                {
                    auto right = glm::normalize(glm::cross(mainView.camera.front, { 0,1,0 }));
                    mainView.camera.data->position -= 0.1f * glm::vec4(right, 0.0);
                }
                if (key == GLFW_KEY_D && hold)
                {
                    auto right = glm::normalize(glm::cross(mainView.camera.front, { 0,1,0 }));
                    mainView.camera.data->position += 0.1f * glm::vec4(right, 0.0);
                }

                glm::vec3 eye = mainView.camera.data->position;
                mainView.camera.data->view = glm::lookAt(eye, eye + mainView.camera.front, { 0,1,0 });
            });
        }
    }

    void RenderScene::buildFrom(SceneGraphNode *root, AssetManager &assetManager)
    {
        auto visitor = [this, &assetManager](SceneGraphNode* node)->void{
            if(!node->shapes.empty())
            {
                for(auto shape : node->shapes)
                {
                    std::string shape_uuid;
                    if(typeid(*shape) == typeid(PLYMeshShape)) {
                        shape_uuid = dynamic_cast<PLYMeshShape *>(shape)->filename;
                    }

                    MeshRigidHandle meshHandle;

                    for(int i = 0; i < meshes.size(); i ++)
                    {
                        if(meshes[i].first == shape_uuid)
                        {
                            meshHandle.scene = this;
                            meshHandle.idx = i;
                            break;
                        }
                    }

                    if(meshHandle.scene == nullptr)
                    {
                        // Existing mesh not found
                        MeshHostObject* meshHost = assetManager.getOrLoadPBRTPLY(shape_uuid);

                        auto interleaveAttribute = meshHost->getInterleavingAttributes();

                        unsigned int * indicies = meshHost->indices.get();
                        auto indexBufferSize = meshHost->index_count * sizeof(indicies[0]);
                        auto vertexBufferSize = meshHost->vertex_count * interleaveAttribute.second.VertexStride;
                        auto interleavingBufferAttribute = interleaveAttribute.second;

                        auto vertexBuffer = backendDevice->allocateBuffer(vertexBufferSize,(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
                        auto indexBuffer = backendDevice->allocateBuffer(indexBufferSize,(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

                        if(!vertexBuffer)
                        {
                            throw std::runtime_error("Failed to allocate vertex buffer");
                        }

                        if(!indexBuffer)
                        {
                            throw std::runtime_error("Failed to allocate index buffer");
                        }

                        MeshRigid::VertexAttribute vertexAttribute;
                        vertexAttribute.stride = interleavingBufferAttribute.VertexStride;
                        vertexAttribute.normalOffset = interleavingBufferAttribute.normalOffset;
                        vertexAttribute.tangentOffset = interleavingBufferAttribute.tangentOffset;
                        vertexAttribute.biTangentOffset = interleavingBufferAttribute.biTangentOffset;
                        vertexAttribute.uvOffset = interleavingBufferAttribute.uvOffset;

                        MeshRigid meshRigid{vertexAttribute,static_cast<uint32_t>(meshes.size())};

                        meshRigid.vertexBuffer = vertexBuffer.value();
                        meshRigid.indexBuffer = indexBuffer.value();
                        meshRigid.vertexCount = meshHost->vertex_count;
                        meshRigid.indexCount = meshHost->index_count;

                        backendDevice->oneTimeUploadSync(meshHost->indices.get(),indexBufferSize,meshRigid.indexBuffer.buffer);
                        backendDevice->oneTimeUploadSync(interleaveAttribute.first,vertexBufferSize,meshRigid.vertexBuffer.buffer);

                        meshes.emplace_back(shape_uuid,meshRigid);
                        AABB aabb{};
                        aabb.minX = meshHost->aabb[0]; aabb.minY = meshHost->aabb[1]; aabb.minZ = meshHost->aabb[2];
                        aabb.maxX = meshHost->aabb[3]; aabb.maxY = meshHost->aabb[4]; aabb.maxZ = meshHost->aabb[5];
                        aabbs.emplace_back(aabb);
                        meshHandle.scene = this;
                        meshHandle.idx = meshes.size() - 1;
                    }

                    bool foundInstance = false;

                    for(auto & inst : _dynamicRigidMeshBatch)
                    {
                        if(inst.mesh == meshHandle)
                        {
                            foundInstance = true;
                            inst.perInstanceData.push_back({node->_finalTransform});
                            break;
                        }
                    }

                    if(!foundInstance)
                    {
                        InstanceBatchRigidDynamic<PerInstanceData> meshInstanceRigidDynamic(meshHandle);
                        meshInstanceRigidDynamic.perInstanceData.push_back({node->_finalTransform});
                        _dynamicRigidMeshBatch.push_back(meshInstanceRigidDynamic);
                    }
                }
            }
        };

        root->visit(visitor);

        /*
         * From a data-oriented view, all entities in renderScene is data, instead of object.
         * Which means, they shouldn't have the behavior of create, update the state of themselves.
         *
         * For example, if one wants to update the transformation of a meshInstance. Instead of get the object
         * the call something like setTransform(newTransform), one should use call the functionalities provided by renderScene,
         * like renderScene::setTransform(instance identifier, newTransform).
         *
         * One reason is, if you directly manipulate the data, you lose the chance to let renderScene knows what happen
         * which otherwise may allow some optimization like batch operation. Of course, with object pattern you can still
         * do that, by call something like scene::addUploadRequest() inside the function. But the question is why you
         * bother to do that: if you ultimately want to notify the manager, would it be more intuitive and convenience
         * to directly do so?
         *
         * In other word, all the manipulation of data must go through the subsystem, or manager.
         */
    }

}
