//
// Created by 王泽远 on 2023/12/28.
//

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <memory>

#include "window.h"
#include "editorGUI.h"

static char const * AppName    = "";
static char const * EngineName = "";

#include "VulkanExtension.h"

std::shared_ptr<DeviceExtended> device;
#define MAX_FRAME_IN_FLIGHT 3

vk::Semaphore imageAvaiableSemaphores[MAX_FRAME_IN_FLIGHT];// use to sychronize swapchain and cpu
vk::Semaphore renderFinishSemaphores[MAX_FRAME_IN_FLIGHT];// use to syhchronize gpu and cpu
vk::Fence inFlightFence[MAX_FRAME_IN_FLIGHT]; //use to sychrnozie frames in the same queue

double delta_time = 0.0f;
double last_time = 0.0f;

void createSychronizeObjects()
{
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    
    for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
    {
        imageAvaiableSemaphores[i] = device->createSemaphore(semaphoreInfo);
        renderFinishSemaphores[i] = device->createSemaphore(semaphoreInfo);
        inFlightFence[i] = device->createFence(fenceInfo);
    }
}

EditorGUI editorGUI;

void drawFrame()
{
    static size_t currentFrameIdx = 0;
    constexpr auto timeout = std::numeric_limits<uint64_t>::max();
    auto swapChain = device->_swapchain.getRawVKSwapChain();
    auto waitResult = device->waitForFences(1, &inFlightFence[currentFrameIdx], vk::True, timeout);
    uint32_t imageIdx;
    auto acquireResult = device->acquireNextImageKHR(swapChain, timeout,
                                           imageAvaiableSemaphores[currentFrameIdx],VK_NULL_HANDLE,&imageIdx);
    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        device->recreateSwapchain();
        return;
    } else if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    auto gui_command = editorGUI.recordGraphicsCommand(currentFrameIdx);

    vk::CommandBuffer commandBuffers[] = { gui_command };

    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(imageAvaiableSemaphores[currentFrameIdx]);
    submitInfo.setWaitSemaphoreCount(1);
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(commandBuffers);
    submitInfo.setCommandBufferCount(std::size(commandBuffers));
    submitInfo.setSignalSemaphoreCount(1);
    submitInfo.setSignalSemaphores(renderFinishSemaphores[currentFrameIdx]);

    //render queue -> producer
    //present queue -> consumer
    //swap chain -> mailbox

    device->resetFences(inFlightFence[currentFrameIdx]);
    vk::Queue graphicsQueue = device->get_queue(vkb::QueueType::graphics).value();
    graphicsQueue.submit(submitInfo, inFlightFence[currentFrameIdx]);

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setWaitSemaphores(renderFinishSemaphores[currentFrameIdx]);
    presentInfo.setSwapchainCount(1);
    presentInfo.setSwapchains(swapChain);
    presentInfo.setImageIndices(imageIdx);

    vk::Queue presentQueue = device->get_queue(vkb::QueueType::present).value();

    auto presentResult = presentQueue.presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        device->recreateSwapchain();
    }
    else if(presentResult != vk::Result::eSuccess){
        throw std::runtime_error("Failed to present image!");
    }

    currentFrameIdx = (currentFrameIdx + 1) % MAX_FRAME_IN_FLIGHT;
}

int main( int /*argc*/, char ** /*argv*/ )
{
    Window window;

    try {
    //init glfw window
    window.init();

    //init vulkan backend
    vkb::InstanceBuilder instanceBuilder;
    auto inst = instanceBuilder
        .set_app_name("pbrt editor")
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();
    if (!inst)
        throw std::runtime_error("Failed to create Vulkan instance. Reason: " + inst.error().message());

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(inst.value(), window.getRawWindowHandle(), nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create glfw window surface!");

    vkb::PhysicalDeviceSelector phyDevSelector{ inst.value()};
    auto phy_dev = phyDevSelector
        .set_surface(surface)
        .set_minimum_version(1, 1)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();

    if(!phy_dev)
        throw std::runtime_error("Failed to find suitable physicalDevice. Reason: " + phy_dev.error().message());

    vkb::DeviceBuilder deviceBuilder{ phy_dev.value() };
    auto device_optional = deviceBuilder.build();

    if (!device_optional)
        throw std::runtime_error("Failed to create logical device. Reason: " + device_optional.error().message());

    device = std::make_shared<DeviceExtended>(device_optional.value(),inst.value().instance);

    vkb::SwapchainBuilder swapchainBuilder{device_optional.value()};
    auto swapChain = swapchainBuilder
        .set_old_swapchain(VK_NULL_HANDLE)
        .set_desired_min_image_count(MAX_FRAME_IN_FLIGHT)
        .build();

    if (!swapChain)
        throw std::runtime_error("Failed to create swapchain. Reason: " + swapChain.error().message());

    device->setSwapchain(swapChain.value());

    window.registryFramebufferResizeCallback([=](int width, int height) {
        device->recreateSwapchain();
    });

    }catch (std::runtime_error & err)
    {
        std::cerr << err.what() << std::endl;
    }

    createSychronizeObjects();

    editorGUI.init(window.getRawWindowHandle(), device);

    while (!window.shouldClose()) {
        window.pollEvents();
        double current_time = glfwGetTime();
        delta_time = current_time - last_time;
        last_time = current_time;
        editorGUI.constructFrame();
        drawFrame();
    }
    
    device->waitIdle();
    return 0;
}