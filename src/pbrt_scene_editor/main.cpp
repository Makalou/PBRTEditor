//
// Created by 王泽远 on 2023/12/28.
//

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <memory>

#include "window.h"
#include "editorGUI.h"
#include "sceneViewer.hpp"

static char const * AppName    = "";
static char const * EngineName = "";

#include "VulkanExtension.h"
#include "ShaderManager.h"

std::shared_ptr<DeviceExtended> device;
#define MAX_FRAME_IN_FLIGHT 3

vk::Semaphore imageAvailableSemaphores[MAX_FRAME_IN_FLIGHT];// use to synchronize swapchain and cpu
vk::Semaphore renderFinishSemaphores[MAX_FRAME_IN_FLIGHT];// use to synchronize rendering and presentation
vk::Fence inFlightFrameFence[MAX_FRAME_IN_FLIGHT]; //use to synchronize CPU and GPU frame resource access

double delta_time = 0.0f;
double last_time = 0.0f;

void createSynchronizeObjects()
{
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    
    for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
    {
        imageAvailableSemaphores[i] = device->createSemaphore(semaphoreInfo);
        renderFinishSemaphores[i] = device->createSemaphore(semaphoreInfo);
        inFlightFrameFence[i] = device->createFence(fenceInfo);
    }
}

EditorGUI editorGUI;
SceneViewer viewer;

auto waitForFence(vk::Fence* fence, uint64_t timeout)
{
    return device->waitForFences(1, fence, vk::True, timeout);
}

void drawFrame()
{
    //https://app.diagrams.net/#G1e5FP16h8o5Py-69lOYuoYUjs2gC5e5ae
    static size_t currentFrameIdx = 0; //range in [0, MAX_FRAME_IN_FLIGHT)
    constexpr static auto timeout = std::numeric_limits<uint64_t>::max();
    auto swapChain = device->_swapchain.getRawVKSwapChain();

    // wait for commands for current frame completion.
    // For example, if triple buffering is enable, then frame 3 should wait the "frame resource" for frame 0 to finish execution
    // to be able re-use them or recycle them.
    // Note if we use wait device idle here, then every iteration will be blocked here until the previous frame finish execution
    // Also note that we are waiting on the graphics operations instead of the presenting. So it's possible that when we acquireNextImage
    // and record command buffer or even submit them, the presenting queue is still doing the present task.
    // But it's actually OK because the imageAvailable semaphore will make sure that the actual execution of submitted command buffers
    // would only happen after presenting has finished.
    //auto begin = std::chrono::steady_clock::now();

    auto waitResult = waitForFence(&inFlightFrameFence[currentFrameIdx],timeout);

    //auto end = std::chrono::steady_clock::now();
    //auto microseconds_count = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    // If CPU often waits for GPU, it means that the application is GPU bound.
    //std::cout <<  microseconds_count << std::endl;

    uint32_t imageIdx;
    auto acquireResult = device->acquireNextImageKHR(swapChain, timeout,
                                           imageAvailableSemaphores[currentFrameIdx],VK_NULL_HANDLE,&imageIdx);
    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        device->recreateSwapchain();
        return;
    } else if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    auto viewer_command = viewer.recordGraphicsCommand(currentFrameIdx);
    auto gui_command = editorGUI.recordGraphicsCommand(currentFrameIdx);

    vk::CommandBuffer commandBuffers[] = { viewer_command, gui_command };

    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(imageAvailableSemaphores[currentFrameIdx]);
    submitInfo.setWaitSemaphoreCount(1);
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(commandBuffers);
    submitInfo.setCommandBufferCount(std::size(commandBuffers));
    submitInfo.setSignalSemaphoreCount(1);
    submitInfo.setSignalSemaphores(renderFinishSemaphores[currentFrameIdx]);

    device->resetFences(inFlightFrameFence[currentFrameIdx]);
    vk::Queue graphicsQueue = device->get_queue(vkb::QueueType::graphics).value();
    // All submitted commands in a submitInfo won't start execution until corresponding wait semaphores have been signaled.
    // Once all submitted commands in a submitInfo complete, corresponding signalSemaphores will be signaled.
    // When all submitted commands in all submitInfos complete, fence will be signaled.
    // When there's only one submitInfo, the differences of signal semaphores and fence may be not so obvious.
    // Note if there're very complicated multiple queues dependency relationship, the inFlightFrameFence should wait on
    // the last queue submit on the dependency graph. Dependency between other submission should be expressed by semaphore.
    // Command buffer submissions to a single queue respect submission order and other implicit ordering guarantees.
    graphicsQueue.submit(submitInfo, inFlightFrameFence[currentFrameIdx]);

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setWaitSemaphores(renderFinishSemaphores[currentFrameIdx]);
    presentInfo.setSwapchainCount(1);
    presentInfo.setSwapchains(swapChain);
    presentInfo.setImageIndices(imageIdx);

    vk::Queue presentQueue = device->get_queue(vkb::QueueType::present).value();
    // Present current frame. must wait until the current frame has finished rendering.
    // Here is a good example to demonstrate that semaphores are finer synchronization primitive than fence.
    // This is because "submission" is a "CPU operation" while "execution" is a "GPU operation".
    // Here, we can continuously submit commands(even into different queues) without any synchronization
    // but still make sure their execution order thanks to semaphores.
    // Presentation requests sent to a particular queue are always performed in order.
    auto presentResult = presentQueue.presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        device->recreateSwapchain();
    }
    else if(presentResult != vk::Result::eSuccess){
        throw std::runtime_error("Failed to present image!");
    }

    currentFrameIdx = (currentFrameIdx + 1) % MAX_FRAME_IN_FLIGHT;
}

bool g_support_bindless;
bool g_support_ray_tracing;

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
        .require_api_version(1,2)
        .set_minimum_instance_version(1,2)
        .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
        .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
        //.request_validation_layers()
        //.use_default_debug_messenger()
        .build();
    if (!inst)
        throw std::runtime_error("Failed to create Vulkan instance. Reason: " + inst.error().message());

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(inst.value(), window.getRawWindowHandle(), nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create glfw window surface!");

    vkb::PhysicalDeviceSelector phyDevSelector{ inst.value()};

    auto required_device_extension = {VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                                      VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME};

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR deviceAddressFeaturesKhr{};
    deviceAddressFeaturesKhr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    deviceAddressFeaturesKhr.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;

    auto phy_dev = phyDevSelector
        .set_surface(surface)
        .set_minimum_version(1, 2)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .add_required_extensions(required_device_extension)
        .add_required_extension_features(deviceAddressFeaturesKhr)
        .add_required_extension_features(descriptorIndexingFeatures)
        .select();

        if(!phy_dev)
        {
            throw std::runtime_error("Failed to find suitable physicalDevice. Reason: " + phy_dev.error().message());
        }


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

    Window::registerFramebufferResizeCallback([=](int width, int height) {
        device->recreateSwapchain();
    });

    }catch (std::runtime_error & err)
    {
        std::cerr << err.what() << std::endl;
    }

    createSynchronizeObjects();

    viewer.init(device);
    editorGUI.init(window.getRawWindowHandle(), device);
    editorGUI.viewer = &viewer;

    //main loop
    while (!window.shouldClose()) {
        window.pollEvents();
        double current_time = glfwGetTime();
        delta_time = current_time - last_time;
        last_time = current_time;
        editorGUI.constructFrame();
        viewer.constructFrame();
        drawFrame();
    }
    
    device->waitIdle();
    return 0;
}