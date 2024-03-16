#pragma once

#include <filesystem>
#include <unordered_map>
#include <future>
#include "LockFreeCircleQueue.hpp"
#include <utility>
#include <variant>
#include "assimp/Importer.hpp"
#include "ThreadPool.h"
#include "stb_image.h"
#include "VulkanExtension.h"
#include <cstdlib>

struct AssetManager;

struct TextureHostObject
{
    unsigned int channels = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    struct STBIMGDeleter {
        void operator()(unsigned char* p) const {
            if(p!= nullptr) stbi_image_free(p);
        }
    };
    std::unique_ptr<unsigned char[],STBIMGDeleter> data = nullptr;
};

struct MeshHostObject
{
    unsigned int index_count = 0;
    std::unique_ptr<unsigned int[]> indices = nullptr;
    unsigned int vertex_count = 0;
    std::unique_ptr<float[]> position = nullptr;
    float aabb[6]{};
    //optional below
    std::unique_ptr<float[]> normal = nullptr;
    std::unique_ptr<float[]> tangent = nullptr;
    std::unique_ptr<float[]> bitangent = nullptr;
    std::unique_ptr<float[]> uv = nullptr;

    struct AttributeLayout
    {
        int VertexStride{};
        int normalOffset = -1;
        int tangentOffset = -1;
        int biTangentOffset = -1;
        int uvOffset = -1;
    };

    MeshHostObject() = default;

    MeshHostObject(MeshHostObject&& other) noexcept = default;

    MeshHostObject& operator=(MeshHostObject&& other) noexcept = default;

public:
    std::pair<unsigned char*,AttributeLayout> getInterleavingAttributes()
    {
        if(interleavingAttributes == nullptr)
        {
            int current_offset = 16; // Position aligned with 16
            int current_size = 16;

            if(normal!= nullptr)
            {
                attributeLayout.normalOffset = current_offset;
                current_size += 16;
                current_offset += 16;
            }
            if(tangent!= nullptr)
            {
                attributeLayout.tangentOffset = current_offset;
                current_size += 16;
                current_offset += 16;
            }
            if(bitangent!= nullptr)
            {
                attributeLayout.biTangentOffset = current_offset;
                current_size += 16;
                current_offset += 16;
            }
            if(uv!= nullptr)
            {
                attributeLayout.uvOffset = current_offset;
                current_size += 8;
            }
            //don't forget position must aligned with 16, so some padding might be necessary.
            if(current_size % 16 != 0)
                current_size += (16 - (current_size % 16));
            attributeLayout.VertexStride = current_size;

            static_assert(sizeof(unsigned char ) == 1);
#if WIN32
            interleavingAttributes.reset((unsigned char*)_aligned_malloc(vertex_count * current_size, 16));
#else
            interleavingAttributes.reset((unsigned char*)std::aligned_alloc(16, vertex_count * current_size));
#endif // WIN32
            auto* interleavingAttributes_raw = interleavingAttributes.get();
            const float * position_raw = position.get();
            //assert(position != nullptr);
            const float* normal_raw = normal.get();
            const float* tangent_raw = tangent.get();
            const float* bitangent_raw = bitangent.get();
            const float* uv_raw = uv.get();

            for(int i = 0; i < vertex_count; i ++)
            {
                auto * current_vertex_start = interleavingAttributes_raw + attributeLayout.VertexStride * i;
                auto * position_start = position_raw + i * 3;
                memcpy(current_vertex_start, position_start, 3 * sizeof(float));
                if(normal_raw != nullptr)
                {
                    auto* normal_start = normal_raw + i * 3;
                    memcpy( current_vertex_start + attributeLayout.normalOffset,normal_start, 3 * sizeof(float));
                }
                if(tangent_raw != nullptr)
                {
                    auto* tangent_start = tangent_raw+ i * 3;
                    memcpy(current_vertex_start  + attributeLayout.tangentOffset,tangent_start, 3 * sizeof(float));
                }
                if(bitangent_raw != nullptr)
                {
                    auto* bitangent_start = bitangent_raw + i * 3;
                    memcpy(current_vertex_start + attributeLayout.biTangentOffset,bitangent_start, 3 * sizeof(float));
                }
                if(uv_raw != nullptr)
                {
                    auto* uv_start = uv_raw + i * 2;
                    memcpy(current_vertex_start + attributeLayout.uvOffset,uv_start, 2 * sizeof(float));
                }
            }
        }
        return {interleavingAttributes.get(),attributeLayout};
    }

    ~MeshHostObject() = default;
private:
    struct AlignDeleter {
        void operator()(unsigned char* p) const {
            if (p != nullptr)
#if WIN32
                _aligned_free(p);
#else
              std::free(p);
#endif
        }
    };
    std::unique_ptr<unsigned char,AlignDeleter> interleavingAttributes = nullptr;
    AttributeLayout attributeLayout;
};

/*
 * Why should we manage mesh device data in AssetManager instead of renderScene?
 * Because not only render scene want the data. For example, inspector may also need it.
 * */
struct MeshRigidDevice{
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

        return attributeDesc;
    }

    explicit MeshRigidDevice(const VertexAttribute& attribute,uint32_t uuid) : vertexAttribute(attribute),
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

    void bindPosOnly(vk::CommandBuffer cmd,vk::DispatchLoaderDynamic loader) {
        vk::DeviceSize vertexStride = vertexAttribute.stride;
        cmd.bindVertexBuffers2EXT(0, { vertexBuffer.buffer }, { 0 }, nullptr, { vertexStride },loader);
        cmd.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
    }
};

struct MeshRigidHandle
{
    MeshHostObject* hostObject;

    MeshRigidDevice* operator->() const;

    bool operator==(const MeshRigidHandle& other) const;

    operator bool() const {
        return (manager != nullptr) && (idx != -1);
    }
    friend AssetManager;
private:
    AssetManager* manager = nullptr;
    uint32_t idx = -1;
};

struct TextureDeviceObject
{
    VMAImage image;
    vk::ImageView imageView;
    vk::Sampler sampler;
    VkImageCreateInfo imgInfo{};
    vk::ImageViewCreateInfo imgViewInfo{};
};

struct TextureDeviceHandle
{
    TextureDeviceObject* operator->() const;

    bool operator==(const TextureDeviceHandle& other) const;

    operator bool() const {
        return (manager != nullptr) && (idx != -1);
    }

    friend AssetManager;
private:
    AssetManager* manager = nullptr;
    uint32_t idx = -1;
};

namespace fs = std::filesystem;

template<typename T>
using AssetCacheT = std::unordered_map<std::string,T>;

/*
 * Asset manager will load all the resource : texture image,
 * mesh, animation when scene is loading, And unload all the memory data while scene is switch.
 * Currently we don't plan to support directly change the resource data, so 'unload' just means
 * free all the memory.
 */
struct AssetManager
{
    TextureHostObject* getOrLoadImg(const std::string & relative_path);

    std::future<TextureHostObject*>* getOrLoadImgAsync(const std::string & relative_path);

    TextureDeviceHandle getOrLoadImgDevice(const std::string & relative_path,
                                           const std::string & encoding,
                                           const std::string & warp,
                                           float maxAnisotropy,
                                           bool genMipmap = true);

    /*
    *  For PBRT PLY file we assume that each file only contain single mesh
    */
    MeshHostObject* getOrLoadPBRTPLY(const std::string & relative_path);

    std::future<MeshHostObject*> getOrLoadMeshAsync(const std::string & relative_path);

    MeshRigidHandle getOrLoadPLYMeshDevice(const std::string & relative_path);

    void setWorkDir(const fs::path & path);
    void setBackendDevice(DeviceExtended * device)
    {
        backendDevice = device;
        auto supportedColorFormat = backendDevice->getSupportedColorFormat();
    }
    void unloadAllImg();

private:

    TextureHostObject loadImg(const std::string & relative_path);
    MeshHostObject loadMeshPBRTPLY(const std::string & relative_path, int importerID);

    std::filesystem::path _currentWorkDir;

    //we use the absolute file path as the identifier of asset in cache
    AssetCacheT<TextureHostObject> loadedImgCache;
    AssetCacheT<MeshHostObject> loadedMeshCache;

    std::vector<std::pair<std::string,std::future<TextureHostObject*>>> imgLoadRequests;
    std::vector<std::pair<std::string,std::unique_ptr<std::future<MeshHostObject*>>>> meshLoadRequests;

    std::atomic<float> _totalImgSizeKB;
    std::mutex imgCacheLock;
    std::mutex meshCacheLock;

    std::unordered_map<std::string,std::unique_ptr<std::mutex>> meshLoadLockMap;
    std::mutex meshLoadLockMapLock;

    std::vector<Assimp::Importer> perThreadImporter{1};
    ThreadPool workerPool{ 1 };

    std::vector<std::pair<std::string,MeshRigidDevice>> device_meshes;
    std::vector<std::pair<std::string,TextureDeviceObject>> device_textures;

    friend MeshRigidHandle;
    friend TextureDeviceHandle;

    DeviceExtended* backendDevice;
};