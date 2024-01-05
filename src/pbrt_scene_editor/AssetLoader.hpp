#pragma once

#include <filesystem>
#include <unordered_map>
#include <future>
#include "LockFreeCircleQueue.hpp"
#include <variant>

namespace fs = std::filesystem;

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

using AssetCacheT = std::unordered_map<std::string,void*>;
using AssetRequest = std::variant<AssetFuture,void*>;

struct AssetLoader
{
    void loadImg(const std::string & relative_path);
    AssetRequest loadImgAsync(const std::string & relative_path);

    void loadMesh(const std::string & relative_path);
    void setWorkDir(const fs::path & path);
    void unloadAllImg();

private:
    std::filesystem::path _currentWorkDir;
    AssetCacheT loadedCache;
    std::vector<AssetFuture> _loadRequestQueue;
    std::mutex _LRQLock;
    std::atomic<float> _totalImgSizeKB;
    std::mutex _cacheLock;
};