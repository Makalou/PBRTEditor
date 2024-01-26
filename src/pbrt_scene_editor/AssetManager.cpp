#include "AssetManager.hpp"
#include "stb_image.h"
#include "GlobalLogger.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <cassert>

void AssetManager::setWorkDir(const fs::path &path) {
    _currentWorkDir = path;
}

void AssetManager::loadImg(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    if(loadedImgCache.find(fileName)!=loadedImgCache.end()){
        return;
    }
    int x,y,channels;
    auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,0);
    if(img_mem != nullptr){
        TextureHostObject textureHostObj{};
        textureHostObj.channels = channels;
        textureHostObj.width = x;
        textureHostObj.height = y;
        int size = x * y * channels;
        textureHostObj.data = new unsigned char[size];
        memcpy(textureHostObj.data,img_mem,size);
        float img_mem_size_kb = float(size * sizeof(stbi_us))/1024.0f;
        GlobalLogger::getInstance().info("Load Image " + relative_path + "\t mem : " + std::to_string(img_mem_size_kb) + "KB");
        loadedImgCache.emplace(fileName,textureHostObj);
        stbi_image_free(img_mem);
    }
}

AssetRequest<TextureHostObject> AssetManager::loadImgAsync(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    std::lock_guard<std::mutex> lg(_LRQLock);
    auto it = loadedImgCache.find(fileName);
    if(it!=loadedImgCache.end()){
        return it->second;
    }

    auto loadTask = [this,fileName,relative_path]()->void*{
        int x,y,channels;
        auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,0);
        if(img_mem != nullptr){
            TextureHostObject textureHostObj{};
            textureHostObj.channels = channels;
            textureHostObj.width = x;
            textureHostObj.height = y;
            int size = x * y * channels;
            textureHostObj.data = new unsigned char[size];
            memcpy(textureHostObj.data,img_mem,size);
            float img_mem_size_kb = float(size * sizeof(stbi_us))/1024.0f;
            GlobalLogger::getInstance().info("Load Image " + relative_path + "\t mem : " + std::to_string(img_mem_size_kb) + "KB");
            loadedImgCache.emplace(fileName,textureHostObj);
            stbi_image_free(img_mem);
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

MeshHostObject* AssetManager::loadMeshPBRTPLY(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    if(loadedMeshCache.find(fileName) != loadedMeshCache.end()){
        return &loadedMeshCache.find(fileName)->second;
    }
    auto postProcessFlags = aiProcess_Triangulate;
    const aiScene* scene = _assimpImporter.ReadFile(fileName, postProcessFlags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        //throw std::runtime_error(_assimpImporter.GetErrorString());
        GlobalLogger::getInstance().error(_assimpImporter.GetErrorString());
        return nullptr;
    }
    assert(scene->mNumMeshes == 1);
    GlobalLogger::getInstance().info("Load Mesh " + fileName);
    auto mesh = scene->mMeshes[0];
    unsigned int vertex_count = 0;
    MeshHostObject meshHostObj;
    meshHostObj.vertex_count = mesh->mNumVertices;
    //we assume each vertex include position
    assert(mesh->HasPositions());
    meshHostObj.position = new float[vertex_count * 3];
    for(int i = 0; i < vertex_count; i++)
    {
        meshHostObj.position[i*3] = mesh->mVertices[i].x;
        meshHostObj.position[i*3+1] = mesh->mVertices[i].y;
        meshHostObj.position[i*3+2] = mesh->mVertices[i].z;
    }
    //normal, tangent and uv is optional
    if(mesh->HasNormals())
    {
        meshHostObj.normal = new float[vertex_count * 3];
        for(int i = 0; i < vertex_count; i++)
        {
            meshHostObj.normal[i*3] = mesh->mNormals[i].x;
            meshHostObj.normal[i*3+1] = mesh->mNormals[i].y;
            meshHostObj.normal[i*3+2] = mesh->mNormals[i].z;
        }
    }
    if(mesh->HasTangentsAndBitangents())
    {
        meshHostObj.tangent = new float[vertex_count*3];
        meshHostObj.bitangent = new float[vertex_count*3];
        for(int i = 0; i < vertex_count; i++)
        {
            meshHostObj.tangent[i*3] = mesh->mTangents[i].x;
            meshHostObj.tangent[i*3+1] = mesh->mTangents[i].y;
            meshHostObj.tangent[i*3+2] = mesh->mTangents[i].z;
        }
        for(int i = 0; i < vertex_count; i++)
        {
            meshHostObj.bitangent[i*3] = mesh->mBitangents[i].x;
            meshHostObj.bitangent[i*3+1] = mesh->mBitangents[i].y;
            meshHostObj.bitangent[i*3+2] = mesh->mBitangents[i].z;
        }
    }

    //We assume each mesh has at most one UV coord, and is stored at idx 0
    if(mesh->HasTextureCoords(0))
    {
        meshHostObj.uv = new float[vertex_count*2];
        for(int i = 0; i < vertex_count; i++)
        {
            meshHostObj.uv[i*2] = mesh->mTextureCoords[0][i].x;
            meshHostObj.uv[i*2+1] = mesh->mTextureCoords[0][i].y;
        }
    }

    //we only handle triangle for now
    if(mesh->mPrimitiveTypes!=aiPrimitiveType::aiPrimitiveType_TRIANGLE)
    {
        //drop all other types of primitives
        unsigned int index_count = 0;
        for(int i = 0;i < mesh->mNumFaces; i++)
        {
            auto face = mesh->mFaces[i];
            if(face.mNumIndices==3)
            {
                index_count += 3;
            }
        }

        int current_idx = 0;
        meshHostObj.index_count = index_count;
        meshHostObj.indices = new unsigned int[meshHostObj.index_count];
        for(int i = 0;i < mesh->mNumFaces; i++)
        {
            auto face = mesh->mFaces[i];
            if(face.mNumIndices==3)
            {
                for(int j = 0; j < 3; j++)
                {
                    meshHostObj.indices[current_idx++] = face.mIndices[j];
                }
            }
        }
        assert(current_idx == index_count);
    }else{
        meshHostObj.index_count = mesh->mNumFaces * 3;
        meshHostObj.indices = new unsigned int[meshHostObj.index_count];
        int current_idx = 0;
        for(int i = 0;i < mesh->mNumFaces; i++)
        {
            auto face = mesh->mFaces[i];
            assert(face.mNumIndices == 3);
            for(int j = 0; j < 3; j++)
            {
                meshHostObj.indices[current_idx++] = face.mIndices[j];
            }
        }
        assert(current_idx == meshHostObj.index_count);
    }
    loadedMeshCache.emplace(fileName,meshHostObj);
    return &loadedMeshCache.find(fileName)->second;
}

void AssetManager::unloadAllImg() {

}