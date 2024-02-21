#include "AssetManager.hpp"
#include "GlobalLogger.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <cassert>
#include <meshoptimizer.h>

void AssetManager::setWorkDir(const fs::path &path) {
    _currentWorkDir = path;
}

/*
 * From Aspect Oriented Programming perspective, load image should only care about how to load image,
 * instead of other stuff like caching or logging.
 * */
TextureHostObject AssetManager::loadImg(const std::string &relative_path) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    int x,y,channels;
    auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,0);
    if(img_mem != nullptr){
        TextureHostObject textureHostObj{};
        textureHostObj.channels = channels;
        textureHostObj.width = x;
        textureHostObj.height = y;
        textureHostObj.data.reset(img_mem);
        return textureHostObj;
    }
    throw std::runtime_error("Load Image " + relative_path);
}

TextureHostObject* AssetManager::getOrLoadImg(const std::string &relative_path) {
    for(auto & req : imgLoadRequests)
    {
        if(req.first == relative_path)
        {
            return req.second.get();
        }
    }
    return getOrLoadImgAsync(relative_path)->get();
}

/*
 * For now getOrLoadImg/Async must use in single thread environment
 * */
std::future<TextureHostObject*>* AssetManager::getOrLoadImgAsync(const std::string &relative_path) {
    for(auto & req : imgLoadRequests)
    {
        if(req.first == relative_path)
        {
            return &req.second;
        }
    }
    auto req = workerPool.enqueue([this,relative_path](int id)->TextureHostObject* {
        auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
        {
            std::lock_guard<std::mutex> lg(imgCacheLock);
            auto it = loadedImgCache.find(fileName);
            if ( it != loadedImgCache.end()) {
                return &it->second;
            }
        }

        auto textureHostObj = loadImg(relative_path);
        //logging
        auto size = textureHostObj.width * textureHostObj.height * textureHostObj.channels;
        float img_mem_size_kb = float(size * sizeof(stbi_us))/1024.0f;
        GlobalLogger::getInstance().info("Load Image " + relative_path + "\t mem : " + std::to_string(img_mem_size_kb) + "KB");

        // Cache loaded texture obj.
        std::lock_guard<std::mutex> lg(imgCacheLock);
        auto it = loadedImgCache.find(fileName);
        if ( it != loadedImgCache.end()) {
            return &it->second;
        }
        loadedImgCache.emplace(fileName,std::move(textureHostObj));
        return &loadedImgCache[fileName];
    });

    imgLoadRequests.emplace_back(relative_path,std::move(req));
    return &imgLoadRequests.back().second;
}

MeshHostObject parseAssimpMesh(aiMesh* mesh)
{
    MeshHostObject meshHostObj;
    meshHostObj.aabb[0] = meshHostObj.aabb[1] = meshHostObj.aabb[2] = std::numeric_limits<float>::infinity();
    meshHostObj.aabb[3] = meshHostObj.aabb[4] = meshHostObj.aabb[5] = -std::numeric_limits<float>::infinity();
    meshHostObj.vertex_count = mesh->mNumVertices;
    //we assume each vertex include position
    assert(mesh->HasPositions());
    try
    {
        auto* position = new float[meshHostObj.vertex_count * 3];
        for (int i = 0; i < meshHostObj.vertex_count; i++)
        {
            position[i * 3] = mesh->mVertices[i].x;
            position[i * 3 + 1] = mesh->mVertices[i].y;
            position[i * 3 + 2] = mesh->mVertices[i].z;
            meshHostObj.aabb[0] = std::min(meshHostObj.aabb[0],mesh->mVertices[i].x);
            meshHostObj.aabb[1] = std::min(meshHostObj.aabb[1],mesh->mVertices[i].y);
            meshHostObj.aabb[2] = std::min(meshHostObj.aabb[2],mesh->mVertices[i].z);
            meshHostObj.aabb[3] = std::max(meshHostObj.aabb[3],mesh->mVertices[i].x);
            meshHostObj.aabb[4] = std::max(meshHostObj.aabb[4],mesh->mVertices[i].y);
            meshHostObj.aabb[5] = std::max(meshHostObj.aabb[5],mesh->mVertices[i].z);
        }
        meshHostObj.position.reset(position);
        //normal, tangent and uv is optional
        if(mesh->HasNormals())
        {
            auto * normal = new float[meshHostObj.vertex_count * 3];
            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                normal[i*3] = mesh->mNormals[i].x;
                normal[i*3+1] = mesh->mNormals[i].y;
                normal[i*3+2] = mesh->mNormals[i].z;
            }
            meshHostObj.normal.reset(normal);
        }
        if(mesh->HasTangentsAndBitangents())
        {
            auto * tangent = new float [meshHostObj.vertex_count * 3];
            auto * bitangent = new float [meshHostObj.vertex_count * 3];

            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                tangent[i*3] = mesh->mTangents[i].x;
                tangent[i*3+1] = mesh->mTangents[i].y;
                tangent[i*3+2] = mesh->mTangents[i].z;
            }
            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                bitangent[i*3] = mesh->mBitangents[i].x;
                bitangent[i*3+1] = mesh->mBitangents[i].y;
                bitangent[i*3+2] = mesh->mBitangents[i].z;
            }

            meshHostObj.tangent.reset(tangent);
            meshHostObj.bitangent.reset(bitangent);
        }

        //We assume each mesh has at most one UV coord, and is stored at idx 0
        if(mesh->HasTextureCoords(0))
        {
            auto * uv = new float[meshHostObj.vertex_count * 2];
            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                uv[i*2] = mesh->mTextureCoords[0][i].x;
                uv[i*2+1] = mesh->mTextureCoords[0][i].y;
            }
            meshHostObj.uv.reset(uv);
        }

        //we only handle triangle for now
        if(mesh->mPrimitiveTypes!=aiPrimitiveType::aiPrimitiveType_TRIANGLE)
        {
            //drop all other types of primitives
            for(int i = 0;i < mesh->mNumFaces; i++)
            {
                auto face = mesh->mFaces[i];
                if(face.mNumIndices==3)
                {
                    meshHostObj.index_count += 3;
                }
            }

            auto* indices = new unsigned int[meshHostObj.index_count];

            int current_idx = 0;
            for(int i = 0;i < mesh->mNumFaces; i++)
            {
                auto face = mesh->mFaces[i];
                if(face.mNumIndices==3)
                {
                    for(int j = 0; j < 3; j++)
                    {
                        indices[current_idx++] = face.mIndices[j];
                    }
                }
            }
            meshHostObj.indices.reset(indices);
            assert(current_idx == meshHostObj.index_count);
        }else{
            meshHostObj.index_count = mesh->mNumFaces * 3;
            auto* indices = new unsigned int[meshHostObj.index_count];
            int current_idx = 0;
            for(int i = 0;i < mesh->mNumFaces; i++)
            {
                auto face = mesh->mFaces[i];
                assert(face.mNumIndices == 3);
                for(int j = 0; j < 3; j++)
                {
                    indices[current_idx++] = face.mIndices[j];
                }
            }
            meshHostObj.indices.reset(indices);
            assert(current_idx == meshHostObj.index_count);
        }
    }catch (std::bad_alloc& e)
    {
        std::cout << e.what() << std::endl;
        throw std::runtime_error(e.what());
    }

    return meshHostObj;
}

MeshHostObject optimize(MeshHostObject&& rawMesh)
{
    //Simplification
    /*float threshold = 0.1f;
    auto target_index_count = static_cast<size_t>(rawMesh.index_count * threshold);
    if(target_index_count > 3)
    {
        float target_error = 1e-1f;

        std::vector<unsigned int> lod(rawMesh.index_count);
        float lod_error;
        lod.resize(meshopt_simplifySloppy(lod.data(), rawMesh.indices.get(), rawMesh.index_count,
                                          rawMesh.position.get(), rawMesh.vertex_count, sizeof(float) * 3,
                                          target_index_count, target_error, &lod_error));
        assert(!lod.empty());
        rawMesh.index_count = lod.size();
        rawMesh.indices.reset(new unsigned int[lod.size()]);
        std::copy(lod.begin(), lod.end(), rawMesh.indices.get());
    }*/

    return std::move(rawMesh);
}

MeshHostObject AssetManager::loadMeshPBRTPLY(const std::string &relative_path,int importerID) {
    auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
    auto postProcessFlags = aiProcess_Triangulate;
    auto & importer = perThreadImporter[importerID];
    const aiScene* scene = importer.ReadFile(fileName, postProcessFlags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        //throw std::runtime_error(_assimpImporter.GetErrorString());
        GlobalLogger::getInstance().error(importer.GetErrorString());
        throw std::runtime_error(importer.GetErrorString());
    }
    assert(scene->mNumMeshes == 1);
    auto meshHostObject = optimize(parseAssimpMesh(scene->mMeshes[0]));
    importer.FreeScene();
    //logging
    GlobalLogger::getInstance().info("Loaded Mesh " + relative_path);
    return meshHostObject;
}

MeshHostObject* AssetManager::getOrLoadPBRTPLY(const std::string &relative_path) {
    for(auto & req : meshLoadRequests)
    {
        if(req.first == relative_path)
        {
            return req.second.get();
        }
    }
    return getOrLoadMeshAsync(relative_path)->get();
}

std::future<MeshHostObject *>* AssetManager::getOrLoadMeshAsync(const std::string &relative_path) {
    for(auto & req : meshLoadRequests)
    {
        if(req.first == relative_path)
        {
            return &req.second;
        }
    }

    auto req = workerPool.enqueue([this,relative_path](int id)->MeshHostObject* {
        auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
        {
            std::lock_guard<std::mutex> lg(meshCacheLock);
            auto it = loadedMeshCache.find(fileName);
            if ( it != loadedMeshCache.end()) {
                return &it->second;
            }
        }
        auto meshHostObj = loadMeshPBRTPLY(relative_path,id);
        // Cache loaded mesh.
        std::lock_guard<std::mutex> lg(meshCacheLock);
        auto it = loadedMeshCache.find(fileName);
        if ( it != loadedMeshCache.end()) {
            return &it->second;
        }
        loadedMeshCache.emplace(fileName,std::move(meshHostObj));
        return &loadedMeshCache[fileName];
    });
    meshLoadRequests.emplace_back(relative_path,std::move(req));
    return &meshLoadRequests.back().second;
}

void AssetManager::unloadAllImg() {

}