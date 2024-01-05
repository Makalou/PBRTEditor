#include "AssetLoader.hpp"
#include "stb_image.h"
#include "GlobalLogger.h"

void AssetLoader::setWorkDir(const fs::path &path) {
    _currentWorkDir = path;
}

void AssetLoader::loadImg(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    if(loadedCache.find(fileName)!=loadedCache.end()){
        return;
    }
    int x,y,channels;
    auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,0);
    if(img_mem != nullptr){
        float img_mem_size_kb = float(x * y * channels * sizeof(stbi_us))/1024.0f;
        GlobalLogger::getInstance().info("Load Image " + relative_path + "\t mem : " + std::to_string(img_mem_size_kb) + "KB");
        loadedCache.emplace(fileName,img_mem);
    }
}

AssetRequest AssetLoader::loadImgAsync(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    std::lock_guard<std::mutex> lg(_LRQLock);
    auto it = loadedCache.find(fileName);
    if(it!=loadedCache.end()){
        return it->second;
    }

    auto loadTask = [this,fileName,relative_path]()->void*{
        int x,y,channels;
        auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,0);
        if(img_mem != nullptr){
            float img_mem_size_kb = float(x * y * channels * sizeof(stbi_us))/1024.0f;
            GlobalLogger::getInstance().info("Load Image " + relative_path + "\t mem : " + std::to_string(img_mem_size_kb) + "KB");
            float oldValue = _totalImgSizeKB.load();
            while (!_totalImgSizeKB.compare_exchange_weak(oldValue, oldValue + img_mem_size_kb));
            std::lock_guard<std::mutex> lg(_cacheLock);
            loadedCache.emplace(fileName,img_mem);
        }
        return img_mem;
    };

//    if (!_loadRequestQueue.empty() && _loadRequestQueue.front().ref_count() == 1)
//        _loadRequestQueue.erase(_loadRequestQueue.begin(), _loadRequestQueue.begin() + 1);

    for(const auto & req : _loadRequestQueue){
        if (req._request_name == fileName)
            return req;
    }

    return _loadRequestQueue.emplace_back(fileName,std::async(std::launch::async,loadTask));
}

void AssetLoader::loadMesh(const std::string &relative_path) {

}

void AssetLoader::unloadAllImg() {
    for(auto & cache : loadedCache){
        stbi_image_free(cache.second);
    }
    loadedCache.clear();
}