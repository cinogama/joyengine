# JoyEngineECS

一个使用 C++ 和 Woolang 实现的，基于ECS架构的游戏引擎。

A game engine based on ECS architecture implemented in C++ and Woolang. 

## 编译说明（How to compile）


在编译之前，需要安装baozi，用以获取引擎编辑器所需的一些包：

<del>安装baozi可以通过[Chief_Reloaded](https://github.com/BiDuang/Chief_Reloaded)进行</del>
在新版本 Chief 发布之前，请手动安装 Baozi。

编译流程中会自动调用baozi拉取最新的pkg，可以通过将cmake配置：`JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD` 设置为 OFF 关闭安装操作

> CMake版本至少达到 3.17.2


若在非Windows平台编译（以ubuntu为例），需要另外安装OpenGL和其他SDK，这里列出来方便直接使用

```shell
apt install libx11-dev libxext-dev libxtst-dev libxrender-dev libxmu-dev libxmuu-dev 
apt install libxrandr-dev libxinerama-dev libxcursor-dev 
apt install libgl1-mesa-dev libglu1-mesa-dev

```

#### 特别注意
引擎的编辑器目前仅在windows上可以使用完整功能，并且需要配置环境 `MSBUILD` 和 `CMAKE`
此环境变量应该是MSBUILD.exe的所在目录，若未配置正确的环境变量，引擎编辑器初次启动时将会提示配置。

---

## 鸣谢（Acknowledgments）

感谢 [OpenAl Soft](https://openal-soft.org/) 为这个引擎提供了非常棒的声音支持，不过需要额外说明的是，引擎使用的OpenAL Soft是OpenAL的软实现，使用LGPL协议开源，可以在[这里](https://github.com/kcat/openal-soft.git)获取到OpenAL Soft的原始代码，JoyEngine使用的版本为原始的1.23.1版本。

Thanks to [OpenAl Soft](https://openal-soft.org/) for the awesome sounds of this engine Support, but what needs to be explained is that the OpenAL Soft used is the soft implementation of OpenAL, which is open-sourced using the LGPL protocol, and can be obtained from [here](https://github.com/kcat/openal-soft.git) The original code, the version used by JoyEngine is the original 1.23.1 version.

感谢 [glew](https://github.com/nigels-com/glew)、[glfw](https://www.glfw.org/)为引擎的渲染提供OpenGL拓展和界面接口。

Thanks to [glew](https://github.com/nigels-com/glew) and [glfw](https://www.glfw.org/) for providing OpenGL extension and interface for engine rendering.

感谢 [stb](https://github.com/nothings/stb) 库提供的方便易用的各种单文件库，JoyEngine目前使用了其中的图片读取和写入，以及字体加载，stb展示了何为简洁和精妙，作者的推特页面在[这儿](https://twitter.com/nothings)。

Thanks to the [stb](https://github.com/nothings/stb) library for providing various convenient and easy-to-use single-file libraries. JoyEngine currently uses it for image reading and writing, as well as font loading. stb shows What is concise and subtle, the author's Twitter page is [here](https://twitter.com/nothings).

感谢 [imgui](https://github.com/ocornut/imgui) 让我摆脱了编写繁杂用户界面的痛苦，他们现在好像叫做 `Dear ImGui`。

Thanks to [imgui](https://github.com/ocornut/imgui) for taking the pain out of writing a messy UI, they seem to be called `Dear ImGui` now.

[Imgui Node Editor](https://github.com/thedmd/imgui-node-editor) 是一个有趣的节点编辑器，它基于 imgui。虽然暂时没有完全想好要用它做什么，不过引擎提供了一部分相关的接口，同时按照我的设想实现了 Wolkflow 机制，谢谢它的作者。

[Imgui Node Editor](https://github.com/thedmd/imgui-node-editor) is an interesting node editor based on imgui. Although I haven't fully figured out what to use it for, the engine provides some related interfaces, and according to my idea, the Wolkflow mechanism is implemented. Thank you to its author.

[ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) 提供了代码编辑器的控件，感谢 @BalazsJako 提供了这个控件。

[ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) provides the widget of the code editor, thanks to @BalazsJako for providing this widget.

感谢 [box2d](https://box2d.org/)提供了良好的物理效果，这真的是个很棒的物理引擎，非常感谢。

Thanks to [box2d](https://box2d.org/) for providing nice physical effects, This is really a great physics engine, thanks a lot.

感谢 [Assimp](https://www.assimp.org/) 这是一个能导入读取许多不同三维模型格式数据的库，功能非常强大，后续JoyEngine在支持3D游戏开发的时候，少不了这个库的帮助——不过现在为时尚早；感谢Assimp的开发者！

Thanks to [Assimp](https://www.assimp.org/). This is a library that can import and read data from many different 3D model formats, with very powerful functions. In the future, JoyEngine will be able to support 3D game development with the help of this library - but it's too early now; Thanks to the developers of Assymp!

很想感谢 [Khronos](https://www.khronos.org/), 他们提供了 glslang 和非常易用的 vulkan-header，这可以使得开发vulkan摆脱SDK（虽然为了使用验证层还是得装上，但是毕竟不是必选项了），并且翻译SPIR-V也更加方便；但是一想到他们把vulkan设计得十分复杂，一些很常见的功能反而需要各种拓展，Uniform Buffer里的数据居然在绑定之后就不能更新了等等等等；以至于想到这里，我就不怎么想感谢他们了。

I would like to thank [Khronos](https://www.khronos.org/), they provide glslang and the very easy-to-use vulkan-header, which can make developing vulkan without the SDK (although you still have to install it in order to use the verification layer, but after all is not a required option), and it is more convenient to translate SPIR-V; but when I think that they designed vulkan to be very complicated, some very common functions require various extensions, and the data in the Uniform Buffer cannot be updated after binding. , the descriptor set cannot be updated after being bound, etc.; so much so that when I think of this, I don't really want to thank them.

感谢 [woolang](https://github.com/cinogama/woolang) 给JoyEngine提供了一门强类型和静态类型的脚本语言，感谢它的创作者——@mr_cino，额，好像就是我。那么谢谢我自己。

Thanks to [woolang](https://github.com/cinogama/woolang) for providing JoyEngine with a strongly typed and statically typed scripting language, and thanks to its creator - @mr_cino, well, it seems to be me. Then thank myself.

最后感谢群里的[牛牛](https://github.com/MistEO/Pallas-Bot)，感谢牛牛提供的技术支持。

Finally, I would like to thank [Niu Niu](https://github.com/MistEO/Pallas-Bot) (A chatbot) in the group for the technical support provided by Niu Niu.
