# BunnyMarkGame

Write a bunnymark benchmark from scratch with Vulkan, to see how much faster can I do than cocos2d-x/pixijs.
<img src="https://github.com/re-esper/BunnyMarkGame/blob/master/screenshot/my-bunnymark-windows.jpg" width="400" height="300">

## Rules
* Completely same with the pixijs's [original bunnymark](https://www.goodboydigital.com/pixijs/bunnymark/)([source code](https://www.goodboydigital.com/pixijs/bunnymark/js/bunnyBenchMark.js)), consistent features and resources.
* Focus on rendering performance, so multi-threading is not used to speed up game logic.


## Results
My bunnymark is able to render **1,120,000** bunnies on my pc(Intel Core i7-7700K + GTX 1060 3GB) and **185,000** bunnies on android(MI6, Snapdragon835), and keep it at 60fps. It is nearly **30x** faster than cocos2d-x bunnymark on my pc, and nearly **20x** faster on my android phone.

| Engine       | desktop (7700k+1060)        | mobile (SD835)  |
| ------------ | ------------ | ------------ |
| **mine (vulkan)** | **1,120,000**     | **185,000**     |
| cocos2d-x    | 37,500    | 9,600    |
| pixi.js      | 248,000   | 54,400  |

#### Notes:
1. cocos2d-x uses its latest version 4.0.
2. cocos2d-x bunnymark set `#define CC_USE_CULLING 0` at ccConfig.h to improve performance.
3. pixijs bunnymark uses the [original version](https://www.goodboydigital.com/pixijs/bunnymark/), instead of [this](https://pixijs.io/bunny-mark/), the original version is much faster.

## Libraries
The basic vulkan code of my bunnymark is based on SaschaWillems's [Vulkan Examples](https://github.com/SaschaWillems/Vulkan)

My bunnymark utilizes the following open-source libraries:
* [OpenGL Mathematics](https://github.com/g-truc/glm)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [stb_image](https://github.com/nothings/stb)
* [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [volk](https://github.com/zeux/volk)

