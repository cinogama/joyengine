# JoyEngineECS

一个使用 C++ 和 Woolang 实现的，基于ECS架构的游戏引擎。

A game engine based on ECS architecture implemented in C++ and Woolang. 

## 编译说明（How to compile）

编译 JoyEngine 前，需要安装OpenAL-SDK，对于Windows用户而言，SDK已经随着子模块一并拉取到 3rd 目录下了，不需要额外的配置，若运行时提示找不到 `OpenAL32.dll`，可通过安装 3rd 目录下的 `oalinst.exe` 安装所需的库。

对于其他平台，需要手动安装SDK，以 Ubuntu 为例，可以执行：

```shell
apt install libopenal-dev
```

另外可能需要安装些别的东西，这里列出来方便直接使用

```shell
apt install libx11-dev libxext-dev libxtst-dev libxrender-dev libxmu-dev libxmuu-dev 
apt install libxrandr-dev libxinerama-dev libxcursor-dev 
apt install libgl1-mesa-dev libglu1-mesa-dev

```

在编译之前，还需要安装baozi，用以获取引擎编辑器所需的一些包：

**安装baozi可以通过[Chief_Reloaded](https://github.com/BiDuang/Chief_Reloaded)进行**

编译流程中会自动调用baozi拉取最新的pkg，可以通过将cmake配置：`INSTALL_PKG_WHEN_BUILD` 设置为 OFF 关闭安装操作

> 推荐CMake版本至少达到 3.20，以保证baozi和编辑器的编译流程能够正常进行

#### 特别注意
引擎的编辑器目前仅在windows上可以使用完整功能，并且需要配置环境 `MSBUILD` 
此环境变量应该是MSBUILD.exe的所在目录，

---

## 鸣谢（Acknowledgments）

感谢 [OpenAL](http://www.openal.org/) 和（或）[OpenAl Soft](https://github.com/kcat/openal-soft.git)为这个引擎提供了非常棒的声音支持，不过需要额外说明的是，引擎使用的OpenAL 1.1基于LGPL协议开源（或许是吧，我在维基百科上看到了各种复杂的说法），并且使用的是官方网站上提供的原始版本的SDK，并没有做出任何修改；使用的OpenAL Soft是OpenAL的软实现，使用LGPL协议开源，可以在[这里](https://github.com/kcat/openal-soft.git)获取到OpenAL Soft的原始代码，JoyEngine使用的版本为原始的1.23.1版本。
> 默认情况下，引擎使用OpenAL Soft而不是OpenAL，这是为了更方便集成。

Thanks to [OpenAL](http://www.openal.org/) for providing great sound support for this engine, but it needs to be additionally explained that the engine uses OpenAL 1.1 and is open source based on the LGPL protocol (maybe, I am in Wikipedia has seen various complicated statements), and used the original version of the SDK provided on the official website without any modification.

感谢 [glew](https://github.com/nigels-com/glew)、[glfw](https://www.glfw.org/)为引擎的渲染提供OpenGL拓展和界面接口。

Thanks to [glew](https://github.com/nigels-com/glew) and [glfw](https://www.glfw.org/) for providing OpenGL extension and interface for engine rendering.

感谢 [stb](https://github.com/nothings/stb) 库提供的方便易用的各种单文件库，JoyEngine目前使用了其中的图片读取和写入，以及字体加载，stb展示了何为简洁和精妙，作者的推特页面在[这儿](https://twitter.com/nothings)。

Thanks to the [stb](https://github.com/nothings/stb) library for providing various convenient and easy-to-use single-file libraries. JoyEngine currently uses it for image reading and writing, as well as font loading. stb shows What is concise and subtle, the author's Twitter page is [here](https://twitter.com/nothings).

感谢 [imgui](https://github.com/ocornut/imgui) 让我摆脱了编写繁杂用户界面的痛苦，他们现在好像叫做 `Dear ImGui`。

Thanks to [imgui](https://github.com/ocornut/imgui) for taking the pain out of writing a messy UI, they seem to be called `Dear ImGui` now.

感谢 [box2d](https://box2d.org/)提供了良好的物理效果，这真的是个很棒的物理引擎，非常感谢。

Thanks to [box2d](https://box2d.org/) for providing nice physical effects, This is really a great physics engine, thanks a lot.

感谢 [woolang](https://github.com/cinogama/woolang) 给JoyEngine提供了一门强类型和静态类型的脚本语言，感谢它的创作者——@mr_cino，额，好像就是我。那么谢谢我自己。

Thanks to [woolang](https://github.com/cinogama/woolang) for providing JoyEngine with a strongly typed and statically typed scripting language, and thanks to its creator - @mr_cino, well, it seems to be me. Then thank myself.

最后感谢群里的[牛牛](https://github.com/MistEO/Pallas-Bot)，感谢牛牛提供的技术支持。

Finally, I would like to thank [Niu Niu](https://github.com/MistEO/Pallas-Bot) (A chatbot) in the group for the technical support provided by Niu Niu.
