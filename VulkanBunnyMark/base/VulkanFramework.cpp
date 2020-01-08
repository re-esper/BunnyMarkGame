/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanFramework.h"

std::vector<const char*> VulkanFramework::args;

VkResult VulkanFramework::createInstance(bool enableValidation)
{
    settings.validation = enableValidation;

    // Validation can also be forced via a define
#if defined(_VALIDATION)
    settings.validation = true;
#endif

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = name.c_str();
    appInfo.pEngineName = name.c_str();
    appInfo.apiVersion = apiVersion;

    std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Enable surface extensions depending on os
#if defined(_WIN32)
    instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    instanceExtensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    if (enabledInstanceExtensions.size() > 0) {
        for (auto enabledExtension : enabledInstanceExtensions) {
            instanceExtensions.push_back(enabledExtension);
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &appInfo;
    if (instanceExtensions.size() > 0) {
        if (settings.validation) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }
    if (settings.validation) {
        // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
        // Note that on Android this layer requires at least NDK r20
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        } else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

std::string VulkanFramework::getWindowTitle()
{
    std::string device(deviceProperties.deviceName);
    std::string windowTitle;
    windowTitle = title + " - " + device;
    if (!settings.overlay) {
        windowTitle += " - " + std::to_string(frameCounter) + " fps";
    }
    return windowTitle;
}

#if !(defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
// iOS & macOS: VulkanFramework::getAssetPath() implemented externally to allow access to Objective-C components
const std::string VulkanFramework::getAssetPath()
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    return "";
#else
    return "./../data/";
#endif
}
#endif

void VulkanFramework::createCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    drawCmdBuffers.resize(swapChain.imageCount);
    drawCmdBuffersValid.resize(swapChain.imageCount, false);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(
        cmdPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(drawCmdBuffers.size())
    );

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanFramework::destroyCommandBuffers()
{
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanFramework::invalidateCommandBuffers()
{
    std::fill(drawCmdBuffersValid.begin(), drawCmdBuffersValid.end(), false);
}

VkCommandBuffer VulkanFramework::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, level, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start the new command buffer
    if (begin) {
        VkCommandBufferBeginInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
}

void VulkanFramework::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    if (free) {
        vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
    }
}

void VulkanFramework::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanFramework::prepare()
{
    initSwapchain();
    createCommandPool();
    setupSwapChain();
    createCommandBuffers();
    createSynchronizationPrimitives();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFrameBuffer();

    if (settings.overlay) {
        UIOverlay.device = vulkanDevice;
        UIOverlay.queue = queue;
        UIOverlay.shaders = {
            loadShader(getAssetPath() + "shaders/base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            loadShader(getAssetPath() + "shaders/base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
        };
        UIOverlay.prepareResources();
        UIOverlay.preparePipeline(pipelineCache, renderPass);
    }
}

VkPipelineShaderStageCreateInfo VulkanFramework::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device);
#else
    shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
#endif
    shaderStage.pName = "main"; // todo : make param
    assert(shaderStage.module != VK_NULL_HANDLE);
    shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void VulkanFramework::renderFrame()
{
    auto tStart = std::chrono::high_resolution_clock::now();
    if (viewUpdated) {
        viewUpdated = false;
        viewChanged();
    }

    render();
    frameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    frameDeltaTime = (float)tDiff / 1000.0f;
    camera.update(frameDeltaTime);
    if (camera.moving()) {
        viewUpdated = true;
    }

    // Convert to clamped timer value
    if (!paused) {
        timer += timerSpeed * frameDeltaTime;
        if (timer > 1.0) timer -= 1.0f;
    }
    float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
    if (fpsTimer > 1000.0f) {
        lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
#if defined(_WIN32)
        if (!settings.overlay) {
            std::string windowTitle = getWindowTitle();
            SetWindowText(window, windowTitle.c_str());
        }
#endif
        frameCounter = 0;
        lastTimestamp = tEnd;
    }
    // TODO: Cap UI overlay update rates
    updateOverlay();
}

void VulkanFramework::renderLoop()
{
    destWidth = width;
    destHeight = height;
    lastTimestamp = std::chrono::high_resolution_clock::now();
#if defined(_WIN32)
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            renderFrame();
        }
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;
        bool destroy = false;

        focused = true;

        while ((ident = ALooper_pollAll(focused ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) {
                source->process(androidApp, source);
            }
            if (androidApp->destroyRequested != 0) {
                LOGD("Android app destroy requested");
                destroy = true;
                break;
            }
        }

        // App destruction requested
        // Exit loop, example will be destroyed in application main
        if (destroy) {
            ANativeActivity_finish(androidApp->activity);
            break;
        }

        // Render frame
        if (prepared) {
            auto tStart = std::chrono::high_resolution_clock::now();
            render();
            frameCounter++;
            auto tEnd = std::chrono::high_resolution_clock::now();
            auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
            frameDeltaTime = tDiff / 1000.0f;
            camera.update(frameDeltaTime);
            // Convert to clamped timer value
            if (!paused) {
                timer += timerSpeed * frameDeltaTime;
                if (timer > 1.0) {
                    timer -= 1.0f;
                }
            }
            float fpsTimer = std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count();
            if (fpsTimer > 1000.0f) {
                lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
                frameCounter = 0;
                lastTimestamp = tEnd;
            }

            // TODO: Cap UI overlay update rates/only issue when update requested
            updateOverlay();

            bool updateView = false;

            // Check touch state (for movement)
            if (touchDown) {
                touchTimer += frameDeltaTime;
            }
            if (touchTimer >= 1.0) {
                camera.keys.up = true;
                viewChanged();
            }

            // Check gamepad state
            const float deadZone = 0.0015f;
            // todo : check if gamepad is present
            // todo : time based and relative axis positions
            if (camera.type != Camera::CameraType::firstperson) {
                // Rotate
                if (std::abs(gamePadState.axisLeft.x) > deadZone) {
                    rotation.y += gamePadState.axisLeft.x * 0.5f * rotationSpeed;
                    camera.rotate(glm::vec3(0.0f, gamePadState.axisLeft.x * 0.5f, 0.0f));
                    updateView = true;
                }
                if (std::abs(gamePadState.axisLeft.y) > deadZone) {
                    rotation.x -= gamePadState.axisLeft.y * 0.5f * rotationSpeed;
                    camera.rotate(glm::vec3(gamePadState.axisLeft.y * 0.5f, 0.0f, 0.0f));
                    updateView = true;
                }
                // Zoom
                if (std::abs(gamePadState.axisRight.y) > deadZone) {
                    zoom -= gamePadState.axisRight.y * 0.01f * zoomSpeed;
                    updateView = true;
                }
                if (updateView) {
                    viewChanged();
                }
            } else {
                updateView = camera.updatePad(gamePadState.axisLeft, gamePadState.axisRight, frameDeltaTime);
                if (updateView) {
                    viewChanged();
                }
            }
        }
    }
#endif
    // Flush device to make sure all resources can be freed
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }
}

void VulkanFramework::updateOverlay()
{
    if (!settings.overlay)
        return;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)width, (float)height);
    io.DeltaTime = frameDeltaTime;

    io.MousePos = ImVec2(mousePos.x, mousePos.y);
    io.MouseDown[0] = mouseButtons.left;
    io.MouseDown[1] = mouseButtons.right;

    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
    ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(deviceProperties.deviceName);
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f * UIOverlay.scale));
#endif
    ImGui::PushItemWidth(110.0f * UIOverlay.scale);
    onUpdateUIOverlay(&UIOverlay);
    ImGui::PopItemWidth();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ImGui::PopStyleVar();
#endif

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();

    if (UIOverlay.update() || UIOverlay.updated) {
        invalidateCommandBuffers();
        UIOverlay.updated = false;
    }

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (mouseButtons.left) {
        mouseButtons.left = false;
    }
#endif
}

void VulkanFramework::drawUI(const VkCommandBuffer commandBuffer)
{
    if (settings.overlay) {
        const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        UIOverlay.draw(commandBuffer);
    }
}

void VulkanFramework::prepareFrame()
{
    // Acquire the next image from the swap chain
    VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
    // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
        windowResize();
    } else {
        VK_CHECK(result);
    }
}

void VulkanFramework::submitFrame()
{
    // Present the current buffer to the swap chain
    // Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
    // This ensures that the image is not presented to the windowing system until all commands have been submitted
    VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Swap chain is no longer compatible with the surface and needs to be recreated
            windowResize();
            return;
        } else {
            VK_CHECK(result);
        }
    }    
}

VulkanFramework::VulkanFramework(bool enableValidation)
{
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
    // Check for a valid asset path
    struct stat info;
    if (stat(getAssetPath().c_str(), &info) != 0) {
#if defined(_WIN32)
        std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
        MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
#else
        std::cerr << "Error: Could not find asset path in " << getAssetPath() << std::endl;
#endif
        exit(-1);
    }
#endif

    settings.validation = enableValidation;

    char* numConvPtr;

    // Parse command line arguments
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == std::string("-validation")) {
            settings.validation = true;
        }
        if (args[i] == std::string("-vsync")) {
            settings.vsync = true;
        }
        if ((args[i] == std::string("-f")) || (args[i] == std::string("--fullscreen"))) {
            settings.fullscreen = true;
        }
        if ((args[i] == std::string("-w")) || (args[i] == std::string("-width"))) {
            uint32_t w = strtol(args[i + 1], &numConvPtr, 10);
            if (numConvPtr != args[i + 1]) {
                width = w;
            };
        }
        if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
            uint32_t h = strtol(args[i + 1], &numConvPtr, 10);
            if (numConvPtr != args[i + 1]) {
                height = h;
            };
        }
    }

    VK_CHECK(volkInitialize());

#if defined(_WIN32)
    // Enable console if validation is active
    // Debug message callback will output to it
    if (this->settings.validation) {
        setupConsole("Vulkan validation output");
    }
    setupDPIAwareness();
#endif
}

VulkanFramework::~VulkanFramework()
{
    // Clean up Vulkan resources
    swapChain.cleanup();
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    destroyCommandBuffers();
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
    }

    for (auto& shaderModule : shaderModules) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);

    vkDestroyPipelineCache(device, pipelineCache, nullptr);

    vkDestroyCommandPool(device, cmdPool, nullptr);

    vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
    vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
    for (auto& fence : waitFences) {
        vkDestroyFence(device, fence, nullptr);
    }

    if (settings.overlay) {
        UIOverlay.freeResources();
    }

    delete vulkanDevice;

    if (settings.validation) {
        vks::debug::freeDebugCallback(instance);
    }

    vkDestroyInstance(instance, nullptr);

    // todo : android cleanup (if required)
}

bool VulkanFramework::initVulkan()
{
    VkResult err;

    // Vulkan instance
    err = createInstance(settings.validation);
    if (err) {
        vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), err);
        return false;
    }

    volkLoadInstance(instance);

    // If requested, we enable the default validation layers for debugging
    if (settings.validation) {
        // The report flags determine what type of messages for the layers will be displayed
        // For validating (debugging) an appplication the error and warning bits should suffice
        VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        // Additional flags include performance info, loader and layer debug messages, etc.
        vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
    }

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    assert(gpuCount > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
    if (err) {
        vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), err);
        return false;
    }

    // GPU selection

    // Select physical device to be used for the Vulkan example
    // Defaults to the first device unless specified by command line
    uint32_t selectedDevice = 0;

#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
    // GPU selection via command line argument
    for (size_t i = 0; i < args.size(); i++) {
        // Select GPU
        if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu"))) {
            char* endptr;
            uint32_t index = strtol(args[i + 1], &endptr, 10);
            if (endptr != args[i + 1]) {
                if (index > gpuCount - 1) {
                    std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << std::endl;
                } else {
                    std::cout << "Selected Vulkan device " << index << std::endl;
                    selectedDevice = index;
                }
            };
            break;
        }
        // List available GPUs
        if (args[i] == std::string("-listgpus")) {
            uint32_t gpuCount = 0;
            VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
            if (gpuCount == 0) {
                std::cerr << "No Vulkan devices found!" << std::endl;
            } else {
                // Enumerate devices
                std::cout << "Available Vulkan devices" << std::endl;
                std::vector<VkPhysicalDevice> devices(gpuCount);
                VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data()));
                for (uint32_t i = 0; i < gpuCount; i++) {
                    VkPhysicalDeviceProperties deviceProperties;
                    vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
                    std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
                    std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << std::endl;
                    std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << std::endl;
                }
            }
        }
    }
#endif

    physicalDevice = physicalDevices[selectedDevice];

    // Store properties (including limits), features and memory properties of the phyiscal device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
    getEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    vulkanDevice = new vks::VulkanDevice(physicalDevice);
    VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    if (res != VK_SUCCESS) {
        vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), res);
        return false;
    }
    device = vulkanDevice->device;
    if (vulkanDevice->enableDebugMarkers) {
        vks::debugmarker::setup(device);
    }

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

    // Find a suitable depth format
    VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
    assert(validDepthFormat);

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    // Ensures that the image is displayed before we start submitting new commands to the queue
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

    // Set up submit info structure
    // Semaphores will stay the same during application lifetime
    // Command buffer submission info is set by each example
    VkSubmitInfo sinfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    sinfo.pWaitDstStageMask = &submitPipelineStages;
    sinfo.waitSemaphoreCount = 1;
    sinfo.pWaitSemaphores = &semaphores.presentComplete;
    sinfo.signalSemaphoreCount = 1;
    sinfo.pSignalSemaphores = &semaphores.renderComplete;
    submitInfo = sinfo;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    // Get Android device name and manufacturer (to display along GPU name)
    androidProduct = "";
    char prop[PROP_VALUE_MAX + 1];
    int len = __system_property_get("ro.product.manufacturer", prop);
    if (len > 0) {
        androidProduct += std::string(prop) + " ";
    };
    len = __system_property_get("ro.product.model", prop);
    if (len > 0) {
        androidProduct += std::string(prop);
    };
    LOGD("androidProduct = %s", androidProduct.c_str());
#endif

    return true;
}

#if defined(_WIN32)
// Win32 : Sets up a console window and redirects standard output to it
void VulkanFramework::setupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanFramework::setupDPIAwareness()
{
    typedef HRESULT*(__stdcall * SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore) {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if (setProcessDpiAwareness != nullptr) {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VulkanFramework::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
    windowInstance = hinstance;

    WNDCLASSEX wndClass;
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndproc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hinstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = name.c_str();
    wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

    if (!RegisterClassEx(&wndClass)) {
        std::cout << "Could not register window class!\n";
        fflush(stdout);
        exit(1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    if (settings.fullscreen) {
        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
        dmScreenSettings.dmSize = sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth = screenWidth;
        dmScreenSettings.dmPelsHeight = screenHeight;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if ((width != (uint32_t)screenWidth) && (height != (uint32_t)screenHeight)) {
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
                if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES) {
                    settings.fullscreen = false;
                } else {
                    return nullptr;
                }
            }
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;
    if (settings.fullscreen) {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    } else {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect;
    windowRect.left = 0L;
    windowRect.top = 0L;
    windowRect.right = settings.fullscreen ? (long)screenWidth : (long)width;
    windowRect.bottom = settings.fullscreen ? (long)screenHeight : (long)height;

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    window = CreateWindowEx(0,
        name.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hinstance,
        NULL);

    if (!settings.fullscreen) {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    if (!window) {
        printf("Could not create window!\n");
        fflush(stdout);
        return nullptr;
        exit(1);
    }

    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(window);

    return window;
}

void VulkanFramework::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CLOSE:
        prepared = false;
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        ValidateRect(window, NULL);
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case KEY_P:
            paused = !paused;
            break;
        case KEY_F1:
            if (settings.overlay) {
                UIOverlay.visible = !UIOverlay.visible;
            }
            break;
        case KEY_ESCAPE:
            PostQuitMessage(0);
            break;
        }
        if (camera.firstperson) {
            switch (wParam) {
            case KEY_W:
                camera.keys.up = true;
                break;
            case KEY_S:
                camera.keys.down = true;
                break;
            case KEY_A:
                camera.keys.left = true;
                break;
            case KEY_D:
                camera.keys.right = true;
                break;
            }
        }
        keyPressed((uint32_t)wParam);
        break;
    case WM_KEYUP:
        if (camera.firstperson) {
            switch (wParam) {
            case KEY_W:
                camera.keys.up = false;
                break;
            case KEY_S:
                camera.keys.down = false;
                break;
            case KEY_A:
                camera.keys.left = false;
                break;
            case KEY_D:
                camera.keys.right = false;
                break;
            }
        }
        break;
    case WM_LBUTTONDOWN:
        mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseButtons.left = true;
        break;
    case WM_RBUTTONDOWN:
        mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseButtons.right = true;
        break;
    case WM_MBUTTONDOWN:
        mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseButtons.middle = true;
        break;
    case WM_LBUTTONUP:
        mouseButtons.left = false;
        break;
    case WM_RBUTTONUP:
        mouseButtons.right = false;
        break;
    case WM_MBUTTONUP:
        mouseButtons.middle = false;
        break;
    case WM_MOUSEWHEEL: {
        short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        zoom += (float)wheelDelta * 0.005f * zoomSpeed;
        camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f * zoomSpeed));
        viewUpdated = true;
        break;
    }
    case WM_MOUSEMOVE: {
        handleMouseMove(LOWORD(lParam), HIWORD(lParam));
        break;
    }
    case WM_SIZE:
        if ((prepared) && (wParam != SIZE_MINIMIZED)) {
            if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED))) {
                destWidth = LOWORD(lParam);
                destHeight = HIWORD(lParam);
                windowResize();
            }
        }
        break;
    case WM_ENTERSIZEMOVE:
        resizing = true;
        break;
    case WM_EXITSIZEMOVE:
        resizing = false;
        break;
    }
}
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
int32_t VulkanFramework::handleAppInput(struct android_app* app, AInputEvent* event)
{
    VulkanFramework* vulkanApp = reinterpret_cast<VulkanFramework*>(app->userData);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t eventSource = AInputEvent_getSource(event);
        switch (eventSource) {
        case AINPUT_SOURCE_JOYSTICK: {
            // Left thumbstick
            vulkanApp->gamePadState.axisLeft.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
            vulkanApp->gamePadState.axisLeft.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
            // Right thumbstick
            vulkanApp->gamePadState.axisRight.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0);
            vulkanApp->gamePadState.axisRight.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0);
            break;
        }

        case AINPUT_SOURCE_TOUCHSCREEN: {
            int32_t action = AMotionEvent_getAction(event);

            switch (action) {
            case AMOTION_EVENT_ACTION_UP: {
                vulkanApp->lastTapTime = AMotionEvent_getEventTime(event);
                vulkanApp->touchPos.x = AMotionEvent_getX(event, 0);
                vulkanApp->touchPos.y = AMotionEvent_getY(event, 0);
                vulkanApp->touchTimer = 0.0;
                vulkanApp->touchDown = false;
                vulkanApp->camera.keys.up = false;

                // Detect single tap
                int64_t eventTime = AMotionEvent_getEventTime(event);
                int64_t downTime = AMotionEvent_getDownTime(event);
                if (eventTime - downTime <= vks::android::TAP_TIMEOUT) {
                    float deadZone = (160.f / vks::android::screenDensity) * vks::android::TAP_SLOP * vks::android::TAP_SLOP;
                    float x = AMotionEvent_getX(event, 0) - vulkanApp->touchPos.x;
                    float y = AMotionEvent_getY(event, 0) - vulkanApp->touchPos.y;
                    if ((x * x + y * y) < deadZone) {
                        vulkanApp->mouseButtons.left = true;
                    }
                };
                return 1;
                break;
            }
            case AMOTION_EVENT_ACTION_DOWN: {
                // Detect double tap
                int64_t eventTime = AMotionEvent_getEventTime(event);
                if (eventTime - vulkanApp->lastTapTime <= vks::android::DOUBLE_TAP_TIMEOUT) {
                    float deadZone = (160.f / vks::android::screenDensity) * vks::android::DOUBLE_TAP_SLOP * vks::android::DOUBLE_TAP_SLOP;
                    float x = AMotionEvent_getX(event, 0) - vulkanApp->touchPos.x;
                    float y = AMotionEvent_getY(event, 0) - vulkanApp->touchPos.y;
                    if ((x * x + y * y) < deadZone) {
                        vulkanApp->keyPressed(TOUCH_DOUBLE_TAP);
                        vulkanApp->touchDown = false;
                    }
                } else {
                    vulkanApp->touchDown = true;
                }
                vulkanApp->touchPos.x = AMotionEvent_getX(event, 0);
                vulkanApp->touchPos.y = AMotionEvent_getY(event, 0);
                vulkanApp->mousePos.x = AMotionEvent_getX(event, 0);
                vulkanApp->mousePos.y = AMotionEvent_getY(event, 0);
                break;
            }
            case AMOTION_EVENT_ACTION_MOVE: {
                bool handled = false;
                if (vulkanApp->settings.overlay) {
                    ImGuiIO& io = ImGui::GetIO();
                    handled = io.WantCaptureMouse;
                }
                if (!handled) {
                    int32_t eventX = AMotionEvent_getX(event, 0);
                    int32_t eventY = AMotionEvent_getY(event, 0);

                    float deltaX = (float)(vulkanApp->touchPos.y - eventY) * vulkanApp->rotationSpeed * 0.5f;
                    float deltaY = (float)(vulkanApp->touchPos.x - eventX) * vulkanApp->rotationSpeed * 0.5f;

                    vulkanApp->camera.rotate(glm::vec3(deltaX, 0.0f, 0.0f));
                    vulkanApp->camera.rotate(glm::vec3(0.0f, -deltaY, 0.0f));

                    vulkanApp->rotation.x += deltaX;
                    vulkanApp->rotation.y -= deltaY;

                    vulkanApp->viewChanged();

                    vulkanApp->touchPos.x = eventX;
                    vulkanApp->touchPos.y = eventY;
                }
                break;
            }
            default:
                return 1;
                break;
            }
        }

            return 1;
        }
    }

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode((const AInputEvent*)event);
        int32_t action = AKeyEvent_getAction((const AInputEvent*)event);
        int32_t button = 0;

        if (action == AKEY_EVENT_ACTION_UP)
            return 0;

        switch (keyCode) {
        case AKEYCODE_BUTTON_A:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_A);
            break;
        case AKEYCODE_BUTTON_B:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_B);
            break;
        case AKEYCODE_BUTTON_X:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_X);
            break;
        case AKEYCODE_BUTTON_Y:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_Y);
            break;
        case AKEYCODE_BUTTON_L1:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_L1);
            break;
        case AKEYCODE_BUTTON_R1:
            vulkanApp->keyPressed(GAMEPAD_BUTTON_R1);
            break;
        case AKEYCODE_BUTTON_START:
            vulkanApp->paused = !vulkanApp->paused;
            break;
        };

        LOGD("Button %d pressed", keyCode);
    }

    return 0;
}

void VulkanFramework::handleAppCommand(android_app* app, int32_t cmd)
{
    assert(app->userData != NULL);
    VulkanFramework* vulkanApp = reinterpret_cast<VulkanFramework*>(app->userData);
    switch (cmd) {
    case APP_CMD_SAVE_STATE:
        LOGD("APP_CMD_SAVE_STATE");
        /*
		vulkanApp->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)vulkanApp->app->savedState) = vulkanApp->state;
		vulkanApp->app->savedStateSize = sizeof(struct saved_state);
		*/
        break;
    case APP_CMD_INIT_WINDOW:
        LOGD("APP_CMD_INIT_WINDOW");
        if (androidApp->window != NULL) {
            if (vulkanApp->initVulkan()) {
                vulkanApp->prepare();
                assert(vulkanApp->prepared);
            } else {
                LOGE("Could not initialize Vulkan, exiting!");
                androidApp->destroyRequested = 1;
            }
        } else {
            LOGE("No window assigned!");
        }
        break;
    case APP_CMD_LOST_FOCUS:
        LOGD("APP_CMD_LOST_FOCUS");
        vulkanApp->focused = false;
        break;
    case APP_CMD_GAINED_FOCUS:
        LOGD("APP_CMD_GAINED_FOCUS");
        vulkanApp->focused = true;
        break;
    case APP_CMD_TERM_WINDOW:
        // Window is hidden or closed, clean up resources
        LOGD("APP_CMD_TERM_WINDOW");
        if (vulkanApp->prepared) {
            vulkanApp->swapChain.cleanup();
        }
        break;
    }
}
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
void* VulkanFramework::setupWindow(void* view)
{
    this->view = view;
    return view;
}
#endif

void VulkanFramework::createSynchronizationPrimitives()
{
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    waitFences.resize(drawCmdBuffers.size());
    for (auto& fence : waitFences) {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
    }
}

void VulkanFramework::createCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void VulkanFramework::setupDepthStencil()
{
    VkImageCreateInfo imageCI = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };    
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = depthFormat;
    imageCI.extent = { width, height, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VK_CHECK(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

    VkMemoryAllocateInfo memAllloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAllloc.allocationSize = memReqs.size;
    memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
    VK_CHECK(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

    VkImageViewCreateInfo imageViewCI = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.image = depthStencil.image;
    imageViewCI.format = depthFormat;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
    if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
        imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    VK_CHECK(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
}

void VulkanFramework::setupFrameBuffer()
{
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;

    VkFramebufferCreateInfo frameBufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    frameBufferCreateInfo.renderPass = renderPass;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = width;
    frameBufferCreateInfo.height = height;
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapChain.imageCount);
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        attachments[0] = swapChain.buffers[i].view;
        VK_CHECK(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanFramework::setupRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanFramework::getEnabledFeatures()
{
    // Can be overriden in derived class
}

void VulkanFramework::windowResize()
{
    if (!prepared) return;

    prepared = false;

    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(device);

    // Recreate swap chain
    width = destWidth;
    height = destHeight;
    setupSwapChain();

    // Recreate the frame buffers
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);
    setupDepthStencil();
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
    }
    setupFrameBuffer();

    if ((width > 0.0f) && (height > 0.0f)) {
        if (settings.overlay) {
            UIOverlay.resize(width, height);
        }
    }

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroyCommandBuffers();
    createCommandBuffers();
    invalidateCommandBuffers();

    vkDeviceWaitIdle(device);

    if ((width > 0.0f) && (height > 0.0f)) {
        camera.updateAspectRatio((float)width / (float)height);
    }

    // Notify derived class
    windowResized();
    viewChanged();

    prepared = true;
}

void VulkanFramework::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mousePos.x - x;
    int32_t dy = (int32_t)mousePos.y - y;

    bool handled = false;

    if (settings.overlay) {
        ImGuiIO& io = ImGui::GetIO();
        handled = io.WantCaptureMouse;
    }
    mouseMoved((float)x, (float)y, handled);

    if (handled) {
        mousePos = glm::vec2((float)x, (float)y);
        return;
    }

    if (mouseButtons.left) {
        rotation.x += dy * 1.25f * rotationSpeed;
        rotation.y -= dx * 1.25f * rotationSpeed;
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
        viewUpdated = true;
    }
    if (mouseButtons.right) {
        zoom += dy * .005f * zoomSpeed;
        camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
        viewUpdated = true;
    }
    if (mouseButtons.middle) {
        cameraPos.x -= dx * 0.01f;
        cameraPos.y -= dy * 0.01f;
        camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
        viewUpdated = true;
    }
    mousePos = glm::vec2((float)x, (float)y);
}

void VulkanFramework::initSwapchain()
{
#if defined(_WIN32)
    swapChain.initialize(windowInstance, window, instance, physicalDevice, device);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    swapChain.initialize(androidApp->window, instance, physicalDevice, device);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
    swapChain.initialize(view, instance, physicalDevice, device);
#endif
}

void VulkanFramework::setupSwapChain()
{
    swapChain.create(&width, &height, settings.vsync);
}