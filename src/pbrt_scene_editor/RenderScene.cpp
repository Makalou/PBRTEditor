//
// Created by 王泽远 on 2024/1/6.
//

#include "RenderScene.h"
#include "SceneBuilder.hpp"

namespace renderScene
{
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
            float aspect = 1440.0f / 810.0f;
            mainView.camera.data->proj = glm::perspective(glm::radians(60.0f),aspect,0.1f,1000.0f);
            mainView.camera.data->proj[1][1] *= -1.0f;

            Window::registerMouseDragCallback([this](int button, double deltaX, double deltaY){
                if(button == GLFW_MOUSE_BUTTON_LEFT)
                {
                    mainView.camera.yaw += 0.1f * deltaX;
                    mainView.camera.pitch -= 0.1f * deltaY;

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

    void RenderScene::buildFrom(SceneGraph * sceneGraph, AssetManager &assetManager)
    {
        auto handleNode = [this, &assetManager](SceneGraphNode* node,const glm::mat4& baseTransform )->void{
            if(!node->shapes.empty())
            {
                for(int i = 0; i < node->shapes.size(); i++)
                {
                    auto shape = node->shapes[i];
                    std::string shape_uuid;
                    PLYMeshShape * plyMeshPtr = dynamic_cast<PLYMeshShape *>(shape);
                    if(plyMeshPtr != nullptr)
                    {
                        shape_uuid = dynamic_cast<PLYMeshShape *>(shape)->filename;
                    }else{
                        break;
                        throw std::runtime_error("Only support ply mesh for now");
                    }

                    MeshRigidHandle meshHandle;

                    for(const auto & mesh : meshes)
                    {
                        if(mesh.first == shape_uuid)
                        {
                            meshHandle = mesh.second;
                        }
                    }

                    if(meshHandle.idx == -1)
                    {
                        meshHandle = assetManager.getOrLoadPLYMeshDevice(shape_uuid);
                        AABB aabb{};
                        aabb.minX = meshHandle.hostObject->aabb[0]; aabb.minY = meshHandle.hostObject->aabb[1];
                        aabb.minZ = meshHandle.hostObject->aabb[2]; aabb.maxX = meshHandle.hostObject->aabb[3];
                        aabb.maxY = meshHandle.hostObject->aabb[4]; aabb.maxZ = meshHandle.hostObject->aabb[5];
                        aabbs.emplace_back(aabb);
                    }

                    Material* mat;
                    do{
                        mat = node->materials[i];
                        auto * coatedDiffuse = dynamic_cast<CoatedDiffuseMaterial*>(mat);
                        if(coatedDiffuse != nullptr)
                        {
                            break;
                        }
                    } while (0);

                    // For now we assume the instance with same mesh also share same material.
                    bool foundInstance = false;
                    for(auto & inst : _dynamicRigidMeshBatch)
                    {
                        if(inst.mesh == meshHandle)
                        {
                            assert(mat->name == inst.materialName);
                            foundInstance = true;
                            inst.perInstanceData.push_back({node->_finalTransform * baseTransform});
                            break;
                        }
                    }

                    if(!foundInstance)
                    {
                        InstanceBatchRigidDynamic<PerInstanceData> meshInstanceRigidDynamic(meshHandle);
                        meshInstanceRigidDynamic.materialName = mat->name;
                        meshInstanceRigidDynamic.perInstanceData.push_back({node->_finalTransform * baseTransform});
                        _dynamicRigidMeshBatch.push_back(meshInstanceRigidDynamic);
                    }
                }
            }
        };

        std::vector<SceneGraphNode *> stack;
        stack.push_back(sceneGraph->root);

        while(!stack.empty())
        {
            auto * node = stack.back();
            stack.pop_back();
            for(auto * child : node->children)
            {
                if(child->is_instance)
                {
                    std::vector<SceneGraphNode *> stack2;
                    stack2.push_back(child);

                    while(!stack2.empty())
                    {
                        auto * node2 = stack2.back();
                        stack2.pop_back();
                        for(auto * child2 : node2->children)
                        {
                            stack2.push_back(child2);
                        }
                        handleNode(node2,node->_finalTransform);
                    }
                }
                else {
                    stack.push_back(child);
                }
            }
            handleNode(node,glm::identity<glm::mat4>());
        }

        for(Texture * tex : sceneGraph->namedTextures)
        {
            if(dynamic_cast<ImageMapTexture*>(tex)!= nullptr)
            {
                ImageMapTexture* image = dynamic_cast<ImageMapTexture*>(tex);
                assetManager.getOrLoadImgDevice(image->filename,image->encoding);
            }
        }
        /*
         * From a data-oriented view, all entities in renderScene is data, instead of object.
         * Which means, they shouldn't have the behavior of create, update the state of themselves.
         *
         * For example, if one wants to update the transformation of a meshInstance. Instead of get the object
         * the call something like setTransform(newTransform), one should use call the functionalities provided by renderScene,
         * like renderScene::setTransform(instance identifier, newTransform).
         *
         * One reason is, if you directly manipulate the data, you lose the chance to let renderScene knows what happen
         * which otherwise may allow some optimization like batch operation. Of course, with object oriented pattern you can still
         * do that, by call something like scene::addUploadRequest() inside the function. But the question is why you
         * bother to do that: if you ultimately want to notify the manager, would it be more intuitive and convenience
         * to directly do so?
         *
         * In other word, all the manipulation of data must go through the subsystem, or manager.
         */
        vk::DescriptorSetLayoutBinding binding1{};
        binding1.setBinding(0);
        binding1.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        binding1.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        binding1.setDescriptorCount(1);
        perInstanceDataSetLayout = backendDevice->createDescriptorSetLayout2({binding1});

        vk::DescriptorPoolCreateInfo poolCreateInfo{};
        vk::DescriptorPoolSize poolSize{};
        poolSize.setType(vk::DescriptorType::eStorageBuffer);
        poolSize.setDescriptorCount(_dynamicRigidMeshBatch.size());
        poolCreateInfo.setPoolSizes(poolSize);
        poolCreateInfo.setMaxSets(_dynamicRigidMeshBatch.size());
        perInstanceDataDescriptorPool = backendDevice->createDescriptorPool(poolCreateInfo);

        vk::DescriptorSetLayoutBinding materialAlbedoBinding{};
        materialAlbedoBinding.setBinding(0);
        binding1.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        binding1.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        binding1.setDescriptorCount(1);
        materialLayout = backendDevice->createDescriptorSetLayout2({materialAlbedoBinding});

        vk::DescriptorPoolCreateInfo poolCreateInfo1{};
        vk::DescriptorPoolSize poolSize1{};
        poolSize1.setType(vk::DescriptorType::eStorageBuffer);
        poolSize1.setDescriptorCount(_dynamicRigidMeshBatch.size());
        poolCreateInfo1.setPoolSizes(poolSize1);
        poolCreateInfo1.setMaxSets(_dynamicRigidMeshBatch.size());
        materialDescriptorPool = backendDevice->createDescriptorPool(poolCreateInfo1);

        for (auto& dynamicInstance : _dynamicRigidMeshBatch)
        {
            dynamicInstance.prepare(backendDevice.get());
            auto descriptorSet = backendDevice->allocateSingleDescriptorSet(perInstanceDataDescriptorPool,perInstanceDataSetLayout);
            dynamicInstance.perInstDataDescriptorLayout = perInstanceDataSetLayout;
            dynamicInstance.perInstDataDescriptorSet = descriptorSet;
            backendDevice->updateDescriptorSetStorageBuffer(descriptorSet,0,dynamicInstance.perInstDataBuffer.buffer);
        }
    }

}
