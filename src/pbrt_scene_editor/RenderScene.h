//
// Created by 王泽远 on 2024/1/6.
//

#ifndef PBRTEDITOR_RENDERSCENE_H
#define PBRTEDITOR_RENDERSCENE_H

#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace renderScene {
    struct MeshRigid {
        vk::Buffer vertexBuffer{};
        vk::Buffer indexBuffer{};
        uint32_t vertexCount{};
        uint32_t indexCount{};

        void bind(vk::CommandBuffer cmd) {
            cmd.bindVertexBuffers(0, 1, &vertexBuffer, nullptr);
            cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
        }
    };

/*
 * Static rigid mesh would never change its geometry or per instance data.
 * Such Mesh doesn't have to multiply the resource for multiple frame buffering.
 * And it also has good chance to cache the draw command buffer, the only thing need to take care is culling
 * It's suitable for all the static instances share the same material setting.
 * */
    template<class PerInstDataT>
    struct MeshInstanceRigidStatic {

        void draw(vk::CommandBuffer cmd) {
            if (!cmd_valid) {
                record();
            }
            cmd.executeCommands(_secondaryCMD);
            cmd_valid = true;
        }

        void record() {
            vk::PipelineLayout layout;
            vk::DescriptorSet set;
            _secondaryCMD.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, set, nullptr);
            mesh->bind(_secondaryCMD);
            _secondaryCMD.drawIndexed(mesh->indexCount, instanceCount, 0, 0, 0);
        }

        std::vector<PerInstDataT> perInstanceData;
        std::vector<bool> visibilities;
        std::vector<bool> visibilities_old;
        bool cmd_valid{};
        MeshRigid *mesh{};
        size_t instanceCount{};
        vk::CommandBuffer _secondaryCMD;
    };


/*
 * Dynamic rigid mesh would never change its geometry, but the change of other per instance data
 * (transformation e.t. )is allowed.
 * The vertex and index buffer don't need to be multiplied, while the per instance data buffer needed.
 * Suitable for rigid dynamic object.
 * */
    template<class PerInstDataT>
    struct MeshInstanceRigidDynamic {

        void draw(vk::CommandBuffer primaryCMD) {
            if (!cmd_valid) {
                record();
            }
            primaryCMD.executeCommands(_secondaryCMD);
            cmd_valid = true;
        }

        void record() {
            vk::PipelineLayout layout;
            vk::DescriptorSet set;
            _secondaryCMD.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, set, nullptr);
            mesh->bind(_secondaryCMD);
            _secondaryCMD.drawIndexed(mesh->indexCount, instanceCount, 0, 0, 0);
        }

        void update(const PerInstDataT *newData, const uint32_t *InstanceIDs, uint32_t count) {
            for (int i = 0; i < count; i++) {
                perInstanceData[InstanceIDs[i]] = newData[i];
            }
            cmd_valid = false;
        }

        std::vector<PerInstDataT> perInstanceData;
        MeshRigid *mesh{};
        size_t instanceCount{};
        std::vector<bool> visibilities;
        std::vector<bool> visibilities_old;
        vk::CommandBuffer _secondaryCMD;
        bool cmd_valid{};
    };

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

        std::array<std::vector<PerInstDataT>, 0> perInstanceData;
        std::vector<uint> matInstanceIDs;

        MeshRigid *mesh{};
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
        std::vector<uint> matInstanceIDs;

        VkBuffer vertexBuffer{};
        VkBuffer indexBuffer{};
    };

/*
 * Deformable mesh may change its geometry topology at every frame.
 * Such as Skinned animator, physic simulation animator.
 * We use compute shader to directly generate deformed vertex buffer,
 * instead of doing so in vertex shader.
 * */
    struct MeshDeformable {
        vk::Buffer vertexBufferOriginal;
        vk::Buffer indexBufferOriginal;

        vk::Buffer vertexBufferCurrent;
        vk::Buffer indexBufferCurrent;

        uint32_t vertexCount{};
        uint32_t indexCount{};

        void update(vk::CommandBuffer computeCMD,float time) {

        }

        void draw(vk::CommandBuffer primaryCMD) {
            vk::PipelineLayout layout;
            vk::DescriptorSet set;
            primaryCMD.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, set, nullptr);
            primaryCMD.bindVertexBuffers(0, 1, &vertexBufferCurrent, nullptr);
            primaryCMD.bindIndexBuffer(indexBufferCurrent, 0, vk::IndexType::eUint32);
            primaryCMD.drawIndexed(indexCount,1, 0, 0, 0);
        }
    };

/*
 * Deformable mesh instance only make sense when using vertex animation
 * (Each instance deform the primitives during vertex shader, using
 * per instance information)
 * */
    struct MeshInstanceDeformable {

        MeshRigid *mesh; // the deformed information is encoded in per instance data
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

    struct Camera {

    };

    struct RenderScene {
        std::vector<MeshInstanceRigidStatic<PerInstanceData>> _staticRigidMeshInstances;
        std::vector<MeshInstanceRigidDynamic<PerInstanceData>> _dynamicRigidMeshInstances;
        std::vector<MeshDeformable> _deformableMeshes;

        std::vector<PointLight> pointLights;
        std::vector<DistantLight> distantLights;
        std::vector<SpotLight> spotLights;
        std::vector<InfiniteLight> infiniteLights;
        std::vector<GoniometricLight> goniometricLights;
        std::vector<ProjectionLight> projectionLights;

        Camera* cam;

        void renderGeometryPass(vk::CommandBuffer primaryGraphicsCMD,vk::CommandBuffer primaryComputeCMD,float time)
        {
            //todo update all mesh instances
            for(auto & staticRigidInstance : _staticRigidMeshInstances)
            {
                staticRigidInstance.draw(primaryGraphicsCMD);
            }

            for(auto & dynamicRigidInstance : _dynamicRigidMeshInstances)
            {
                dynamicRigidInstance.draw(primaryGraphicsCMD);
            }

            for(auto & deformable : _deformableMeshes){
                deformable.update(primaryComputeCMD,time); // need compute command
            }

            for(auto & deformable : _deformableMeshes){
                deformable.draw(primaryGraphicsCMD);
            }
        }

        void renderShadowPass(vk::CommandBuffer primaryCMD)
        {

        }

        void renderDeferredLightingPass(vk::CommandBuffer primaryCMD)
        {

        }
    };

    struct RayTracingScene {

    };
}
#endif //PBRTEDITOR_RENDERSCENE_H
