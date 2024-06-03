#pragma once

#include <variant>
#include "VulkanExtension.h"

struct GPUPass;
struct GPUFrame;

enum class FrameGraphResourceType
{
    Texture,
    Buffer
};

namespace FrameGraphResourceAccess
{
    struct Init
    {

    };

    struct Write
    {

    };

    struct RenderTarget
    {
        vk::AttachmentLoadOp loadOp;
        vk::AttachmentStoreOp storeOp;
    };

    struct Read
    {

    };

    struct Sample
    {
        bool need_mimap;
    };

    using Access = std::variant<Init, Write, RenderTarget, Read, Sample>;
}

struct FrameGraphResourceAccessInfo
{
    GPUPass* pass;
    FrameGraphResourceAccess::Access accessType;
};

struct FrameGraphResourceDescriptionBase
{
    // uuid
    std::string name;

    /*If not allowed process in flight, then when multiple frame is executing on the GPU in the same queue
      there must be a barrier between the first Pass touch the resource and the last pass touch the resource in the previous frame,
      If allowed process in flight, then multiple backing resource will be created to increase the overlapping between multiple frames*/
    bool allow_process_in_flight = false;
    bool is_persistent = false;

    virtual FrameGraphResourceType getType() const = 0;

    explicit FrameGraphResourceDescriptionBase(const std::string& nam) : name(nam)
    {

    }

    virtual ~FrameGraphResourceDescriptionBase() = default;

    virtual void resolveBarriers(GPUFrame*) = 0;

    std::vector<FrameGraphResourceAccessInfo> accessList;
};

namespace FrameGraphTextureExtent
{
    struct SwapchainRelative
    {
        SwapchainRelative() = default;

        SwapchainRelative(float width_scale, float height_scale) : wScale(width_scale), hScale(height_scale) {}

        float wScale = 1.0f;
        float hScale = 1.0f;
    };

    struct Absolute
    {
        Absolute(int width, int height) : w(width), h(height) {}

        int w;
        int h;
    };
}

using FrameGraphTextureExtentType = std::variant<FrameGraphTextureExtent::SwapchainRelative, FrameGraphTextureExtent::Absolute>;

/* Deprecated:
 * Used to distinguish images from attachments.
 * An attachment has to be part of the definition of the render pass and framebuffer for a node,
 * while a texture is read during the pass and is part of a shader data definition.
 * This distinction is also important to determine which images need to be transitioned to a
 * different layout and require an image barrier.
 * We don’t need to specify the size and format of the texture here, as we had already done so
 * when we first defined the resource as an output.*/
struct FrameGraphTextureDescription : FrameGraphResourceDescriptionBase
{
    vk::Format format = vk::Format::eUndefined;
    int width = 0;
    int height = 0;
    FrameGraphTextureExtentType extent;

    FrameGraphTextureDescription(const std::string& name, vk::Format format, int width, int height) : FrameGraphResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
        this->extent = FrameGraphTextureExtent::Absolute(width, height);
    }

    FrameGraphTextureDescription(const std::string& name, vk::Format format, FrameGraphTextureExtentType&& ext) : FrameGraphResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
        this->extent = ext;
    }

    explicit FrameGraphTextureDescription(const std::string& name) : FrameGraphResourceDescriptionBase(name)
    {

    }

    FrameGraphResourceType getType() const override
    {
        return FrameGraphResourceType::Texture;
    }

    vk::Extent2D getExtent(const vkb::Swapchain& swapchain)
    {
        vk::Extent2D size;
        std::visit([&](auto&& ext) {
            using T = std::decay_t<decltype(ext)>;

            if constexpr (std::is_same_v<T, FrameGraphTextureExtent::SwapchainRelative>) {
                size.width = swapchain.extent.width * ext.wScale;
                size.height = swapchain.extent.height * ext.hScale;
            }
            else if constexpr (std::is_same_v<T, FrameGraphTextureExtent::Absolute>) {
                size.width = ext.w;
                size.height = ext.h;
            }
            }, extent);

        return size;
    }

    bool need_sample() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<FrameGraphResourceAccess::Sample>(access.accessType))
            {
                return true;
            }
        }
        return false;
    }

    bool use_as_attachment() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(access.accessType))
            {
                return true;
            }
        }
        return false;
    }

    bool need_mipmap() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<FrameGraphResourceAccess::Sample>(access.accessType))
            {
                auto sample = std::get<FrameGraphResourceAccess::Sample>(access.accessType);
                if (sample.need_mimap)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void resolveBarriers(GPUFrame* frame) override;
};

/* This type represents a storage buffer that we can write to or read from. As with
 * textures, we will need to insert memory barriers to ensure the writes from a previous pass are
 * completed before accessing the buffer data in another pass.*/
struct FrameGraphBufferDescription : FrameGraphResourceDescriptionBase
{
    void resolveBarriers(GPUFrame* frame) override;
};

using FrameGraphTexture = FrameGraphTextureDescription;
using FrameGraphBuffer = FrameGraphBufferDescription;

namespace FrameGraphConditionalVariable
{
    struct BoolVariabel
    {
        bool* val;

        BoolVariabel()
        {
            val = new bool;
        }

        bool operator()() const
        {
            return *val;
        }
    };

    struct SwitchVariable
    {
        std::string* val;

        SwitchVariable()
        {
            val = new std::string();
        }

        auto is(const std::string& _case)
        {
            return [=, val = this->val] {return _case == *val; };
        }
    };

    template<typename T1, typename T2>
    auto operator&(const T1& t1, const T2& t2)
    {
        return [=] {return t1() && t2(); };
    }

    template<typename T1, typename T2>
    auto operator|(const T1& t1, const T2& t2)
    {
        return [=] {return t1() && t2(); };
    }

    template<typename T1>
    auto operator!(const T1& t1)
    {
        return [=] {return !t1(); };
    }
}