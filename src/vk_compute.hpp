#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>

struct VkCtx {
    VkInstance instance{};
    VkPhysicalDevice phys{};
    VkDevice device{};
    uint32_t qf{};
    VkQueue queue{};
    VkCommandPool cmdPool{};
    VkDescriptorSetLayout dsl{};
    VkPipelineLayout pipeLayout{};
    VkPipeline pipeline{};
    VkDescriptorPool descPool{};
    VkDescriptorSet dset{};
    VkBuffer outBuf{};
    VkDeviceMemory outMem{};
    size_t outSize{};
    VkCommandBuffer cmd{};
};

struct PushData { uint32_t prefix76[19]; uint32_t nonceBase; };

class VkCompute {
public:
    ~VkCompute();
    void init(uint32_t deviceIndex, const std::string& spirvPath, size_t maxItems);
    void shutdown();
    const uint32_t* dispatch(const PushData& push, uint32_t count);
private:
    VkCtx c{};
    void createInstance();
    void pickDevice(uint32_t deviceIndex);
    void createDevice();
    void createPipeline(const std::string& spirvPath);
    void createBuffers(size_t maxItems);
};
