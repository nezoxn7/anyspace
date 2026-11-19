#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>

#define MAX_FRAMES_IN_FLIGHT 2U
#define CFG_MAX_STR_LEN 32

typedef struct LibsData{
    // 4 bytes
    uint32_t scImgc; uint32_t countUniqueQueues;
    // 8 bytes
    GLFWwindow* win; VkInstance instance; VkSurfaceKHR surface;
    VkPhysicalDevice physDev; VkDevice dev;
    uint32_t queueFamiliesId[2]; VkQueue gfxQueue; VkQueue presQueue; VkSurfaceFormatKHR surfFormat;
    VkImageView *imgViews;  VkExtent2D imgExtent; VkSwapchainKHR swapchain; VkFramebuffer *FrameBuffer;
    VkRenderPass RenderPass; VkPipeline pipeline; VkPipelineLayout pipelineLayout;
    VkCommandPool CommandPool;
    // 16 bytes
    VkSemaphore imgAvailible[MAX_FRAMES_IN_FLIGHT]; VkSemaphore renderFinished[MAX_FRAMES_IN_FLIGHT];
    VkFence fences[MAX_FRAMES_IN_FLIGHT]; VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT];
    // >16
    VkSurfaceCapabilitiesKHR surfCap;
} LibsData;

typedef struct AnsConfig{
    int WinWidth;
    int WinHeight;
    uint32_t VkVersion;
    int GpuId;
    VkPresentModeKHR Vsync;
} AnsConfig;

int AnsMkWinInstSurf(LibsData *ld, AnsConfig *ansCfg){
    int exitCode = glfwInit();
    if (!exitCode){
        fputs("[Error] glfwInit", stderr);
        return 9;
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ld->win = glfwCreateWindow(ansCfg->WinWidth, ansCfg->WinHeight, "AnySpace Engine Demo", NULL, NULL);

    uint32_t countExt;
    const char** VkExtns = glfwGetRequiredInstanceExtensions(&countExt);

    VkResult result;
    VkApplicationInfo appInfo = {0};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Early development demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.pEngineName = "AnySpace";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.apiVersion = ansCfg->VkVersion;

    VkInstanceCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = countExt;
        createInfo.ppEnabledExtensionNames = VkExtns;

    result = vkCreateInstance(&createInfo, NULL, &ld->instance);
    if (result){
        fputs("[Error] CreateInstance\n", stderr);
        return 20; }

    result = glfwCreateWindowSurface(ld->instance, ld->win, NULL, &ld->surface);
    if (result){
        fputs("[Error] CreateWindowSurface\n", stderr);
        return 25; }
    return 0;
}

int AnsDevInit(LibsData *ld, int GpuId){
    #ifdef MULTI_GPU
    uint32_t devCount;
    VkResult result = vkEnumeratePhysicalDevices(ld->instance, &devCount, NULL);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || !devCount) {
        fprintf(stderr, "[Error] EnumeratePhysicalDevices\n");
        return 4;
    }
    VkPhysicalDevice *devices = malloc(devCount * sizeof(VkPhysicalDevice));
    result = vkEnumeratePhysicalDevices(ld->instance, &devCount, devices);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || !devices) {
        free(devices);
        fprintf(stderr, "[Error] EnumeratePhysicalDevices + malloc\n");
        return 4;
    }
    ld->physDev = devices[GpuId];
    free(devices);
#else
    uint32_t devCount = 1;
    VkResult result = vkEnumeratePhysicalDevices(ld->instance, &devCount, &ld->physDev);
    if (result){
        fputs("[Error] EnumeratePhysicalDevices\n", stderr);
        return 3;
    }
#endif
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(ld->physDev, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ld->physDev, &queueFamilyCount, queueFamilies);

    // 0 - gfx, 1 - pres
    ld->queueFamiliesId[0] = UINT32_MAX; ld->queueFamiliesId[1] = UINT32_MAX; 
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkQueueFamilyProperties queueFamily = queueFamilies[i];
        VkBool32 presentSuppuort;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(ld->physDev, i, ld->surface, &presentSuppuort);
        if (result){
            fputs("[Error] GetPhysicalDeviceSurfaceSupport\n", stderr);
            free(queueFamilies);
            return 11;
        }
        if (presentSuppuort)
            ld->queueFamiliesId[1] = i;
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            ld->queueFamiliesId[0] = i;
    }
    
    free(queueFamilies);

    ld->countUniqueQueues = 2;
    if (ld->queueFamiliesId[0] == ld->queueFamiliesId[1]){
        ld->countUniqueQueues = 1;
        if (ld->queueFamiliesId[0] == UINT32_MAX){
            fputs("[Error] Queue not found\n", stderr);
            return 8;
        }
    }

    VkPhysicalDeviceFeatures devFeatures = {0};
    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo *queueInfos = calloc(ld->countUniqueQueues ,sizeof(VkDeviceQueueCreateInfo));
        queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[0].queueCount = 1;
        queueInfos[0].queueFamilyIndex = ld->queueFamiliesId[0];
        queueInfos[0].pQueuePriorities = &queuePriority;
    if (ld->countUniqueQueues == 2){
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueCount = 1;
        queueInfos[1].queueFamilyIndex = ld->queueFamiliesId[1];
        queueInfos[1].pQueuePriorities = &queuePriority; }

    const char* devExt[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo devInfo = {0};
        devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devInfo.enabledExtensionCount = 1;
        devInfo.ppEnabledExtensionNames = devExt;
        devInfo.pEnabledFeatures = &devFeatures;
        devInfo.queueCreateInfoCount = ld->countUniqueQueues;
        devInfo.pQueueCreateInfos = queueInfos;

    result = vkCreateDevice(ld->physDev, &devInfo, NULL, &ld->dev);

    free(queueInfos);
    
    if (result){
        fputs("Error at creating logical device\n", stderr);
        return 7;
    }
    
    vkGetDeviceQueue(ld->dev, ld->queueFamiliesId[0], 0, &ld->gfxQueue);
    vkGetDeviceQueue(ld->dev, ld->queueFamiliesId[1], 0, &ld->presQueue);
    return 0;
}

int ansInitSurface(LibsData *ld){
    VkResult result;
#ifdef dontCheckFormats
    ld->surfFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    ld->surfFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
#else
    uint32_t formatsCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ld->physDev, ld->surface, &formatsCount, NULL);
    if (result){
        fputs("[Error] GetPhysicalDeviceSurfaceFormats\n", stderr);
        return 1;
    }
    VkSurfaceFormatKHR *sFormats = malloc(formatsCount * sizeof(VkSurfaceFormatKHR));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ld->physDev, ld->surface, &formatsCount, sFormats);
    if (result|| !sFormats){
        fputs("Device dont support image format (1)\n", stderr);
        return 1;
    }
    char notfound = 1;
    for (uint32_t i = 0; i < formatsCount; i++) 
        if (sFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && sFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB){
            ld->surfFormat = sFormats[i];
            notfound = 0;
            break;
        }
    free(sFormats);
    if (notfound) {
        fputs("Device dont support image format (2)\n", stderr);
        return 1;
    }
#endif
    
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ld->physDev, ld->surface, &ld->surfCap);
    if (result){
        fputs("[Error] GetPhysicalDeviceSurfaceCapabilities\n", stderr);
        return 19;
    }
    return 0;
}

int ansMkSwapChain(LibsData *ld, VkPresentModeKHR vsync){
    VkResult result;
    VkSwapchainCreateInfoKHR sChainInfo = {0};
        sChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sChainInfo.surface = ld->surface;
        sChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sChainInfo.clipped = VK_TRUE;
        sChainInfo.oldSwapchain = VK_NULL_HANDLE;
        sChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sChainInfo.imageArrayLayers = 1;

        sChainInfo.queueFamilyIndexCount = ld->countUniqueQueues;
        sChainInfo.pQueueFamilyIndices = ld->queueFamiliesId;
        if (ld->countUniqueQueues > 1)
            sChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        else
            sChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    #ifdef oldVsyncCode
        VkPresentModeKHR presMode = VK_PRESENT_MODE_FIFO_KHR; // default vsync
        uint32_t presModeCount;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(ld->physDev, surface, &presModeCount, NULL);
        if (result){
            fputs("[Error] GetPhysicalDeviceSurfacePresentModes\n", stderr);
            return 11;
        }
        VkPresentModeKHR *presModes = malloc(presModeCount * sizeof(VkPresentModeKHR));
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(ld->physDev, surface, &presModeCount, presModes);
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
        sChainInfo.presentMode = vsync;
    #endif
    
    if (ld->surfCap.currentExtent.width != UINT32_MAX)
        sChainInfo.imageExtent = ld->surfCap.currentExtent;
    else {
        int w, h;
        glfwGetFramebufferSize(ld->win, &w, &h);
        sChainInfo.imageExtent.width  = (uint32_t)w;
        sChainInfo.imageExtent.height = (uint32_t)h;
    }
    ld->imgExtent = sChainInfo.imageExtent;
    
    if (ld->surfCap.maxImageCount == ld->surfCap.minImageCount)
        sChainInfo.minImageCount = ld->surfCap.minImageCount;
    else
        sChainInfo.minImageCount = ld->surfCap.minImageCount + 1;
    sChainInfo.preTransform = ld->surfCap.currentTransform;
    sChainInfo.imageFormat = ld->surfFormat.format;
    sChainInfo.imageColorSpace = ld->surfFormat.colorSpace;
    
    /*printf("minImageCount=%u maxImageCount=%u currentExtent=%ux%u supportedUsage=0x%x presentMode=%d format=%d colorSpace=%d\n",
    sCap.minImageCount, sCap.maxImageCount,
    sCap.currentExtent.width, sCap.currentExtent.height,
    sCap.supportedUsageFlags, sChainInfo.presentMode,
    sChainInfo.imageFormat, sChainInfo.imageColorSpace);*/
    
    
    result = vkCreateSwapchainKHR(ld->dev, &sChainInfo, NULL, &ld->swapchain);
    if (result){
        fputs("[Error] CreateSwapchain\n", stderr);
        return 17;
    }

    result = vkGetSwapchainImagesKHR(ld->dev, ld->swapchain, &ld->scImgc, NULL);
    if (result){
        fputs("[Error] GetSwapchainImages (1)\n", stderr);
        return 15;
    }
    //printf("%u\n", scImgc);
    VkImage *SCImgs = malloc(ld->scImgc * sizeof(VkImage));

    result = vkGetSwapchainImagesKHR(ld->dev, ld->swapchain, &ld->scImgc, SCImgs);
    if (result){
        fputs("[Error] GetSwapchainImages (2)\n", stderr);
        return 15;
    }

    ld->imgViews = malloc(ld->scImgc * sizeof(VkImageView));

    VkImageViewCreateInfo imgViewInfo = {0};
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;    
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = ld->surfFormat.format;

        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;    
        imgViewInfo.subresourceRange.baseMipLevel = 0;
        imgViewInfo.subresourceRange.layerCount = 1;
        imgViewInfo.subresourceRange.baseArrayLayer = 0;
        imgViewInfo.subresourceRange.levelCount = 1;

    for(uint32_t i = 0; i < ld->scImgc; i++){
        imgViewInfo.image = SCImgs[i];
        result = vkCreateImageView(ld->dev, &imgViewInfo, NULL, ld->imgViews+i);
        if (result){
            fputs("[Error] CreateImageView\n", stderr);
            return 21;
        }
    }
    free(SCImgs);
    return 0;
}

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

int ansMkRenderPassPipeline(LibsData *ld){
    VkAttachmentDescription colorAttachment = {0};
        colorAttachment.format = ld->surfFormat.format;
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
    
    VkResult result = vkCreateRenderPass(ld->dev, &renderPassInfo, NULL, &ld->RenderPass);

    if (result){
        fputs("[Error] CreateRenderPass\n", stderr);
        return 27;
    }
    VkShaderModule vertShader = ansCreateShader("vert.spv", ld->dev);
    if (!vertShader){
        fputs("[Error] CreateShader (vert)\n", stderr);
        return 50;
    }
    VkShaderModule fragShader = ansCreateShader("frag.spv", ld->dev);
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

    result = vkCreatePipelineLayout(ld->dev, &pipelineLayoutInfo, NULL, &ld->pipelineLayout);
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
        pipelineInfo.layout = ld->pipelineLayout;
        pipelineInfo.renderPass = ld->RenderPass;
       //pipelineInfo.subpass = 0;
       //pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    result = vkCreateGraphicsPipelines(ld->dev, NULL, 1, &pipelineInfo, NULL, &ld->pipeline);

    vkDestroyShaderModule(ld->dev, vertShader, NULL);
    vkDestroyShaderModule(ld->dev, fragShader, NULL);
    
    if (result){
        fputs("[Error] CreateGraphicsPipelines\n", stderr);
        return 25;
    }
    return 0;
}

int ansMkFrameBuffers(LibsData *ld){
    ld->FrameBuffer = malloc(ld->scImgc * sizeof(VkFramebuffer));
    VkFramebufferCreateInfo fbInfo = {0};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = ld->RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.width = ld->imgExtent.width;
        fbInfo.height = ld->imgExtent.height;
        fbInfo.layers = 1;
    for(uint32_t i = 0; i < ld->scImgc; i++){
        fbInfo.pAttachments = ld->imgViews+i;
        
        VkResult result = vkCreateFramebuffer(ld->dev, &fbInfo, NULL, ld->FrameBuffer+i);
        if(result){
            fputs("[Error] CreateFramebuffer\n", stderr);
            return 3;
        }
    }
    return 0;
}

int ansMkCommand(LibsData *ld){
     VkCommandPoolCreateInfo commandPoolInfo = {0};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = ld->queueFamiliesId[0];

    
    VkResult result = vkCreateCommandPool(ld->dev, &commandPoolInfo, NULL, &ld->CommandPool);
    if(result){
        fputs("[Error] CommandPool\n", stderr);
        return 5;
    }

    VkCommandBufferAllocateInfo CBAllocInfo = {0};
        CBAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CBAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        CBAllocInfo.commandPool = ld->CommandPool;
        //CBAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    result = vkAllocateCommandBuffers(ld->dev, &CBAllocInfo, ld->CommandBuffers);
    if(result){
        fputs("[Error] AllocateCommandBuffers\n", stderr);
        return 4;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {0};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo = {0};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        result = vkCreateSemaphore(ld->dev, &semaphoreInfo, NULL, ld->imgAvailible + i);
        if (result){
            fprintf(stderr, "[Error] CreateSemaphore (imgAvailible) (%u)\n", i);
            return 35;
        }
        result = vkCreateSemaphore(ld->dev, &semaphoreInfo, NULL, ld->renderFinished+i);
        if (result){
            fprintf(stderr, "[Error] CreateSemaphore (renderFinished) (%u)\n", i);
            return 36;
        }
        result = vkCreateFence(ld->dev, &fenceInfo, NULL, ld->fences+i);
        if (result){
            fprintf(stderr, "[Error] CreateFence (%u)\n", i);
            return 37;
        }
    }
    return 0;
}

int ansRunCycle(LibsData *ld){
    VkResult result; int exitCode = 0;
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
        RPBeginInfo.renderPass = ld->RenderPass;
        //RPBeginInfo.renderArea.offset.x = 0;
        //RPBeginInfo.renderArea.offset.y = 0;
        RPBeginInfo.renderArea.extent = ld->imgExtent;
        RPBeginInfo.pClearValues = &clearValue;
        RPBeginInfo.clearValueCount = 1;

    VkViewport viewport = {0};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)ld->imgExtent.width;
        viewport.height = (float)ld->imgExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
        //scissor.offset.x = 0;
        scissor.extent = ld->imgExtent;
    
    VkPresentInfoKHR presInfo = {0};
        presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presInfo.swapchainCount = 1;
        presInfo.pSwapchains = &ld->swapchain;

    while (!glfwWindowShouldClose(ld->win)) {
        //printf("frame = %u\n", currentFrame);
        glfwPollEvents();

        vkWaitForFences(ld->dev, 1, ld->fences+currentFrame, VK_TRUE, UINT64_MAX);
        vkResetFences(ld->dev, 1, ld->fences+currentFrame);

        result = vkAcquireNextImageKHR(ld->dev, ld->swapchain, UINT64_MAX, ld->imgAvailible[currentFrame], NULL, &imgId);
        if (result){
            fprintf(stderr, "[Error] vkAcquireNextImageKHR\n");
            exitCode = 6;
            break;
        }

        RPBeginInfo.framebuffer = ld->FrameBuffer[imgId];

        result = vkResetCommandBuffer(ld->CommandBuffers[currentFrame], 0);
        if (result){
            fputs("[Error] vkResetCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }
        result = vkBeginCommandBuffer(ld->CommandBuffers[currentFrame], &CBbeginInfo);
        if (result){
            fputs("[Error] vkBeginCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }

        vkCmdBeginRenderPass(ld->CommandBuffers[currentFrame], &RPBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(ld->CommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ld->pipeline);
        vkCmdSetViewport(ld->CommandBuffers[currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(ld->CommandBuffers[currentFrame], 0, 1, &scissor);
        vkCmdDraw(ld->CommandBuffers[currentFrame], 3, 1, 0, 0);

        vkCmdEndRenderPass(ld->CommandBuffers[currentFrame]);
        result = vkEndCommandBuffer(ld->CommandBuffers[currentFrame]);
        if (result){
            fputs("[Error] EndCommandBuffer\n", stderr);
            exitCode = 6;
            break;
        }
        submitInfo.pCommandBuffers = ld->CommandBuffers + currentFrame;
        submitInfo.pWaitSemaphores = ld->imgAvailible + currentFrame;
        submitInfo.pSignalSemaphores = ld->renderFinished + currentFrame;

        presInfo.pWaitSemaphores = submitInfo.pSignalSemaphores;
        presInfo.pImageIndices = &imgId;

        result = vkQueueSubmit(ld->gfxQueue, 1, &submitInfo, ld->fences[currentFrame]);
        if (result){
            fputs("[Error] QueueSubmit\n", stderr);
            exitCode = 6;
            break;
        }
        
        result = vkQueuePresentKHR(ld->presQueue, &presInfo);
        if (result){
            fputs("[Error] QueuePresentKHR\n", stderr);
            exitCode = 6;
            break;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    return exitCode;
}

void ansClean(LibsData *ld){
    vkDeviceWaitIdle(ld->dev);

    for (uint32_t i = 0; i < ld->scImgc; i++)
        vkDestroyFramebuffer(ld->dev, ld->FrameBuffer[i], NULL);
    vkDestroyPipeline(ld->dev, ld->pipeline, NULL);
    vkDestroyPipelineLayout(ld->dev, ld->pipelineLayout, NULL);
    vkDestroyRenderPass(ld->dev, ld->RenderPass, NULL);

    for (uint32_t i = 0; i < ld->scImgc; i++)
        vkDestroyImageView(ld->dev, ld->imgViews[i], NULL);
    vkDestroySwapchainKHR(ld->dev, ld->swapchain, NULL);

    free(ld->FrameBuffer); free(ld->imgViews);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(ld->dev, ld->renderFinished[i], NULL);
        vkDestroySemaphore(ld->dev, ld->imgAvailible[i], NULL);
        vkDestroyFence(ld->dev, ld->fences[i], NULL);
    }

    vkDestroyCommandPool(ld->dev, ld->CommandPool, NULL);
    vkDestroyDevice(ld->dev, NULL);
    vkDestroySurfaceKHR(ld->instance, ld->surface, NULL);
    vkDestroyInstance(ld->instance, NULL);
    glfwDestroyWindow(ld->win);
    glfwTerminate();
}

AnsConfig ansGetConfig(char* filename){
    AnsConfig config = {0};
    FILE *fp = fopen(filename, "r");
    if (!fp) return config; // width = 0 -> error

    char str[CFG_MAX_STR_LEN] = {0};
    while (fgets(str, CFG_MAX_STR_LEN, fp)){
        if (str[0] == '#') continue;
        if (str[6] == 'W') // recognise strings by key symbols
            config.WinWidth = atoi(str + 14);
        else if (str[6] == 'H')
            config.WinHeight = atoi(str + 15);
        else if (str[1] == 'u'){
            if (str[18] == '0') config.VkVersion = VK_API_VERSION_1_0;
            else if (str[18] == '1') config.VkVersion = VK_API_VERSION_1_1;
            else if (str[18] == '2') config.VkVersion = VK_API_VERSION_1_2;
            else if (str[18] == '3') config.VkVersion = VK_API_VERSION_1_3;
            else config.VkVersion = VK_API_VERSION_1_4;
        }
        else if (str[0] == 'G')
            config.GpuId = atoi(str + 8);
        else {
            if (str[12] == 'f') config.Vsync = VK_PRESENT_MODE_FIFO_KHR;
            else if (str[12] == 'm') config.Vsync = VK_PRESENT_MODE_MAILBOX_KHR;
            else config.Vsync = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    if (!config.WinHeight || !config.VkVersion)
        config.WinWidth = 0;
    fclose(fp);
    return config;
}

int main(void){
    VkResult result; // temp
    LibsData *ld = malloc(sizeof(LibsData));
    AnsConfig ansCfg = ansGetConfig("config.cfg");
    
    //printf("%u\n", ld->countUniqueQueues);
    if (!ansCfg.WinWidth){
        fputs("[Error] Invalid or mising config file\n", stderr);
        return 6;
    }

    int exitCode = AnsMkWinInstSurf(ld, &ansCfg);
    if (exitCode) return exitCode;

    exitCode = AnsDevInit(ld, ansCfg.GpuId);
    if (exitCode) return exitCode;

    exitCode = ansInitSurface(ld);
    if (exitCode) return exitCode;
    
    exitCode = ansMkSwapChain(ld, ansCfg.Vsync);
    if (exitCode) return exitCode;

    exitCode = ansMkRenderPassPipeline(ld);
    if (exitCode) return exitCode;

    exitCode = ansMkFrameBuffers(ld);
    if (exitCode) return exitCode;

    exitCode = ansMkCommand(ld);
    if (exitCode) return exitCode;

    puts("[Info] Init completed");

    exitCode = ansRunCycle(ld);

    ansClean(ld);
    return exitCode;
}
