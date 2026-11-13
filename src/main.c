#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>

#define MAX_FRAMES_IN_FLIGHT 2U

#ifdef oldVkApi
#define VkApi VK_API_VERSION_1_1
#else
#define VkApi VK_API_VERSION_1_4
#endif

VkShaderModule ansCreateShader(char* filename, VkDevice dev){
    FILE *shaderFp = fopen(filename, "rb");
    if (!shaderFp) return NULL;
    fseek(shaderFp, 0, SEEK_END);
    size_t shaderFsize = (size_t)ftell(shaderFp);
    if (shaderFsize % 4){
        fclose(shaderFp); return NULL;
    }
    rewind(shaderFp);
    uint32_t *shaderFile = malloc(shaderFsize);
    fread(shaderFile, 1, shaderFsize, shaderFp);
    fclose(shaderFp);
    
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleInfo = {0};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = shaderFsize;
        shaderModuleInfo.pCode = shaderFile;
    VkResult result = vkCreateShaderModule(dev, &shaderModuleInfo, NULL, &shaderModule);
    free(shaderFile);
    if (result) return NULL;
    return shaderModule;
}

int main(void){
    int exitCode = glfwInit();
    if (!exitCode){
        fputs("[Error] glfwInit", stderr);
        return 9;
    }
    exitCode = 0;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "AnySpace Engine Demo", NULL, NULL);

    uint32_t countExt;
    const char** VkExtns = glfwGetRequiredInstanceExtensions(&countExt);

    VkResult result;
    VkApplicationInfo appInfo = {0};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Early development demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.pEngineName = "AnySpace";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.apiVersion = VkApi;

    VkInstanceCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = countExt;
        createInfo.ppEnabledExtensionNames = VkExtns;

    VkInstance instance = {0};
    result = vkCreateInstance(&createInfo, NULL, &instance);
    if (result){
        fputs("[Error] CreateInstance\n", stderr);
        return 20; }

    VkSurfaceKHR surface;
    result = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (result){
        fputs("[Error] CreateWindowSurface\n", stderr);
        return 25; }

#ifdef multiGpu
    uint32_t devCount;
    result = vkEnumeratePhysicalDevices(instance, &devCount, NULL);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || !devCount) {
        fprintf(stderr, "Error at getting devices count\n");
        return 4;
    }
    VkPhysicalDevice *devices = malloc(devCount * sizeof(VkPhysicalDevice));
    result = vkEnumeratePhysicalDevices(instance, &devCount, devices);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || !devices) {
        free(devices);
        fprintf(stderr, "Error at getting devices\n");
        return 3;
    }
    VkPhysicalDevice physDev = devices[0]; // curently only one gpu
    free(devices);
#else
    uint32_t devCount = 1;
    VkPhysicalDevice physDev;
    result = vkEnumeratePhysicalDevices(instance, &devCount, &physDev);
    if (result){
        fputs("[Error] EnumeratePhysicalDevices\n", stderr);
        return 3;
    }
#endif
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, queueFamilies);

    uint32_t queueFamiliesId[] = {UINT32_MAX, UINT32_MAX}; // 0 - gfx, 1 - pres
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkQueueFamilyProperties queueFamily = queueFamilies[i];

        VkBool32 presentSuppuort;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(physDev, i, surface, &presentSuppuort);
        if (result){
            fputs("[Error] GetPhysicalDeviceSurfaceSupport\n", stderr);
            free(queueFamilies);
            return 11;
        }
        if (presentSuppuort)
            queueFamiliesId[1] = i;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamiliesId[0] = i;
    }
    
    free(queueFamilies);

    uint32_t countUniqueQueues = 2;
    if (queueFamiliesId[0] == queueFamiliesId[1]){
        countUniqueQueues = 1;
        if (queueFamiliesId[0] == UINT32_MAX){
            fputs("[Error] Queue not found\n", stderr);
            return 8;
        }
    }

    VkPhysicalDeviceFeatures devFeatures = {0};
    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo *queueInfos = calloc(countUniqueQueues ,sizeof(VkDeviceQueueCreateInfo));
        queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[0].queueCount = 1;
        queueInfos[0].queueFamilyIndex = queueFamiliesId[0];
        queueInfos[0].pQueuePriorities = &queuePriority;
    if (countUniqueQueues == 2){
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueCount = 1;
        queueInfos[1].queueFamilyIndex = queueFamiliesId[1];
        queueInfos[1].pQueuePriorities = &queuePriority; }

    const char* devExt[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo devInfo = {0};
        devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devInfo.enabledExtensionCount = 1;
        devInfo.ppEnabledExtensionNames = devExt;
        devInfo.pEnabledFeatures = &devFeatures;
        devInfo.queueCreateInfoCount = countUniqueQueues;
        devInfo.pQueueCreateInfos = queueInfos;

    VkDevice dev;
    result = vkCreateDevice(physDev, &devInfo, NULL, &dev);

    free(queueInfos);
    
    if (result){
        fputs("Error at creating logical device\n", stderr);
        return 7;
    }

    VkQueue gfxQueue; VkQueue presQueue;
    vkGetDeviceQueue(dev, queueFamiliesId[0], 0, &gfxQueue);
    vkGetDeviceQueue(dev, queueFamiliesId[1], 0, &presQueue);

    VkSurfaceFormatKHR sFormat = {0};
#ifdef dontCheckFormats
    sFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    sFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
#else
    uint32_t formatsCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatsCount, NULL);
    if (result){
        fputs("[Error] GetPhysicalDeviceSurfaceFormats\n", stderr);
        return 1;
    }
    VkSurfaceFormatKHR *sFormats = malloc(formatsCount * sizeof(VkSurfaceFormatKHR));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatsCount, sFormats);
    if (result|| !sFormats){
        fprintf(stderr, "Device dont support image format (1)\n");
        return 1;
    }
    char notfound = 1;
    for (uint32_t i = 0; i < formatsCount; i++) 
        if (sFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && sFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB){
            sFormat = sFormats[i];
            notfound = 0;
            break;
        }
    free(sFormats);
    if (notfound) {
        fputs("Device dont support image format (2)\n", stderr);
        return 1;
    }
#endif

    VkSurfaceCapabilitiesKHR sCap;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &sCap);
    if (result){
        fputs("[Error] GetPhysicalDeviceSurfaceCapabilities\n", stderr);
        return 19;
    }

    VkSwapchainCreateInfoKHR sChainInfo = {0};
        sChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sChainInfo.surface = surface;
        sChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sChainInfo.clipped = VK_TRUE;
        sChainInfo.oldSwapchain = VK_NULL_HANDLE;
        sChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sChainInfo.imageArrayLayers = 1;

        sChainInfo.queueFamilyIndexCount = countUniqueQueues;
        sChainInfo.pQueueFamilyIndices = queueFamiliesId;
        if (countUniqueQueues > 1)
            sChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        else
            sChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    #ifdef useVsync
        VkPresentModeKHR presMode = VK_PRESENT_MODE_FIFO_KHR; // default vsync
        uint32_t presModeCount;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &presModeCount, NULL);
        if (result){
            fputs("[Error] GetPhysicalDeviceSurfacePresentModes\n", stderr);
            return 11;
        }
        VkPresentModeKHR *presModes = malloc(presModeCount * sizeof(VkPresentModeKHR));
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &presModeCount, presModes);
        if (result || !presModes){
            fputs("[Error] GetPhysicalDeviceSurfacePresentModes + malloc\n", stderr);
            return 11;
        }
        for (uint32_t i = 0; i < presModeCount; i++)
            if (presModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                presMode = VK_PRESENT_MODE_MAILBOX_KHR; // better vsync
                break; }
        sChainInfo.presentMode = presMode;
        free(presModes);
    #else
        sChainInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // no vsync
    #endif

    if (sCap.currentExtent.width != UINT32_MAX) {
        sChainInfo.imageExtent = sCap.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        sChainInfo.imageExtent.width  = (uint32_t)w;
        sChainInfo.imageExtent.height = (uint32_t)h;
    }
        if (sCap.maxImageCount == sCap.minImageCount)
            sChainInfo.minImageCount = sCap.minImageCount;
        else
            sChainInfo.minImageCount = sCap.minImageCount + 1;
        sChainInfo.preTransform = sCap.currentTransform;
        sChainInfo.imageFormat = sFormat.format;
        sChainInfo.imageColorSpace = sFormat.colorSpace;
    
    /*printf("minImageCount=%u maxImageCount=%u currentExtent=%ux%u supportedUsage=0x%x presentMode=%d format=%d colorSpace=%d\n",
    sCap.minImageCount, sCap.maxImageCount,
    sCap.currentExtent.width, sCap.currentExtent.height,
    sCap.supportedUsageFlags, sChainInfo.presentMode,
    sChainInfo.imageFormat, sChainInfo.imageColorSpace);*/
    
    VkSwapchainKHR sChain;
    result = vkCreateSwapchainKHR(dev, &sChainInfo, NULL, &sChain);
    if (result){
        fputs("[Error] CreateSwapchain\n", stderr);
        return 17;
    }

    uint32_t scImgc;
    result = vkGetSwapchainImagesKHR(dev, sChain, &scImgc, NULL);
    if (result){
        fputs("[Error] GetSwapchainImages (1)\n", stderr);
        return 15;
    }
    //printf("%u\n", scImgc);
    VkImage *SCImgs = malloc(scImgc * sizeof(VkImage));

    result = vkGetSwapchainImagesKHR(dev, sChain, &scImgc, SCImgs);
    if (result){
        fputs("[Error] GetSwapchainImages (2)\n", stderr);
        return 15;
    }

    VkImageView *imgViews = malloc(scImgc * sizeof(VkImageView));

    for(uint32_t i = 0; i < scImgc; i++){
        VkImageViewCreateInfo imgViewInfo = {0};
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        imgViewInfo.image = SCImgs[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = sFormat.format;

        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;    
        imgViewInfo.subresourceRange.baseMipLevel = 0;
        imgViewInfo.subresourceRange.layerCount = 1;
        imgViewInfo.subresourceRange.baseArrayLayer = 0;
        imgViewInfo.subresourceRange.levelCount = 1;

        result = vkCreateImageView(dev, &imgViewInfo, NULL, imgViews+i);
        if (result){
            fputs("[Error] CreateImageView\n", stderr);
            return 21;
        }
    }

    VkAttachmentDescription colorAttachment = {0};
        colorAttachment.format = sFormat.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference coloRef = {0};
        coloRef.attachment = 0;
        coloRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subPass = {0};
        subPass.colorAttachmentCount = 1;
        subPass.pColorAttachments = &coloRef;
        subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkSubpassDependency SPDep = {0};
        SPDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SPDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SPDep.srcSubpass = VK_SUBPASS_EXTERNAL;
        SPDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //SPDep.dstSubpass = 0;
        //SPDep.srcAccessMask = 0;

    VkRenderPassCreateInfo renderPassInfo = {0};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subPass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &SPDep;

    VkRenderPass RenderPass;
    result = vkCreateRenderPass(dev, &renderPassInfo, NULL, &RenderPass);

    free(SCImgs);

    if (result){
        fputs("[Error] CreateRenderPass\n", stderr);
        return 27;
    }

    VkShaderModule vertShader = ansCreateShader("vert.spv", dev);
    if (!vertShader){
        fputs("[Error] CreateShader (vert)\n", stderr);
        return 50;
    }
    VkShaderModule fragShader = ansCreateShader("frag.spv", dev);
    if (!fragShader){
        fputs("[Error] CreateShader (frag)\n", stderr);
        return 50;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2] = {0}; // 0 - vert, 1 - frag
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShader;
        shaderStages[0].pName = "main";
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShader;
        shaderStages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertInputInfo = {0};
        vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo asmInputInfo = {0};
        asmInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        asmInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        //asmInputInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportInfo = {0};
        viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportInfo.viewportCount = 1;
        viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterInfo = {0};
        rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        //rasterInfo.depthClampEnable = VK_FALSE;
        //rasterInfo.rasterizerDiscardEnable = VK_FALSE;
        //rasterInfo.depthBiasEnable = VK_FALSE;
        //rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterInfo.lineWidth = 1.0f;
        rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multiSampleInfo = {0};
        multiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        //multiSampleInfo.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAtt = {0};
        //colorBlendAtt.blendEnable = VK_FALSE;
        colorBlendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {0};
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        //colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAtt;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {0};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = 2;
        dynamicStateInfo.pDynamicStates = dynamicStates;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        //pipelineLayoutInfo.setLayoutCount = 0;
        //pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout pipelineLayout;
    result = vkCreatePipelineLayout(dev, &pipelineLayoutInfo, NULL, &pipelineLayout);
    if (result){
        fputs("[Error] CreatePipelineLayout\n", stderr);
        return 45;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertInputInfo;
        pipelineInfo.pInputAssemblyState = &asmInputInfo;
        pipelineInfo.pViewportState = &viewportInfo;
        pipelineInfo.pRasterizationState = &rasterInfo;
        pipelineInfo.pMultisampleState = &multiSampleInfo;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = RenderPass;
       //pipelineInfo.subpass = 0;
       //pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(dev, NULL, 1, &pipelineInfo, NULL, &pipeline);

    vkDestroyShaderModule(dev, vertShader, NULL);
    vkDestroyShaderModule(dev, fragShader, NULL);
    
    if (result){
        fputs("[Error] CreateGraphicsPipelines\n", stderr);
        return 25;
    }

    VkFramebuffer *FrameBuffer = malloc(scImgc * sizeof(VkFramebuffer));
    for(uint32_t i = 0; i < scImgc; i++){
        VkFramebufferCreateInfo fbInfo = {0};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = RenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments = imgViews+i;
            fbInfo.width = sChainInfo.imageExtent.width;
            fbInfo.height = sChainInfo.imageExtent.height;
            fbInfo.layers = 1;
        result = vkCreateFramebuffer(dev, &fbInfo, NULL, FrameBuffer+i);
        if(result){
            fputs("[Error] CreateFramebuffer\n", stderr);
            return 3;
        }
    }

    VkCommandPoolCreateInfo commandPoolInfo = {0};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamiliesId[0];

    VkCommandPool CommandPool;
    result = vkCreateCommandPool(dev, &commandPoolInfo, NULL, &CommandPool);
    if(result){
        fputs("[Error] CommandPool\n", stderr);
        return 5;
    }

    VkCommandBufferAllocateInfo CBAllocInfo = {0};
        CBAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CBAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        CBAllocInfo.commandPool = CommandPool;
        //CBAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT];
    result = vkAllocateCommandBuffers(dev, &CBAllocInfo, CommandBuffers);
    if(result){
        fputs("[Error] AllocateCommandBuffers\n", stderr);
        return 4;
    }

    VkSemaphore imgAvailible[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinished[MAX_FRAMES_IN_FLIGHT];
    VkFence fences[MAX_FRAMES_IN_FLIGHT];

    VkSemaphoreCreateInfo semaphoreInfo = {0};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo = {0};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        result = vkCreateSemaphore(dev, &semaphoreInfo, NULL, imgAvailible + i);
        if (result){
            fprintf(stderr, "[Error] CreateSemaphore (imgAvailible) (%u)\n", i);
            return 35;
        }
        result = vkCreateSemaphore(dev, &semaphoreInfo, NULL, renderFinished+i);
        if (result){
            fprintf(stderr, "[Error] CreateSemaphore (renderFinished) (%u)\n", i);
            return 36;
        }
        result = vkCreateFence(dev, &fenceInfo, NULL, fences+i);
        if (result){
            fprintf(stderr, "[Error] CreateFence (%u)\n", i);
            return 37;
        }
    }

    uint32_t currentFrame = 0;
    uint32_t imgId;
    VkClearValue clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo = {0};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.signalSemaphoreCount = 1;

    VkCommandBufferBeginInfo CBbeginInfo = {0};
        CBbeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    VkRenderPassBeginInfo RPBeginInfo = {0};
        RPBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RPBeginInfo.renderPass = RenderPass;
        //RPBeginInfo.renderArea.offset.x = 0;
        //RPBeginInfo.renderArea.offset.y = 0;
        RPBeginInfo.renderArea.extent = sChainInfo.imageExtent;
        RPBeginInfo.pClearValues = &clearValue;
        RPBeginInfo.clearValueCount = 1;

    VkViewport viewport = {0};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)sChainInfo.imageExtent.width;
        viewport.height = (float)sChainInfo.imageExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
        //scissor.offset.x = 0;
        scissor.extent = sChainInfo.imageExtent;
    
    VkPresentInfoKHR presInfo = {0};
        presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presInfo.swapchainCount = 1;
        presInfo.pSwapchains = &sChain;

    puts("[Info] Init completed");

    while (!glfwWindowShouldClose(window)) {
        //printf("frame = %u\n", currentFrame);
        glfwPollEvents();

        vkWaitForFences(dev, 1, &fences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1, &fences[currentFrame]);

        result = vkAcquireNextImageKHR(dev, sChain, UINT64_MAX, imgAvailible[currentFrame], NULL, &imgId);
        if (result){
            fprintf(stderr, "[Error] vkAcquireNextImageKHR\n");
            exitCode = 6;
            break;
        }

        RPBeginInfo.framebuffer = FrameBuffer[imgId];

        result = vkResetCommandBuffer(CommandBuffers[currentFrame], 0);
        if (result){
            fputs("[Error] vkResetCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }
        result = vkBeginCommandBuffer(CommandBuffers[currentFrame], &CBbeginInfo);
        if (result){
            fputs("[Error] vkBeginCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }

        vkCmdBeginRenderPass(CommandBuffers[currentFrame], &RPBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(CommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(CommandBuffers[currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(CommandBuffers[currentFrame], 0, 1, &scissor);
        vkCmdDraw(CommandBuffers[currentFrame], 3, 1, 0, 0);

        vkCmdEndRenderPass(CommandBuffers[currentFrame]);
        result = vkEndCommandBuffer(CommandBuffers[currentFrame]);
        if (result){
            fputs("[Error] EndCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }
        submitInfo.pCommandBuffers = CommandBuffers + currentFrame;
        submitInfo.pWaitSemaphores = imgAvailible + currentFrame;
        submitInfo.pSignalSemaphores = renderFinished + currentFrame;

        presInfo.pWaitSemaphores = submitInfo.pSignalSemaphores;
        presInfo.pImageIndices = &imgId;

        result = vkQueueSubmit(gfxQueue, 1, &submitInfo, fences[currentFrame]);
        if (result){
            fputs("[Error] QueueSubmit\n", stderr);
            exitCode = 6;
            break;
        }
        
        result = vkQueuePresentKHR(presQueue, &presInfo);
        if (result){
            fputs("[Error] QueuePresentKHR\n", stderr);
            exitCode = 6;
            break;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(dev);

    for (uint32_t i = 0; i < scImgc; i++)
        vkDestroyFramebuffer(dev, FrameBuffer[i], NULL);
    vkDestroyPipeline(dev, pipeline, NULL);
    vkDestroyPipelineLayout(dev, pipelineLayout, NULL);
    vkDestroyRenderPass(dev, RenderPass, NULL);

    for (uint32_t i = 0; i < scImgc; i++)
        vkDestroyImageView(dev, imgViews[i], NULL);
    vkDestroySwapchainKHR(dev,sChain, NULL);

    free(FrameBuffer); free(imgViews);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(dev, renderFinished[i], NULL);
        vkDestroySemaphore(dev, imgAvailible[i], NULL);
        vkDestroyFence(dev, fences[i], NULL);
    }

    vkDestroyCommandPool(dev, CommandPool, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return exitCode;
}
