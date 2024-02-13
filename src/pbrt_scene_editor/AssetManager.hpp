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
#include <cstdlib>

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
            interleavingAttributes.reset((unsigned char*)_aligned_malloc(vertex_count * current_size,16));
#else
            interleavingAttributes.reset((unsigned char*)std::aligned_alloc(16, vertex_count * current_size));
#endif // WIN32
            for(int i = 0; i < vertex_count; i ++)
            {
                auto * current_vertex_start = interleavingAttributes.get() + attributeLayout.VertexStride * i;
                memcpy(current_vertex_start, position.get() + i * 3 * sizeof(float), 3 * sizeof(float));
                if(normal != nullptr)
                {
                    memcpy( current_vertex_start + attributeLayout.normalOffset,normal.get() + i * 3, 3 * sizeof(float));
                }

                if(tangent != nullptr)
                {
                    memcpy(current_vertex_start  + attributeLayout.tangentOffset,tangent.get() + i * 3, 3 * sizeof(float));
                }

                if(bitangent != nullptr)
                {
                    memcpy(current_vertex_start + attributeLayout.biTangentOffset,bitangent.get() + i * 3, 3 * sizeof(float));
                }

                if(uv != nullptr)
                {
                    memcpy(current_vertex_start  + attributeLayout.uvOffset,uv.get() + i * 2, 2 * sizeof(float));
                }
            }
        }
        return {interleavingAttributes.get(),attributeLayout};
    }

    ~MeshHostObject() = default;
private:
    struct AlignDeleter {
        void operator()(unsigned char* p) const {
            if(p!= nullptr) std::free(p);
        }
    };
    std::unique_ptr<unsigned char,AlignDeleter> interleavingAttributes = nullptr;
    AttributeLayout attributeLayout;
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

    /*
    *  For PBRT PLY file we assume that each file only contain single mesh
    */
    MeshHostObject* getOrLoadPBRTPLY(const std::string & relative_path);

    std::future<MeshHostObject*>* getOrLoadMeshAsync(const std::string & relative_path);

    void setWorkDir(const fs::path & path);
    void unloadAllImg();

private:

    TextureHostObject loadImg(const std::string & relative_path);
    MeshHostObject loadMeshPBRTPLY(const std::string & relative_path, int importerID);

    std::filesystem::path _currentWorkDir;

    //we use the absolute file path as the identifier of asset in cache
    AssetCacheT<TextureHostObject> loadedImgCache;
    AssetCacheT<MeshHostObject> loadedMeshCache;

    std::vector<std::pair<std::string,std::future<TextureHostObject*>>> imgLoadRequests;
    std::vector<std::pair<std::string,std::future<MeshHostObject*>>> meshLoadRequests;

    std::atomic<float> _totalImgSizeKB;
    std::mutex imgCacheLock;
    std::mutex meshCacheLock;

    std::vector<Assimp::Importer> perThreadImporter{std::thread::hardware_concurrency()};
    ThreadPool workerPool{1};
};