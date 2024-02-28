#include "AssetManager.hpp"
#include "GlobalLogger.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <cassert>
#include <meshoptimizer.h>
#include "happly.h"

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
    stbi_info(fileName.c_str(),&x,&y,&channels);
    int desired_channels = (channels == 3 || channels == 2 )? 4 : channels;
    stbi_set_flip_vertically_on_load(true);
    auto img_mem = stbi_load(fileName.c_str(),&x,&y,&channels,desired_channels);
    if(img_mem != nullptr){
        TextureHostObject textureHostObj{};
        textureHostObj.channels =desired_channels;
        textureHostObj.width = x;
        textureHostObj.height = y;
        textureHostObj.data.reset(img_mem);
        //logging
        auto size = x * y * desired_channels;
        float img_mem_size_kb = float(size * sizeof(stbi_us))/1024.0f;
        GlobalLogger::getInstance().info("Loaded Image " + relative_path + "\t + [ " +std::to_string(desired_channels) + "] + mem : " + std::to_string(img_mem_size_kb) + "KB");
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

TextureDeviceHandle AssetManager::getOrLoadImgDevice(const std::string &relative_path,
                                                     const std::string & encoding,
                                                     const std::string & warp,
                                                     float maxAnisotropy) {
    TextureDeviceHandle handle;
    for(int i = 0; i < device_textures.size(); i++)
    {
        if(device_textures[i].first == relative_path)
        {
            handle.manager = this;
            handle.idx = i;
            break;
        }
    }

    if(handle.manager == nullptr)
    {
        auto * textureHost = getOrLoadImg(relative_path);

        TextureDeviceObject textureDevice;
        textureDevice.imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        textureDevice.imgInfo.extent.width = textureHost->width;
        textureDevice.imgInfo.extent.height = textureHost->height;
        textureDevice.imgInfo.extent.depth = 1;
        textureDevice.imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        textureDevice.imgInfo.imageType = VK_IMAGE_TYPE_2D;
        textureDevice.imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        //https://www.intel.com/content/dam/develop/external/us/en/documents/apiwithoutwithoutsecretsintroductiontovulkanpart6-final-738455.pdf
        /*
         * there are severe	restrictions on	images with	linear tiling.
         * For	example, the Vulkan	specification says that	only 2D	images must support	linear tiling.
         * Hardware vendors	may	implement support for linear tiling	in other image types, but this is not obligatory.
         * What's more important, linear tiling images may have worse performance than their optimal tiling counterparts.
         * */
        textureDevice.imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        assert(textureHost->channels == 1 || textureHost->channels == 4);
        do {
            if (textureHost->channels == 4)
            {
                if (encoding == "sRGB")
                {
                    textureDevice.imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
                    break;
                }
                if (encoding == "linear")
                {
                    textureDevice.imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                    break;
                }
            }
            if (textureHost->channels == 1)
            {
                if (encoding == "sRGB")
                {
                    textureDevice.imgInfo.format = VK_FORMAT_R8_SRGB;
                    break;
                }
                if (encoding == "linear")
                {
                    textureDevice.imgInfo.format = VK_FORMAT_R8_UNORM;
                    break;
                }
            }
        } while (0);
        textureDevice.imgInfo.arrayLayers = 1;
        textureDevice.imgInfo.mipLevels = 1;
        textureDevice.imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        textureDevice.imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        textureDevice.imgInfo.pQueueFamilyIndices = nullptr;
        textureDevice.imgInfo.queueFamilyIndexCount = 0;

        auto img = backendDevice->allocateVMAImage(textureDevice.imgInfo);
        if(!img.has_value())
            throw std::runtime_error("Failed to allocate VKImage for " + relative_path);

        textureDevice.image = img.value();
        backendDevice->setObjectDebugName(static_cast<vk::Image>(textureDevice.image.image),relative_path.c_str());

        backendDevice->oneTimeUploadSync(textureHost->data.get(),textureHost->width,textureHost->height,textureHost->channels,textureDevice.image.image,VK_IMAGE_LAYOUT_UNDEFINED);

        textureDevice.imgViewInfo.setViewType(vk::ImageViewType::e2D);
        textureDevice.imgViewInfo.setImage(textureDevice.image.image);
        textureDevice.imgViewInfo.setFormat(static_cast<vk::Format>(textureDevice.imgInfo.format));
        vk::ComponentMapping mapping;
        mapping.r = vk::ComponentSwizzle::eIdentity;
        mapping.g = vk::ComponentSwizzle::eIdentity;
        mapping.b = vk::ComponentSwizzle::eIdentity;
        mapping.a = vk::ComponentSwizzle::eIdentity;
        textureDevice.imgViewInfo.setComponents(mapping);
        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
        subresourceRange.setLevelCount(1);
        subresourceRange.setBaseMipLevel(0);
        subresourceRange.setLayerCount(1);
        subresourceRange.setBaseArrayLayer(0);
        textureDevice.imgViewInfo.setSubresourceRange(subresourceRange);
        textureDevice.imageView = backendDevice->createImageView(textureDevice.imgViewInfo);
        backendDevice->setObjectDebugName(textureDevice.imageView,relative_path.c_str());

        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
        samplerInfo.setMinFilter(vk::Filter::eLinear);
        samplerInfo.setMagFilter(vk::Filter::eLinear);
        samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);
        samplerInfo.setMinLod(0.0);
        samplerInfo.setMaxLod(1.0);
        samplerInfo.setMipLodBias(0.0);
        samplerInfo.setCompareEnable(vk::False);
        if(maxAnisotropy > 0.0)
        {
            samplerInfo.setAnisotropyEnable(vk::True);
            samplerInfo.setMaxAnisotropy(maxAnisotropy);
        }else{
            samplerInfo.setAnisotropyEnable(vk::False);
        }
        do {
            if(warp == "repeat")
            {
                samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
                samplerInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
                samplerInfo.setAddressModeW(vk::SamplerAddressMode::eRepeat);
            }
            if(warp == "black")
            {
                samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
                samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
                samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
            }
            if(warp == "clamp")
            {
                samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
                samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
                samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
            }
        } while (0);
        textureDevice.sampler = backendDevice->createSampler(samplerInfo);
        device_textures.emplace_back(relative_path,textureDevice);
        handle.manager = this;
        handle.idx = device_textures.size() - 1;
    }

    return handle;
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
        for(int i = 1; i < AI_MAX_NUMBER_OF_TEXTURECOORDS;i++)
        {
            assert(!mesh->HasTextureCoords(i));
        }
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

MeshHostObject parseHapply(happly::PLYData & ply)
{
    MeshHostObject meshHostObj;
    meshHostObj.aabb[0] = meshHostObj.aabb[1] = meshHostObj.aabb[2] = std::numeric_limits<float>::infinity();
    meshHostObj.aabb[3] = meshHostObj.aabb[4] = meshHostObj.aabb[5] = -std::numeric_limits<float>::infinity();

    // Get mesh-style data from the object
    assert(ply.hasElement("vertex") && ply.hasElement("face"));
    auto & vertexElement = ply.getElement("vertex");
    meshHostObj.vertex_count = vertexElement.count;
    assert(vertexElement.hasProperty("x") && vertexElement.hasProperty("y") && vertexElement.hasProperty("z"));
    try
    {
        auto pos_x = vertexElement.getProperty<float>("x");
        auto pos_y = vertexElement.getProperty<float>("y");
        auto pos_z = vertexElement.getProperty<float>("z");

        auto* position = new float[meshHostObj.vertex_count * 3];

        for (int i = 0; i < meshHostObj.vertex_count; i++)
        {
            position[i * 3] = pos_x[i];
            position[i * 3 + 1] = pos_y[i];
            position[i * 3 + 2] = pos_z[i];
            meshHostObj.aabb[0] = std::min(meshHostObj.aabb[0],pos_x[i]);
            meshHostObj.aabb[1] = std::min(meshHostObj.aabb[1],pos_y[i]);
            meshHostObj.aabb[2] = std::min(meshHostObj.aabb[2],pos_z[i]);
            meshHostObj.aabb[3] = std::max(meshHostObj.aabb[3],pos_x[i]);
            meshHostObj.aabb[4] = std::max(meshHostObj.aabb[4],pos_y[i]);
            meshHostObj.aabb[5] = std::max(meshHostObj.aabb[5],pos_z[i]);
        }
        meshHostObj.position.reset(position);
        //normal, tangent and uv is optional
        bool hasNormal = vertexElement.hasProperty("nx") && vertexElement.hasProperty("ny") && vertexElement.hasProperty("nz");
        if(hasNormal)
        {
            auto normal_x = vertexElement.getProperty<float>("nx");
            auto normal_y = vertexElement.getProperty<float>("ny");
            auto normal_z = vertexElement.getProperty<float>("nz");

            auto * normal = new float[meshHostObj.vertex_count * 3];

            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                normal[i * 3] = normal_x[i];
                normal[i * 3 + 1] = normal_y[i];
                normal[i * 3 + 2] = normal_z[i];
            }
            meshHostObj.normal.reset(normal);
        }

        bool hasTextureCoords = vertexElement.hasProperty("u") && vertexElement.hasProperty("v");
        if(hasTextureCoords)
        {
            auto u = vertexElement.getProperty<float>("u");
            auto v = vertexElement.getProperty<float>("v");

            auto * uv = new float[meshHostObj.vertex_count * 2];

            for(int i = 0; i < meshHostObj.vertex_count; i++)
            {
                uv[i * 2 + 0] = u[i];
                uv[i * 2 + 1] = v[i];
            }
            meshHostObj.uv.reset(uv);
        }

        //we only handle triangle for now
        auto faces = ply.getFaceIndices();
        meshHostObj.index_count = faces.size() * 3;
        auto* indices = new unsigned int[meshHostObj.index_count];

        for(int i = 0; i < faces.size(); i ++)
        {
            auto & face = faces[i];
            assert(face.size() == 3);
            indices[i * 3 + 0] = face[0];
            indices[i * 3 + 1] = face[1];
            indices[i * 3 + 2] = face[2];
        }

        meshHostObj.indices.reset(indices);

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
    /*auto postProcessFlags = aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_SortByPType;
    auto & importer = perThreadImporter[importerID];
    const aiScene* scene = importer.ReadFile(fileName, postProcessFlags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        //throw std::runtime_error(_assimpImporter.GetErrorString());
        GlobalLogger::getInstance().error(importer.GetErrorString());
        throw std::runtime_error(importer.GetErrorString());
    }
    assert(scene->mNumMeshes == 1);
    auto meshHostObject = optimize(parseAssimpMesh(scene->mMeshes[0]));*/
    happly::PLYData ply(fileName);
    auto meshHostObject = optimize(parseHapply(ply));
    //importer.FreeScene();
    //logging
    GlobalLogger::getInstance().info("Loaded Mesh " + relative_path);
    return meshHostObject;
}

MeshHostObject* AssetManager::getOrLoadPBRTPLY(const std::string &relative_path) {
    return getOrLoadMeshAsync(relative_path).get();
}

std::future<MeshHostObject*> AssetManager::getOrLoadMeshAsync(const std::string &relative_path) {
    return workerPool.enqueue([this,relative_path](int id)->MeshHostObject* {
        auto fileName = fs::absolute(_currentWorkDir / relative_path).make_preferred().string();
        std::mutex *  loadLock;
        {
            std::lock_guard<std::mutex> lg(meshLoadLockMapLock);
            auto it = meshLoadLockMap.find(fileName);
            if( it != meshLoadLockMap.end())
            {
                loadLock = it->second.get();
            }else{
                loadLock = new std::mutex;
                meshLoadLockMap.emplace(fileName,loadLock);
            }
        }
        {
            std::lock_guard<std::mutex> lg(*loadLock);
            auto it = loadedMeshCache.find(fileName);
            if ( it != loadedMeshCache.end()) {
                return &it->second;
            }
            auto meshHostObj = loadMeshPBRTPLY(relative_path,id);
            loadedMeshCache.emplace(fileName,std::move(meshHostObj));
            return &loadedMeshCache[fileName];
        }
    });
}

MeshRigidHandle AssetManager::getOrLoadPLYMeshDevice(const std::string &relative_path) {
    MeshRigidHandle handle;
    handle.hostObject = getOrLoadPBRTPLY(relative_path);

    for(int i = 0; i < device_meshes.size(); i ++)
    {
        if(device_meshes[i].first == relative_path)
        {
            handle.manager = this;
            handle.idx = i;
            break;
        }
    }

    if(handle.manager == nullptr)
    {
        // Existing mesh not found
        auto interleaveAttribute = handle.hostObject->getInterleavingAttributes();

        unsigned int * indicies = handle.hostObject->indices.get();
        auto indexBufferSize = handle.hostObject->index_count * sizeof(indicies[0]);
        auto vertexBufferSize = handle.hostObject->vertex_count * interleaveAttribute.second.VertexStride;
        auto interleavingBufferAttribute = interleaveAttribute.second;

        auto vertexBuffer = backendDevice->allocateBuffer(vertexBufferSize,(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        auto indexBuffer = backendDevice->allocateBuffer(indexBufferSize,(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

        auto vertexBufferDebugName = relative_path + "VertexBuffer";
        auto indexBufferDebugName = relative_path + "IndexBuffer";

        backendDevice->setObjectDebugName(static_cast<vk::Buffer>(vertexBuffer->buffer),vertexBufferDebugName.c_str());
        backendDevice->setObjectDebugName(static_cast<vk::Buffer>(indexBuffer->buffer),indexBufferDebugName.c_str());

        if(!vertexBuffer)
        {
            throw std::runtime_error("Failed to allocate vertex buffer");
        }

        if(!indexBuffer)
        {
            throw std::runtime_error("Failed to allocate index buffer");
        }

        MeshRigidDevice::VertexAttribute vertexAttribute;
        vertexAttribute.stride = interleavingBufferAttribute.VertexStride;
        vertexAttribute.normalOffset = interleavingBufferAttribute.normalOffset;
        vertexAttribute.tangentOffset = interleavingBufferAttribute.tangentOffset;
        vertexAttribute.biTangentOffset = interleavingBufferAttribute.biTangentOffset;
        vertexAttribute.uvOffset = interleavingBufferAttribute.uvOffset;

        MeshRigidDevice meshRigid{vertexAttribute,static_cast<uint32_t>(device_meshes.size())};

        meshRigid.vertexBuffer = vertexBuffer.value();
        meshRigid.indexBuffer = indexBuffer.value();
        meshRigid.vertexCount = handle.hostObject->vertex_count;
        meshRigid.indexCount = handle.hostObject->index_count;

        backendDevice->oneTimeUploadSync(handle.hostObject->indices.get(),indexBufferSize,meshRigid.indexBuffer.buffer);
        backendDevice->oneTimeUploadSync(interleaveAttribute.first,vertexBufferSize,meshRigid.vertexBuffer.buffer);

        device_meshes.emplace_back(relative_path,meshRigid);

        handle.manager= this;
        handle.idx = device_meshes.size() - 1;
    }

    return handle;
}

void AssetManager::unloadAllImg() {

}

MeshRigidDevice *MeshRigidHandle::operator->() const {
    return &manager->device_meshes[idx].second;
}

bool MeshRigidHandle::operator==(const MeshRigidHandle &other) const {
    if(manager != other.manager) return false;
    if(idx != other.idx) return false;
    return true;
}

TextureDeviceObject *TextureDeviceHandle::operator->() const {
    return &manager->device_textures[idx].second;
}

bool TextureDeviceHandle::operator==(const TextureDeviceHandle &other) const {
    return false;
}

