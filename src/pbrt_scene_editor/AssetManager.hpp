#pragma once

#include <filesystem>
#include <unordered_map>
#include <future>
#include "LockFreeCircleQueue.hpp"
#include <utility>
#include <variant>
#include "assimp/Importer.hpp"

struct TextureHostObject
{
    unsigned int channels = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned char* data = nullptr;
};

struct MeshHostObject
{
    unsigned int index_count = 0;
    unsigned int* indices = nullptr;
    unsigned int vertex_count = 0;
    float* position = nullptr;
    //optional below
    float* normal = nullptr;
    float* tangent = nullptr;
    float* bitangent = nullptr;
    float* uv = nullptr;

    struct AttributeLayout
    {
        int VertexStride;
        int normalOffset = -1;
        int tangentOffset = -1;
        int biTangentOffset = -1;
        int uvOffset = -1;
    };

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
            interleavingAttributes = (unsigned char *)std::aligned_alloc(16,vertex_count * current_size);

            for(int i = 0; i < vertex_count; i ++)
            {
                auto * current_vertex_start = interleavingAttributes + attributeLayout.VertexStride * i;
                memcpy(current_vertex_start, position + i * 3 * sizeof(float), 3 * sizeof(float));
                if(normal != nullptr)
                {
                    memcpy( current_vertex_start + attributeLayout.normalOffset,&normal[i * 3], 3 * sizeof(float));
                }

                if(tangent != nullptr)
                {
                    memcpy(current_vertex_start  + attributeLayout.tangentOffset,&tangent[i * 3], 3 * sizeof(float));
                }

                if(bitangent != nullptr)
                {
                    memcpy(current_vertex_start + attributeLayout.biTangentOffset,&bitangent[i * 3], 3 * sizeof(float));
                }

                if(uv != nullptr)
                {
                    memcpy(current_vertex_start  + attributeLayout.uvOffset,&uv[i * 2], 2 * sizeof(float));
                }
            }
        }
        return {interleavingAttributes,attributeLayout};
    }

    ~MeshHostObject()
    {
        //todo WTF ??? Who release these?
//        if(indices!= nullptr) std::free(indices);
//        if(position!= nullptr) std::free(position);
//        if(normal!= nullptr) std::free(normal);
//        if(tangent!= nullptr) std::free(tangent);
//        if(bitangent!= nullptr) std::free(bitangent);
//        if(uv!= nullptr) std::free(uv);
        if(interleavingAttributes != nullptr)
            std::free(interleavingAttributes);
    }
private:
    unsigned char* interleavingAttributes = nullptr;
    AttributeLayout attributeLayout;
};

namespace fs = std::filesystem;

template<typename T>
struct AssetFuture
{
    explicit AssetFuture(std::string  name, std::shared_future<void*> future) :
                                                _request_name(std::move(name)),
                                                _sharedFuture(std::move(future)){
        _refCount = std::make_shared<std::atomic_int>(1);
    }

    AssetFuture(const AssetFuture& other) :
        _request_name(other._request_name),
        _sharedFuture(other._sharedFuture),
        _refCount(other._refCount){
        (*_refCount)++;
    }

    AssetFuture& operator=(const AssetFuture& other) {
        if (this != &other) {
            _request_name = other._request_name;
            _sharedFuture = other._sharedFuture;
            _refCount = other._refCount;
            (*_refCount)++;
        }
        return *this;
    }

    ~AssetFuture() {
        if (--(*_refCount) == 0) {
            // If the last reference is gone, future is no longer needed
            _sharedFuture.wait();
        }
    }

    void* get() const{
        return _sharedFuture.get();
    }

    int ref_count() const{
        return *_refCount;
    }

    std::string _request_name;

private:
    std::shared_future<void*> _sharedFuture;
    std::shared_ptr<std::atomic_int> _refCount;
};

template<typename T>
using AssetCacheT = std::unordered_map<std::string,T>;

template<typename T>
using AssetRequest = std::variant<AssetFuture<T>,T>;

/*
 * Asset manager will load all the resource : texture image,
 * mesh, animation when scene is loading, And unload all the memory data while scene is switch.
 * Currently we don't plan to support directly change the resource data, so 'unload' just means
 * free all the memory.
 */
struct AssetManager
{
    void loadImg(const std::string & relative_path);
    AssetRequest<TextureHostObject> loadImgAsync(const std::string & relative_path);
    /*
    *  For PBRT PLY file we assume that each file only contain single mesh
    */
    MeshHostObject* loadMeshPBRTPLY(const std::string & relative_path);
    void setWorkDir(const fs::path & path);
    void unloadAllImg();

private:
    std::filesystem::path _currentWorkDir;

    //we use the absolute file path as the identifier of asset in cache
    AssetCacheT<TextureHostObject> loadedImgCache;
    AssetCacheT<MeshHostObject> loadedMeshCache;

    std::vector<AssetFuture<TextureHostObject>> _loadRequestQueue;
    std::mutex _LRQLock;
    std::atomic<float> _totalImgSizeKB;
    std::mutex _cacheLock;

    Assimp::Importer _assimpImporter; //todo: not thread-safe
};