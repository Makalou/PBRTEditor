//
// Created by 王泽远 on 2024/1/6.
//

#ifndef PBRTEDITOR_RENDERSCENE_H
#define PBRTEDITOR_RENDERSCENE_H

#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sceneGraphEditor.hpp"
#include "VulkanExtension.h"
#include "AssetManager.hpp"
#include "window.h"
#include <glm/gtc/matrix_transform.hpp>

namespace renderScene {

    struct RenderScene;

    struct InstanceUUID
    {
        uint32_t low = 0;
        uint32_t high = 0;

        bool operator==(const InstanceUUID& other) const
        {
            return other.low == low && other.high == high;
        }
    };

    struct InstanceUUIDHash {
        std::size_t operator()(const InstanceUUID& uuid) const {
            std::size_t hashLow = std::hash<int>{}(uuid.low);
            std::size_t hashHigh = std::hash<int>{}(uuid.high);
            return hashLow ^ (hashHigh << 1);
        }
    };
    /*
     * Static rigid mesh would never change its geometry or per instance data.
     * Such Mesh doesn't have to multiply the resource for multiple frame buffering.
     * And it also has good chance to cache the draw command buffer, the only thing need to take care is culling
     * It's suitable for all the static instances share the same material setting.
     * */
    template<class PerInstDataT>
    struct InstanceBatchRigidStatic {

        void draw(vk::CommandBuffer cmd) {

        }

        std::vector<PerInstDataT> perInstanceData;
        std::vector<uint32_t> instanceDataIdx;
        MeshRigidDevice *mesh{};
        size_t drawInstanceCount{};

        VMABuffer perInstDataBuffer{};
    };

    /*
     * Dynamic rigid mesh would never change its geometry, but the change of other per instance data
     * (transformation e.t. )is allowed.
     * The vertex and index buffer don't need to be multiplied, while the per instance data buffer needed.
     * Suitable for rigid dynamic object.
     * */
    template<class PerInstDataT>
    struct InstanceBatchRigidDynamic {

        auto initPipelineVertexInputInfo(MeshRigidHandle meshHandle)
        {
            vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
            auto perVertexBindingDesc = meshHandle->getVertexInputBindingDesc();
            auto perVertexAttributeDesc = meshHandle->getVertexInputAttributeDesc();

            vk::VertexInputBindingDescription perInstanceBindingDesc{};
            perInstanceBindingDesc.setInputRate(vk::VertexInputRate::eInstance);
            perInstanceBindingIdx = perVertexBindingDesc.binding + 1;
            perInstanceBindingDesc.setBinding(perInstanceBindingIdx);
            perInstanceBindingDesc.setStride(sizeof(uint32_t));//todo wrong way to calculate stride

            vk::VertexInputAttributeDescription perInstanceAttributeDescription{};
            perInstanceAttributeDescription.setBinding(perInstanceBindingDesc.binding);
            perInstanceAttributeDescription.setOffset(0);
            perInstanceAttributeDescription.setLocation(5);
            // todo if we can modify the shader variant, then explicitly specific location may be unnecessary
            perInstanceAttributeDescription.setFormat(vk::Format::eR32Uint);

            std::vector<vk::VertexInputBindingDescription> bindings{perVertexBindingDesc,perInstanceBindingDesc};
            perVertexAttributeDesc.push_back(perInstanceAttributeDescription);// misleading, but efficient ...
            vertexInputStateCreateInfo.setPVertexBindingDescriptions(bindings.data());
            vertexInputStateCreateInfo.setVertexBindingDescriptionCount(bindings.size());
            vertexInputStateCreateInfo.setPVertexAttributeDescriptions(perVertexAttributeDesc.data());
            vertexInputStateCreateInfo.setVertexAttributeDescriptionCount(perVertexAttributeDesc.size());

            return VulkanPipelineVertexInputStateInfo(vertexInputStateCreateInfo);
        }

        explicit InstanceBatchRigidDynamic(MeshRigidHandle handle) : mesh(handle),
                    pipelineVertexInputStateInfo(initPipelineVertexInputInfo(handle))
        {
            _uuid.high = mesh->_uuid;
        }

        void prepare(DeviceExtended * device)
        {
            auto instanceDataBufferSize = sizeof(PerInstDataT) * perInstanceData.size();
            auto bufferRes = device->allocateBuffer(instanceDataBufferSize, 
                (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), 
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

            if (!bufferRes.has_value())
            {
                throw std::runtime_error("Failed to allocate instance data buffer");
            }

            perInstDataBuffer = bufferRes.value();
            device->oneTimeUploadSync(perInstanceData.data(), instanceDataBufferSize, perInstDataBuffer.buffer);

            for (int i = 0; i < perInstanceData.size(); i++)
            {
                instanceDataIdices.push_back(i);
            }

            bufferRes = device->allocateBuffer(sizeof(uint32_t) * perInstanceData.size(),
                (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

            if (!bufferRes.has_value())
            {
                throw std::runtime_error("Failed to allocate instance indies buffer");
            }

            instanceDataIdicesBuffer = bufferRes.value();
            device->oneTimeUploadSync(instanceDataIdices.data(), sizeof(uint32_t) * perInstanceData.size(), instanceDataIdicesBuffer.buffer);

            bufferRes = device->allocateBuffer(sizeof(uint32_t) * perInstanceData.size(),
                (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

            if (!bufferRes.has_value())
            {
                throw std::runtime_error("Failed to allocate instance mask indies buffer");
            }

            instanceMaskedIdicesBuffer = bufferRes.value();
        }

        VkBuffer getInstanceDataBuffer() const
        {
            return perInstDataBuffer.buffer;
        }

        vk::DescriptorSetLayout getSetLayout() const
        {
            return perInstDataDescriptorLayout;
        }

        vk::DescriptorSet getDescriptorSet() const
        {
            return perInstDataDescriptorSet;
        }

        void drawAll(vk::CommandBuffer cmd) const{
            mesh->bind(cmd);
            //bind per instance data
            cmd.bindVertexBuffers(1, { instanceDataIdicesBuffer.buffer }, {0});
            cmd.drawIndexed(mesh->indexCount, perInstanceData.size(), 0, 0, 0);
        }

        void drawAllPosOnly(vk::CommandBuffer cmd,vk::DispatchLoaderDynamic loader) const {
            mesh->bindPosOnly(cmd,loader);

            //bind per instance data
            cmd.bindVertexBuffers2EXT(1, { instanceDataIdicesBuffer.buffer }, { 0 }, nullptr, { sizeof(uint32_t) },loader);
            cmd.drawIndexed(mesh->indexCount, perInstanceData.size(), 0, 0, 0);
        }

        uint32_t updateCurrentMask(uint32_t _mask,DeviceExtended* device)
        {
            std::vector<uint32_t> collectMaskedIndices;
            for (int i = 0; i < mask.size(); i++)
            {
                if (mask[i] == _mask)
                {
                    collectMaskedIndices.push_back(i);
                }
            }
            if (collectMaskedIndices.empty())
            {
                collectedMaskedCache.clear();
                return 0;
            }

            if (collectMaskedIndices.size() > collectedMaskedCache.size())
            {
                collectedMaskedCache = collectMaskedIndices;
                device->oneTimeUploadSync(collectedMaskedCache.data(), sizeof(uint32_t) * collectedMaskedCache.size(), instanceMaskedIdicesBuffer.buffer);
                return collectedMaskedCache.size();
            }

            bool consistence = true;
            for (int i = 0; i < collectMaskedIndices.size(); i++)
            {
                if (collectMaskedIndices[i] != collectedMaskedCache[i])
                {
                    consistence = false;
                    break;
                }
            }

            if (consistence)
            {
                collectedMaskedCache.resize(collectMaskedIndices.size());
                return collectedMaskedCache.size();
            }

            collectedMaskedCache = collectMaskedIndices;
            device->oneTimeUploadSync(collectedMaskedCache.data(), sizeof(uint32_t) * collectedMaskedCache.size(), instanceMaskedIdicesBuffer.buffer);
            return collectedMaskedCache.size();
        }

        void drawAllPosOnlyMasked(vk::CommandBuffer cmd,vk::DispatchLoaderDynamic loader) const {
            if (collectedMaskedCache.empty()) return;

            mesh->bindPosOnly(cmd,loader);
           
            //bind per instance data
            cmd.bindVertexBuffers2EXT(1, { instanceMaskedIdicesBuffer.buffer }, { 0 }, nullptr, { sizeof(uint32_t) },loader);
            cmd.drawIndexed(mesh->indexCount, collectedMaskedCache.size(), 0, 0, 0);
        }

        void drawOne(vk::CommandBuffer cmd) const{
            mesh->bind(cmd);
            //todo bind per instance data
            cmd.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
        }

        uint32_t drawCulled( vk::CommandBuffer cmd, const std::function<bool(MeshRigidHandle meshHandle, const PerInstDataT &)> & cull) const{
            uint32_t visibleInstanceCount = 0;
            for(int i = 0; i < perInstanceData.size(); i ++)
            {
                if(cull(mesh,perInstanceData[i]))
                {
                    visibleInstanceCount++;
                }
            }

            if(visibleInstanceCount > 0)
            {
                mesh->bind(cmd);
                //todo bind per instance data
                cmd.drawIndexed(mesh->indexCount, visibleInstanceCount, 0, 0, 0);
            }

            return visibleInstanceCount;
        }

        /*
         * Update visibility is a heavy and stateful operation.
         * When calling update visibility, instance data buffer will be reorder
         * to put all the invisible instances to the end of array. and update the
         * GPU buffer
         */
        void updateVisibility()
        {
            //todo
        }

        auto getVertexInputState() const
        {
            return pipelineVertexInputStateInfo;
        }

        static auto getPosOnlyVertexInputState()
        {
            vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
            vk::VertexInputBindingDescription perVertexBindingDesc{};
            perVertexBindingDesc.setInputRate(vk::VertexInputRate::eVertex);
            perVertexBindingDesc.setBinding(0);
            //Stride is ignored
            vk::VertexInputAttributeDescription perVertexPosAttributeDesc;
            perVertexPosAttributeDesc.setBinding(0);
            perVertexPosAttributeDesc.setBinding(0);
            perVertexPosAttributeDesc.setOffset(0);
            perVertexPosAttributeDesc.setFormat(vk::Format::eR32G32B32Sfloat);

            vk::VertexInputBindingDescription perInstanceBindingDesc{};
            perInstanceBindingDesc.setInputRate(vk::VertexInputRate::eInstance);
            perInstanceBindingDesc.setBinding(1);
            //perInstanceBindingDesc.setStride(sizeof(uint32_t));//todo wrong way to calculate stride
            //Stride is ignored
            vk::VertexInputAttributeDescription perInstanceAttributeDesc{};
            perInstanceAttributeDesc.setBinding(perInstanceBindingDesc.binding);
            perInstanceAttributeDesc.setOffset(0);
            perInstanceAttributeDesc.setLocation(1);
            perInstanceAttributeDesc.setFormat(vk::Format::eR32Uint);

            std::vector<vk::VertexInputBindingDescription> bindings{ perVertexBindingDesc,perInstanceBindingDesc };
            std::vector<vk::VertexInputAttributeDescription> attributes{ perVertexPosAttributeDesc,perInstanceAttributeDesc };
            vertexInputStateCreateInfo.setVertexBindingDescriptions(bindings);
            vertexInputStateCreateInfo.setVertexAttributeDescriptions(attributes);

            return VulkanPipelineVertexInputStateInfo(vertexInputStateCreateInfo);
        }

        inline InstanceUUID getUUID() const
        {
            return _uuid;
        }

        std::vector<PerInstDataT> perInstanceData;
        std::vector<uint32_t> instanceDataIdices;
        std::vector<uint32_t> mask;

        std::vector<uint32_t> collectedMaskedCache;

        int perInstanceBindingIdx;
        MeshRigidHandle mesh;
        TextureDeviceHandle texture;
        const VulkanPipelineVertexInputStateInfo pipelineVertexInputStateInfo{};
        VMABuffer perInstDataBuffer{};
        vk::DescriptorSetLayout perInstDataDescriptorLayout;
        vk::DescriptorSet perInstDataDescriptorSet;
        // Why we use secondary indirect instance data indexing?
        // Short answer : for better support on culling.
        //https://app.diagrams.net/#G1ei8XsclhGNg_qMR_J7LBKmGRjSSXyShI#%7B%22pageId%22%3A%22sedBS7P0nTr2XQddadu8%22%7D
        VMABuffer instanceDataIdicesBuffer{};
        VMABuffer instanceMaskedIdicesBuffer{};
        InstanceUUID _uuid;
        std::string materialName;
    };

    //struct DrawDataBindless
    //{
    //    uint materialIndex;
    //    uint transformOffset;
    //    uint vertexOffset;
    //    uint unused0; // vec4 padding
    //    // ... extra gameplay data goes here
    //};

    /*
     * When bindless rendering is enabled, it's even possible to batch instances with same material category
     * to one drawInstance command. Each instance then use per instance material ID to fetch material instanced data,
     * (including the albedo maps, normal maps, e.t.). Still, any varying of data is not allowed.
     *
     * It's called partial bindless because we are still conservative : we only allow same material category, which implies,
     * They share same pipeline layout. A more aggressive total bindless rendering even allow to group all the geometry and
     * material from the whole scene together. This functionality, it's somehow indispensable for serious ray tracing.
     *
     * */
    template<class PerInstDataT>
    struct MeshInstanceRigidStaticBindlessPartial {

        std::vector<PerInstDataT> perInstanceData;
        std::vector<uint32_t> matInstanceIDs;

        int32_t meshIdx{};
    };


    /*
     * Dynamic rigid mesh would never change its geometry, but the change of other per instance data
     * (transformation e.t. )is allowed.
     * The vertex and index buffer don't need to be multiplied, while the per instance data buffer needed.
     * And it seems not worth to cache the draw command.
     * Suitable for rigid dynamic object.
     * */
    template<class PerInstDataT>
    struct MeshInstanceRigidDynamicBindlessPartial {

        std::vector<PerInstDataT> perInstanceData;
        std::vector<uint32_t> matInstanceIDs;

        VMABuffer vertexBuffer{};
        VMABuffer indexBuffer{};
    };

    /*
     * Deformable mesh may change its geometry topology at every frame.
     * Such as Skinned animator, physic simulation animator.
     * We use compute shader to directly generate deformed vertex buffer,
     * instead of doing so in vertex shader.
     * */
    struct MeshDeformable {
        VMABuffer vertexBufferOriginal;
        VMABuffer indexBufferOriginal;

        VMABuffer vertexBufferCurrent;
        VMABuffer indexBufferCurrent;

        uint32_t vertexCount{};
        uint32_t indexCount{};

        void update(vk::CommandBuffer computeCMD,float time) {

        }

        void draw(vk::CommandBuffer cmd) {

        }
    };

    /*
     * Deformable mesh instance only make sense when using vertex animation
     * (Each instance deform the primitives during vertex shader, using
     * per instance information)
     * */
    struct MeshInstanceDeformable {

        int32_t meshIdx; // the deformed information is encoded in per instance data
    };

    struct PerInstanceData {
        glm::mat4x4 _wTransform;
    };

    struct RenderScenePointLight {

    };

    struct RenderSceneDistantLight {

    };

    struct RenderSceneSpotLight {

    };

    struct RenderSceneInfiniteLight {

    };

    struct RenderSceneGoniometricLight{

    };

    struct RenderSceneProjectionLight{

    };

    struct RenderSceneAreaLight {
        MeshRigidDevice *mesh;
    };

    struct MainCameraData{
        glm::vec4 position;
        glm::vec4 target;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct MainCamera
    {
        double yaw{};
        double pitch{};
        glm::vec3 front{};
        VMAObservedBufferMapped<MainCameraData> data;
    };

    struct RenderScene;

    struct RenderView
    {
        RenderScene* scene;
        std::vector<int> instanceIDs;
        MainCamera camera;
    };

    struct InstanceData
    {
        int meshID;
    };

    struct AABB
    {
        float minX; float minY; float minZ;
        float maxX; float maxY; float maxZ;
    };

    using InstanceBatchRigidDynamicType = InstanceBatchRigidDynamic<PerInstanceData>;
    struct RenderScene {
        std::vector<std::pair<std::string,MeshRigidHandle>> meshes{}; //use file path as uuid
        std::vector<InstanceBatchRigidStatic<PerInstanceData>> _staticRigidMeshBatch{};
        std::vector<InstanceBatchRigidDynamicType> _dynamicRigidMeshBatch{};
        std::vector<MeshDeformable> _deformableMeshes{};

        std::vector<InstanceData> instances;

        std::vector<PointLight> pointLights{};
        std::vector<DistantLight> distantLights{};
        std::vector<SpotLight> spotLights{};
        std::vector<InfiniteLight> infiniteLights{};
        std::vector<GoniometricLight> goniometricLights{};
        std::vector<ProjectionLight> projectionLights{};

        std::vector<AABB> aabbs{};
        RenderView mainView;

        SceneGraph* m_sceneGraph;
        std::vector<std::pair<SceneGraphNode*, std::pair<int, int>>> _sceneGraphNodeDynamicRigidMeshBatchBindingTable;

        glm::uvec4 selectedDynamicRigidMeshID;

        /*
         * Is it a good idea to let render scene manage perInstanceData descriptorSet?
         *
         * Pros:
         *  1. Renderscene has the global information of instance buffer(layout, count)
         *  2. It's more intuitive, flexible, and simple
         * Cons:
         *  1. Renderscene doesn't have the complete knowledge of render context 'by design'. So
         *     instance buffer must exclude the whole descriptor set.(It's actually make some sense since
         *     per instance data has different frequency than other contexts).
         */
        vk::DescriptorSetLayout perInstanceDataSetLayout;
        vk::DescriptorPool perInstanceDataDescriptorPool;
        vk::DescriptorSetLayout materialLayout;
        vk::DescriptorPool materialDescriptorPool;

        std::vector<DeviceExtended::BufferCopy> uploadRequests;

        explicit RenderScene(const std::shared_ptr<DeviceExtended>& device);

        void buildFrom(SceneGraph* sceneGraph, AssetManager & assetManager);
        void handleNodeShapes(SceneGraphNode* node, const glm::mat4& instanceBaseTransform, AssetManager& assetManager);
        void handleNodeLights(SceneGraphNode* node, const glm::mat4& instanceBaseTransform, AssetManager& assetManager);
        void prepareGPUResource();

        std::shared_ptr<DeviceExtended> backendDevice;

        void forEachInstanceRigidDynamic(std::function<void(const InstanceBatchRigidDynamicType &)>&& visitor)
        {
            for(const auto & dynamicInstance : _dynamicRigidMeshBatch)
            {
                visitor(dynamicInstance);
            }
        }

        void update();
    };

    struct RayTracingScene {

    };
}
#endif //PBRTEDITOR_RENDERSCENE_H
