#include "vk_compute.hpp"
#include <vector>
#include <stdexcept>
#include <fstream>

static std::vector<char> readFile(const std::string& path){
    std::ifstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("open "+path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

VkCompute::~VkCompute(){ shutdown(); }

void VkCompute::createInstance(){
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="janusv"; app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app;
    if(vkCreateInstance(&ci,nullptr,&c.instance)!=VK_SUCCESS) throw std::runtime_error("vkCreateInstance");
}
void VkCompute::pickDevice(uint32_t deviceIndex){
    uint32_t n=0; vkEnumeratePhysicalDevices(c.instance,&n,nullptr); if(!n) throw std::runtime_error("no devices");
    std::vector<VkPhysicalDevice> devs(n); vkEnumeratePhysicalDevices(c.instance,&n,devs.data());
    if(deviceIndex>=n) deviceIndex=0; c.phys=devs[deviceIndex];
    uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(c.phys,&qn,nullptr);
    std::vector<VkQueueFamilyProperties> p(qn); vkGetPhysicalDeviceQueueFamilyProperties(c.phys,&qn,p.data());
    for(uint32_t i=0;i<qn;i++) if(p[i].queueFlags & VK_QUEUE_COMPUTE_BIT){ c.qf=i; break; }
}
void VkCompute::createDevice(){
    float pr=1.0f; VkDeviceQueueCreateInfo dq{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    dq.queueFamilyIndex=c.qf; dq.queueCount=1; dq.pQueuePriorities=&pr;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&dq;
    if(vkCreateDevice(c.phys,&dci,nullptr,&c.device)!=VK_SUCCESS) throw std::runtime_error("vkCreateDevice");
    vkGetDeviceQueue(c.device,c.qf,0,&c.queue);
    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.queueFamilyIndex=c.qf; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(c.device,&cp,nullptr,&c.cmdPool);
}
void VkCompute::createPipeline(const std::string& spv){
    auto code=readFile(spv);
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; sm.codeSize=code.size(); sm.pCode=(const uint32_t*)code.data();
    VkShaderModule mod; if(vkCreateShaderModule(c.device,&sm,nullptr,&mod)!=VK_SUCCESS) throw std::runtime_error("shader module");

    VkDescriptorSetLayoutBinding b{}; b.binding=0; b.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; b.descriptorCount=1; b.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dl.bindingCount=1; dl.pBindings=&b;
    vkCreateDescriptorSetLayout(c.device,&dl,nullptr,&c.dsl);

    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=sizeof(PushData);
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pl.setLayoutCount=1; pl.pSetLayouts=&c.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr;
    vkCreatePipelineLayout(c.device,&pl,nullptr,&c.pipeLayout);

    VkPipelineShaderStageCreateInfo ss{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ss.stage=VK_SHADER_STAGE_COMPUTE_BIT; ss.module=mod; ss.pName="main";
    VkComputePipelineCreateInfo pc{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; pc.stage=ss; pc.layout=c.pipeLayout;
    if(vkCreateComputePipelines(c.device,VK_NULL_HANDLE,1,&pc,nullptr,&c.pipeline)!=VK_SUCCESS) throw std::runtime_error("compute pipeline");
    vkDestroyShaderModule(c.device,mod,nullptr);

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpi.maxSets=1; dpi.poolSizeCount=1; dpi.pPoolSizes=&ps;
    vkCreateDescriptorPool(c.device,&dpi,nullptr,&c.descPool);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool=c.descPool; ai.descriptorSetCount=1; ai.pSetLayouts=&c.dsl;
    vkAllocateDescriptorSets(c.device,&ai,&c.dset);
}
void VkCompute::createBuffers(size_t maxItems){
    c.outSize = maxItems * 8 * sizeof(uint32_t);
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=c.outSize; bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    vkCreateBuffer(c.device,&bi,nullptr,&c.outBuf);
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(c.device,c.outBuf,&mr);
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(c.phys,&mp);
    uint32_t idx=0; for(uint32_t i=0;i<mp.memoryTypeCount;i++) if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){ idx=i; break; }
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; mai.allocationSize=mr.size; mai.memoryTypeIndex=idx;
    vkAllocateMemory(c.device,&mai,nullptr,&c.outMem);
    vkBindBufferMemory(c.device,c.outBuf,c.outMem,0);
    VkDescriptorBufferInfo dbi{c.outBuf,0,c.outSize};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; w.dstSet=c.dset; w.dstBinding=0; w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.descriptorCount=1; w.pBufferInfo=&dbi;
    vkUpdateDescriptorSets(c.device,1,&w,0,nullptr);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cai.commandPool=c.cmdPool; cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount=1;
    vkAllocateCommandBuffers(c.device,&cai,&c.cmd);
}
void VkCompute::init(uint32_t deviceIndex,const std::string& spv,size_t maxItems){ createInstance(); pickDevice(deviceIndex); createDevice(); createPipeline(spv); createBuffers(maxItems); }
const uint32_t* VkCompute::dispatch(const PushData& push, uint32_t count){
    vkResetCommandBuffer(c.cmd,0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(c.cmd,&bi);
    vkCmdBindPipeline(c.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,c.pipeline);
    vkCmdBindDescriptorSets(c.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,c.pipeLayout,0,1,&c.dset,0,nullptr);
    vkCmdPushConstants(c.cmd,c.pipeLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(PushData),&push);
    uint32_t groups=(count+255)/256; vkCmdDispatch(c.cmd,groups,1,1);
    vkEndCommandBuffer(c.cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&c.cmd; vkQueueSubmit(c.queue,1,&si,VK_NULL_HANDLE); vkQueueWaitIdle(c.queue);
    void* mapped=nullptr; vkMapMemory(c.device,c.outMem,0,c.outSize,0,&mapped); return (const uint32_t*)mapped;
}
void VkCompute::shutdown(){
    if(!c.device) return;
    vkDeviceWaitIdle(c.device);
    if(c.cmd) vkFreeCommandBuffers(c.device,c.cmdPool,1,&c.cmd);
    if(c.outMem) vkFreeMemory(c.device,c.outMem,nullptr);
    if(c.outBuf) vkDestroyBuffer(c.device,c.outBuf,nullptr);
    if(c.descPool) vkDestroyDescriptorPool(c.device,c.descPool,nullptr);
    if(c.pipeline) vkDestroyPipeline(c.device,c.pipeline,nullptr);
    if(c.pipeLayout) vkDestroyPipelineLayout(c.device,c.pipeLayout,nullptr);
    if(c.dsl) vkDestroyDescriptorSetLayout(c.device,c.dsl,nullptr);
    if(c.cmdPool) vkDestroyCommandPool(c.device,c.cmdPool,nullptr);
    if(c.device) vkDestroyDevice(c.device,nullptr);
    if(c.instance) vkDestroyInstance(c.instance,nullptr);
    c = VkCtx{};
}
