#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulkanFramework.h"

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

#define ENABLE_VALIDATION false

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1

const float bunniesAddingThreshold = 0.1f; // in seconds
const float gravity = 0.5f;
const uint32_t bunniesEachTime = 5000;

inline float rrand() { return (float)rand() / (float)RAND_MAX; }
inline float rrand(float a, float b) {
    float r = (float)rand() / (float)RAND_MAX;
    return a + r * (b - a);
}

struct VertexData {
    glm::vec4 inPositionTexcoord;
};

struct SpriteData {
    glm::mat2 inSpriteScaleRotation;
    glm::vec2 inSpritePosition;
};

struct Sprite {
    float x, y;
    float scale;
    float rotation;
    SpriteData* renderData = nullptr;
    // bunny.speedX, bunny.speedY
    float speedX, speedY;

    void setRenderData(SpriteData* data) {
        if (data) {
            renderData = data;
            renderData->inSpritePosition = glm::vec2(x, y);
            updateScaleRotation();
        }
    }
    void setPosition(float px, float py) {
        x = px; y = py;
        updatePosition();
    }
    void setScale(float s) {
        if (scale != s) {
            scale = s;
            updateScaleRotation();
        }
    }
    void setRotation(float r) {
        if (rotation != r) {
            rotation = r;
            updateScaleRotation();
        }
    }
    inline void updatePosition() {
        if (renderData) {
            renderData->inSpritePosition.x = x;
            renderData->inSpritePosition.y = y;
        }
    }
    inline void updateScaleRotation() {
        if (renderData) {
            glm::mat2 scalemat = glm::mat2(scale, 0.f, 0.f, scale);
            float sinr = std::sinf(rotation);
            float cosr = std::cosf(rotation);
            glm::mat2 rotmat = glm::mat2(cosr, -sinr, sinr, cosr);
            renderData->inSpriteScaleRotation = scalemat * rotmat;
        }
    }
};

struct SpriteBatch {
    uint32_t texId;

    std::vector<SpriteData> spriteDatas;
    std::vector<Sprite> sprites;
    vks::Buffer instanceBuffer;

    SpriteBatch(uint32_t type) : texId(type) {}

    void initSprites(vks::VulkanDevice* vdevice, const std::vector<Sprite>& sprs) {
        spriteDatas.resize(sprs.size());
        sprites.insert(sprites.end(), sprs.begin(), sprs.end());
        for (int i = 0; i < sprites.size(); ++i) {
            sprites[i].setRenderData(&spriteDatas[i]);
        }
        instanceBuffer.create(vdevice, vks::BufferType::transient, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, spriteDatas.size() * sizeof(SpriteData), true);
    }
    inline size_t size() { return sprites.size(); }
    void flush() {
        memcpy(instanceBuffer.mappedData, spriteDatas.data(), spriteDatas.size() * sizeof(SpriteData));
    }
};

class VulkanDemo : public VulkanFramework {
public:
    vks::Texture2D texture;

    struct {
        VkPipelineVertexInputStateCreateInfo inputState;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    vks::Buffer vertexBuffer;
    vks::Buffer indexBuffer;
    vks::Buffer instanceBuffer;
    vks::Buffer uniformBuffer;

    struct {
        glm::mat4 projection;
    } uboVS;

    VkPipeline spritePipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    VulkanDemo()
        : VulkanFramework(ENABLE_VALIDATION)
    {
        title = "Bunny Mark";
        settings.overlay = true;
    }

    ~VulkanDemo()
    {
        vkDestroyPipeline(device, spritePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        for (auto& batch : spriteBatches) {
            batch.instanceBuffer.destroy();
        }

        texture.destroy();
        vertexBuffer.destroy();
        indexBuffer.destroy();
        instanceBuffer.destroy();
        uniformBuffer.destroy();
    }

    void initBunny(Sprite& bunny) {
        bunny.x = bunny.y = 0.f;
        bunny.speedX = rrand() * 10;
        bunny.speedY = rrand() * 10 - 5;
        bunny.scale = rrand(0.5, 1.0);
        bunny.rotation = rrand() - 0.5;
    }

    uint32_t bunnyCount = 0;
    std::vector<SpriteBatch> spriteBatches;
    uint32_t currentTexId = 0;
    void addBunnies(int32_t amount) {
        spriteBatches.emplace_back(currentTexId);
        SpriteBatch& batch = spriteBatches.back();
        std::vector<Sprite> sprs;
        sprs.resize(amount);
        for (int i = 0; i < amount; ++i) {
            Sprite& bunny = sprs[i];
            bunny.x = bunny.y = 0.f;
            bunny.speedX = rrand() * 10;
            bunny.speedY = rrand() * 10 - 5;
            bunny.scale = rrand(0.5, 1.0);
            bunny.rotation = rrand() - 0.5;
        }
        batch.initSprites(vulkanDevice, sprs);
        bunnyCount += amount;
        invalidateCommandBuffers();
    }

    float clickDownTime = -1.f;
    inline bool isClickDown() {
#if defined(_WIN32)
        return mouseButtons.left;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        return touchDown;
#endif
    }
    void update(float deltaTime)
    {
        if (isClickDown()) {
            if (clickDownTime < 0) {
                clickDownTime = deltaTime;
                addBunnies(bunniesEachTime);
            }
            else {
                clickDownTime += deltaTime;
                if (clickDownTime > bunniesAddingThreshold) {
                    // add bunnies!
                    addBunnies(bunniesEachTime);
                    clickDownTime -= bunniesAddingThreshold;
                }
            }
        }
        else if (clickDownTime > 0) {
            clickDownTime = -1.0f;
            currentTexId++;
            currentTexId %= 5;
        }

        // update bunnies!
        float maxX = width;
        //float minX = 0;
        float maxY = height;
        //float minY = 0;
        float d = 60.f * deltaTime; // pixijs's bunnymark work at 60 fps
        float gravityd = gravity * d;
        for (auto& batch : spriteBatches) {
            for (auto& bunny : batch.sprites) {
                bunny.x += bunny.speedX * d;
                bunny.y += bunny.speedY * d;
                bunny.speedY += gravityd;

                if (bunny.x > maxX) {
                    bunny.speedX *= -1;
                    bunny.x = maxX;
                }
                else if (bunny.x < 0) {
                    bunny.speedX *= -1;
                    bunny.x = 0;
                }
                if (bunny.y > maxY) {
                    bunny.speedY *= -0.85f;
                    bunny.y = maxY;
                    if (rand() & 1) { // if (Math.rrand() > 0.5)
                        bunny.speedY -= rrand() * 6;
                    }
                }
                else if (bunny.y < 0) {
                    bunny.speedY = 0;
                    bunny.y = 0;
                }
                bunny.updatePosition();
            }
            batch.flush();
        }
    }

    virtual void getEnabledFeatures()
    {
    }

    void buildCommandBuffer(VkCommandBuffer drawCmdBuffer, VkFramebuffer frameBuffer)
    {
        VkCommandBufferBeginInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VK_CHECK(vkBeginCommandBuffer(drawCmdBuffer, &cmdBufInfo));

        VkClearValue clearValues[2];
        clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.framebuffer = frameBuffer;
        vkCmdBeginRenderPass(drawCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
        vkCmdSetViewport(drawCmdBuffer, 0, 1, &viewport);

        VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
        vkCmdSetScissor(drawCmdBuffer, 0, 1, &scissor);

        vkCmdBindDescriptorSets(drawCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
        vkCmdBindPipeline(drawCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline);

        VkDeviceSize offsets[1];
        for (auto& spriteBatch : spriteBatches) {
            offsets[0] = sizeof(VertexData) * 4 * spriteBatch.texId;
            vkCmdBindVertexBuffers(drawCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &vertexBuffer.buffer, offsets);

            offsets[0] = 0;
            vkCmdBindVertexBuffers(drawCmdBuffer, INSTANCE_BUFFER_BIND_ID, 1, &spriteBatch.instanceBuffer.buffer, offsets);

            vkCmdBindIndexBuffer(drawCmdBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdDrawIndexed(drawCmdBuffer, 6, spriteBatch.size(), 0, 0, 0);
        }

        drawUI(drawCmdBuffer);

        vkCmdEndRenderPass(drawCmdBuffer);

        VK_CHECK(vkEndCommandBuffer(drawCmdBuffer));
    }

    void draw()
    {
        VulkanFramework::prepareFrame();

        VK_CHECK(vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &waitFences[currentBuffer]));

        // Build command buffer if needed
        if (!drawCmdBuffersValid[currentBuffer]) {
            buildCommandBuffer(drawCmdBuffers[currentBuffer], frameBuffers[currentBuffer]);
            drawCmdBuffersValid[currentBuffer] = true;
        }

        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
        submitInfo.commandBufferCount = 1;
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]));

        VulkanFramework::submitFrame();
    }


    std::vector<VertexData> generateQuadVertices(float tx, float ty, float tw, float th)
    {
        float texw = (float)texture.width, texh = (float)texture.height;
        float halfw = tw * 0.5f;
        float halfh = th * 0.5f;
        std::vector<VertexData> vertices = {
            { { halfw, halfh, (tw + tx) / texw, (th + ty) / texh } },
            { { -halfw, halfh, tx / texw, (th + ty) / texh } },
            { { -halfw, -halfh, tx / texw, ty / texh } },
            { { halfw, -halfh, (tw + tx) / texw, ty / texh } }
        };
        return vertices;
    }
    void generateQuad()
    {
        std::string filename = getAssetPath() + "textures/bunnys.png";
        texture.loadFromFile(filename, vulkanDevice, queue);
        /*
            bunny1 = new PIXI.Texture(wabbitTexture.baseTexture, new PIXI.math.Rectangle(2, 47, 26, 37));
            bunny2 = new PIXI.Texture(wabbitTexture.baseTexture, new PIXI.math.Rectangle(2, 86, 26, 37));
            bunny3 = new PIXI.Texture(wabbitTexture.baseTexture, new PIXI.math.Rectangle(2, 125, 26, 37));
            bunny4 = new PIXI.Texture(wabbitTexture.baseTexture, new PIXI.math.Rectangle(2, 164, 26, 37));
            bunny5 = new PIXI.Texture(wabbitTexture.baseTexture, new PIXI.math.Rectangle(2, 2, 26, 37));
        */
        std::vector<VertexData> vertices;
        std::vector<VertexData> quad;
        quad = generateQuadVertices(2, 47, 26, 37); vertices.insert(vertices.end(), quad.begin(), quad.end());
        quad = generateQuadVertices(2, 86, 26, 37); vertices.insert(vertices.end(), quad.begin(), quad.end());
        quad = generateQuadVertices(2, 125, 26, 37); vertices.insert(vertices.end(), quad.begin(), quad.end());
        quad = generateQuadVertices(2, 164, 26, 37); vertices.insert(vertices.end(), quad.begin(), quad.end());
        quad = generateQuadVertices(2, 2, 26, 37); vertices.insert(vertices.end(), quad.begin(), quad.end());
        vertexBuffer.create(vulkanDevice, vks::BufferType::device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.size() * sizeof(VertexData));
        vertexBuffer.uploadFromStaging(vertices.data(), vertices.size() * sizeof(VertexData), queue);

        std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
        indexBuffer.create(vulkanDevice, vks::BufferType::device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.size() * sizeof(uint16_t));
        indexBuffer.uploadFromStaging(indices.data(), indices.size() * sizeof(uint16_t), queue);
    }

    void setupVertexDescriptions()
    {
        // Binding description
        vertices.bindingDescriptions = {
            vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(VertexData), VK_VERTEX_INPUT_RATE_VERTEX),
            vks::initializers::vertexInputBindingDescription(INSTANCE_BUFFER_BIND_ID, sizeof(SpriteData), VK_VERTEX_INPUT_RATE_INSTANCE)
        };
        vertices.attributeDescriptions = {
            vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0),
            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0),
            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteData, inSpritePosition)),
        };

        vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
        vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
        vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
        vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
    }

    void setupDescriptorPool()
    {
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(
            static_cast<uint32_t>(poolSizes.size()),
            poolSizes.data(),
            2);
        VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
    }

    void setupDescriptorSetLayout()
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::descriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT,
                0),
            // Binding 1 : sampler
            vks::initializers::descriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                1)
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
        VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
        VK_CHECK(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
    }

    void setupDescriptorSet()
    {
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

        // Setup a descriptor image info for the current texture to be used as a combined image sampler
        VkDescriptorImageInfo textureDescriptor;
        textureDescriptor.imageView = texture.view;
        textureDescriptor.sampler = texture.sampler;
        textureDescriptor.imageLayout = texture.imageLayout;
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(
                descriptorSet,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &uniformBuffer.descriptor),
            vks::initializers::writeDescriptorSet(
                descriptorSet,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                &textureDescriptor)
        };
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    }

    void preparePipelines()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

        VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

        VkPipelineColorBlendAttachmentState blendAttachmentState = {};
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(
            VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

        VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

        VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

        std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

        // Load shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
        shaderStages[0] = loadShader(getAssetPath() + "shaders/bunnymark/sprite.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/bunnymark/sprite.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(
            pipelineLayout, renderPass, 0);

        pipelineCreateInfo.pVertexInputState = &vertices.inputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages = shaderStages.data();

        VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &spritePipeline));
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        uniformBuffer.create(vulkanDevice, vks::BufferType::device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(uboVS));

        float fov = 90;
        float eyed = height * 0.5f / std::tan(glm::radians(fov) * 0.5f);
        glm::vec3 eye = { 0.0f, 0.0f, -eyed };
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width / -(float)height, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(eye, glm::vec3(0, 0, 0), glm::vec3(0, 1.0, 0));
        view = glm::translate(view, glm::vec3(-(float)width / 2, -(float)height / 2, 0));

        uboVS.projection = projection * view;

        uniformBuffer.uploadFromStaging(&uboVS, sizeof(uboVS), queue);
    }

    void prepare()
    {
        VulkanFramework::prepare();
        generateQuad();
        setupVertexDescriptions();
        prepareUniformBuffers();
        setupDescriptorSetLayout();
        preparePipelines();
        setupDescriptorPool();
        setupDescriptorSet();
        prepared = true;
    }

    virtual void render()
    {
        if (prepared) draw();
        update(frameDeltaTime);
    }

    virtual void viewChanged()
    {
    }

    virtual void onUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        static uint32_t lastCount = -1;
        static char str[0x80];
        if (lastCount != bunnyCount) {
            lastCount = bunnyCount;
            sprintf(str, "%d\nBUNNIES", bunnyCount);
        }
        overlay->text(str);
    }
};

#if defined(_WIN32)
// Windows entry point
VulkanDemo* app;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (app != NULL) {
        app->handleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    for (size_t i = 0; i < __argc; i++) {
        VulkanDemo::args.push_back(__argv[i]);
    };
    app = new VulkanDemo();
    app->initVulkan();
    app->setupWindow(hInstance, WndProc);
    app->prepare();
    app->renderLoop();
    delete (app);
    return 0;
}

#elif defined(__ANDROID__)
// Android entry point
VulkanDemo* app;
void android_main(android_app* state)
{
    app = new VulkanDemo();
    state->userData = app;
    state->onAppCmd = VulkanDemo::handleAppCommand;
    state->onInputEvent = VulkanDemo::handleAppInput;
    androidApp = state;
    app->renderLoop();
    delete (app);
}
#endif
