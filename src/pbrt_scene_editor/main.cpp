//
// Created by 王泽远 on 2023/12/28.
//

#include <vulkan/vulkan.hpp>
#include <iostream>

static char const * AppName    = "";
static char const * EngineName = "";

struct DeviceExtended : vk::Device
{
    DeviceExtended(vk::Device device) : vk_device(device){}
    vk::Device vk_device;
};

int main( int /*argc*/, char ** /*argv*/ )
{
    try
    {
        vk::InstanceCreateInfo instance_info;

        vk::Instance instance = vk::createInstance(instance_info);

        auto physicalDevices = instance.enumeratePhysicalDevices();

        if(physicalDevices.empty()) throw std::exception();

        auto physicalDevice = physicalDevices.front();

        /* VULKAN_HPP_KEY_START */

        // get the QueueFamilyProperties of the first PhysicalDevice
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        auto queueFamilyIterator = [](std::vector<vk::QueueFamilyProperties> & queueFamilyProperties,vk::QueueFlagBits queueFlag){
            return std::find_if( queueFamilyProperties.begin(),
                          queueFamilyProperties.end(),
                          [=]( vk::QueueFamilyProperties const & qfp ) { return qfp.queueFlags & queueFlag; } );
        };

        size_t graphicsQueueFamilyIndex = std::distance( queueFamilyProperties.begin(), queueFamilyIterator(queueFamilyProperties,vk::QueueFlagBits::eGraphics) );
        assert( graphicsQueueFamilyIndex < queueFamilyProperties.size() );

        size_t computeQueueFamilyIndex = std::distance( queueFamilyProperties.begin(), queueFamilyIterator(queueFamilyProperties,vk::QueueFlagBits::eCompute) );
        assert( computeQueueFamilyIndex < queueFamilyProperties.size() );

        size_t transferQueueFamilyIndex = std::distance( queueFamilyProperties.begin(), queueFamilyIterator(queueFamilyProperties,vk::QueueFlagBits::eTransfer) );
        assert( transferQueueFamilyIndex < queueFamilyProperties.size() );

        // create a Device
        float                     queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo( vk::DeviceQueueCreateFlags(), static_cast<uint32_t>( graphicsQueueFamilyIndex ), 1, &queuePriority );
        DeviceExtended                device = physicalDevice.createDevice( vk::DeviceCreateInfo( vk::DeviceCreateFlags(), deviceQueueCreateInfo ) );

        // destroy the device
        device.destroy();
        /* VULKAN_HPP_KEY_END */
        instance.destroy();
    }
    catch ( vk::SystemError & err )
    {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit( -1 );
    }
    catch ( std::exception & err )
    {
        std::cout << "std::exception: " << err.what() << std::endl;
        exit( -1 );
    }
    catch ( ... )
    {
        std::cout << "unknown error\n";
        exit( -1 );
    }
    return 0;
}