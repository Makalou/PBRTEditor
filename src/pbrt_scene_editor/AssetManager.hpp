#pragma once

#include <filesystem>
#include <unordered_map>
#include <future>
#include "LockFreeCircleQueue.hpp"
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
    float* normal = nullptr;
    float* tangent = nullptr;
    float* bitangent = nullptr;
    float* uv = nullptr;
};

namespace fs = std::filesystem;

template<typename T>
struct AssetFuture
{
    explicit AssetFuture(const std::string & name, std::shared_future<void*> future) :
                                                _request_name(name),
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
    void loadMeshPBRTPLY(const std::string & relative_path);
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