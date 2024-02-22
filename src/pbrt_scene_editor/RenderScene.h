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

    struct MeshRigid {
        VMABuffer vertexBuffer{};
        VMABuffer indexBuffer{};
        uint32_t vertexCount{};
        uint32_t indexCount{};
        uint32_t _uuid;

        struct VertexAttribute
        {
            int stride = 0;
            int normalOffset = -1;
            int tangentOffset = -1;
            int biTangentOffset = -1;
            int uvOffset = -1;
        };

        const VertexAttribute vertexAttribute;
        const vk::VertexInputBindingDescription bindingDescription{};
        const std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

        auto initBindingDesc(const VertexAttribute& attribute) const
        {
            vk::VertexInputBindingDescription bindingDesc;
            bindingDesc.setInputRate(vk::VertexInputRate::eVertex);
            bindingDesc.setBinding(0);
            bindingDesc.setStride(vertexAttribute.stride);
            return bindingDesc;
        }

        auto initAttributeDescription(const VertexAttribute& attribute)
        {
            std::vector<vk::VertexInputAttributeDescription> attributeDesc;
            // todo if we can modify the shader variant, then explicitly specific location may be unnecessary
            attributeDesc.emplace_back(0,0,vk::Format::eR32G32B32Sfloat,0);
            if(vertexAttribute.normalOffset != -1)
            {
                attributeDesc.emplace_back(1,0,vk::Format::eR32G32B32Sfloat,vertexAttribute.normalOffset);
            }
            if(vertexAttribute.tangentOffset != -1)
            {
                attributeDesc.emplace_back(2,0,vk::Format::eR32G32B32Sfloat,vertexAttribute.tangentOffset);
            }
            if(vertexAttribute.biTangentOffset != -1)
            {
                attributeDesc.emplace_back(3,0,vk::Format::eR32G32B32Sfloat,vertexAttribute.biTangentOffset);
            }
            if(vertexAttribute.uvOffset != -1)
            {
                attributeDesc.emplace_back(4,0,vk::Format::eR32G32Sfloat,vertexAttribute.uvOffset);
            }

            return std::move(attributeDesc);
        }

        explicit MeshRigid(const VertexAttribute& attribute,uint32_t uuid) : vertexAttribute(attribute),
                                                                bindingDescription(initBindingDesc(attribute)),
                                                                attributeDescriptions(initAttributeDescription(attribute)),
                                                                _uuid(uuid){}

        /*
         * Only contains per vertex information.
         */
        auto getVertexInputBindingDesc() const
        {
            return bindingDescription;
        }

        /*
         * Only contains per vertex information.
         */
        auto getVertexInputAttributeDesc() const
        {
            return attributeDescriptions;
        }

        /*
         * Only bind per vertex data input
         */
        void bind(vk::CommandBuffer cmd) {
            cmd.bindVertexBuffers(0, {vertexBuffer.buffer}, {0});
            cmd.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
        }
    };

    struct RenderScene;

    struct MeshRigidHandle
    {
        RenderScene* scene = nullptr;
        uint32_t idx = -1;

        MeshRigid* operator->() const;

        bool operator==(const MeshRigidHandle& other) const;
    };

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
        MeshRigid *mesh{};
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
                (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), 
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
                throw std::runtime_error("Failed to allocate instance data buffer");
            }

            instanceDataIdicesBuffer = bufferRes.value();
            device->oneTimeUploadSync(instanceDataIdices.data(), sizeof(uint32_t) * perInstanceData.size(), instanceDataIdicesBuffer.buffer);
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
            //cmd.bindVertexBuffers(perInstanceBindingIdx,instDataIdxBuffer)
            cmd.drawIndexed(mesh->indexCount, perInstanceData.size(), 0, 0, 0);
        }

        void drawOne(vk::CommandBuffer cmd) const{
            mesh->bind(cmd);
            //todo bind per instance data
            //cmd.bindVertexBuffers(perInstanceBindingIdx,instDataIdxBuffer)
            //cmd.drawIndexed(mesh->vertexCount,1,0,0);
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

        inline InstanceUUID getUUID() const
        {
            return _uuid;
        }

        std::vector<PerInstDataT> perInstanceData;
        std::vector<uint64_t> instanceDataIdices;

        int perInstanceBindingIdx;
        MeshRigidHandle mesh;
        const VulkanPipelineVertexInputStateInfo pipelineVertexInputStateInfo{};
        VMABuffer perInstDataBuffer{};
        vk::DescriptorSetLayout perInstDataDescriptorLayout;
        vk::DescriptorSet perInstDataDescriptorSet;
        // Why we use secondary indirect instance data indexing?
        // Short answer : for better support on culling.
        //https://app.diagrams.net/#G1ei8XsclhGNg_qMR_J7LBKmGRjSSXyShI#%7B%22pageId%22%3A%22sedBS7P0nTr2XQddadu8%22%7D
        VMABuffer instanceDataIdicesBuffer{};
        InstanceUUID _uuid;
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

    struct PointLight {

    };

    struct DistantLight {

    };

    struct SpotLight {

    };

    struct InfiniteLight {

    };

    struct GoniometricLight{

    };

    struct ProjectionLight{

    };

    struct AreaLight {
        MeshRigid *mesh;
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
        std::vector<std::pair<std::string,MeshRigid>> meshes{}; //use file path as uuid
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

        explicit RenderScene(const std::shared_ptr<DeviceExtended>& device);

        void buildFrom(SceneGraphNode* root, AssetManager & assetManager);

        std::shared_ptr<DeviceExtended> backendDevice;

        void forEachInstanceRigidDynamic(std::function<void(const InstanceBatchRigidDynamicType &)>&& visitor)
        {
            for(const auto & dynamicInstance : _dynamicRigidMeshBatch)
            {
                visitor(dynamicInstance);
            }
        }
    };

    struct RayTracingScene {

    };
}
#endif //PBRTEDITOR_RENDERSCENE_H
