# 音频（Audio）

本文档是[术语表](README.md)中「音频」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述音频的概念与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 音频是什么

音频（Audio）负责播放声音，并模拟发声源处于场景中特定位置时的听感（远近、方位、多普勒效应）以及混响、回声等环境效果。JoyEngine 的音频后端基于 OpenAL（openal-soft）。

挂在实体上的音频组件由*音频系统*（类型名 `Audio::AudioUpdatingSystem`）驱动，它和其它[系统](system.md)一样需要注册进[世界](world.md)才会工作。

### 三个基本概念

| 概念 | 含义 | 数量 |
| --- | --- | --- |
| *音频缓冲*（buffer） | 一段解码好的音频数据，可被多个声源共享 | 任意 |
| *声源*（source） | 一个播放实例：绑定缓冲后播放、暂停、停止，带位置、速度、音量、音调、循环属性 | 任意 |
| *听者*（listener） | “耳朵”：所有声源都相对它的位置与朝向被听到 | **全局唯一** |

音频缓冲从 **WAV** 文件加载，支持 8/16 位整数 PCM 与 32 位浮点、单声道或立体声；C++ 侧也可以由原始采样数据直接创建。

加载走与纹理相同的缓存管线：引擎按 `<音频路径>.je4cache` 缓存加载（裸 PCM 波形区开头，后跟解码信息与配置文档），缓存缺失或失效时经 **pkg::audio** 解码源文件并生成；可选的 `<音频路径>.je4audio`（TOML，编辑器“音频属性”窗口编辑）可配置 `streaming`（流式播放，见下文）与 `format`（缓存格式 "i8" 缺省 / "i16" / "f32"，PCM 转换后存储——因此任何源编码的单/双声道 WAV 都能加载）。相同路径的重复加载各自创建独立的缓冲区。

### 流式播放

默认情况下音频整段解码进内存。对时长较长的音频（如背景音乐），可在编辑器“音频属性”窗口勾选“流式播放”（或直接编辑 `<音频路径>.je4audio` 的 `streaming = true`）：

* 整段 PCM 不驻留内存：缓冲区只保持 `.je4cache` 打开，声源播放时按需分块读取（每块约 0.25 秒，每个声源排队 4 块，约 1 秒余量），内存占用与音频时长无关；
* 循环、暂停/继续、停止、跳转与整段载入的缓冲区表现一致，进度接口返回的仍是 PCM 字节偏移；
* 同一路径的重复加载各自持有独立的缓存文件句柄；缓冲区用毕仍用 `je_audio_buffer_close`（Woolang 的 `buffer::close`）释放，缓存文件随之关闭。

短促音效一般不必开启流式播放：分块读取会带来额外的磁盘 I/O，且每个播放中的声源各占一份 4 块的缓冲。

### 把声音挂到实体上

给实体添加 `Audio::Source` 与 `Audio::Playing`（编辑器“添加组件→音频→音源”预设），再配合 `Transform::Translation` 提供位置：

| 组件 | 成员（默认值） | 说明 |
| --- | --- | --- |
| `Audio::Source` | `pitch`（1）、`volume`（1） | 实体专属的声源，复制实体时新实体得到独立的声源 |
| `Audio::Playing` | `buffer`、`play`（true）、`loop`（true） | 播放控制；`buffer` 是指向 `.wav` 文件的资源路径 |

`Playing` 的行为约定：

* `play` 相当于播放/暂停开关：置为 false 时**暂停**，再置为 true 时从暂停处继续；
* **非循环播放结束后，`play` 会被自动置回 false**，想再播一次只需重新置为 true；
* 更换 `buffer` 会先停止当前播放，若 `play` 为 true 则从头播放新音频；`buffer` 为空时停止播放。

⚠️ 注意 `play` 与 `loop` 默认都是 true：一个设置了 `buffer` 的新 `Playing` 组件会立即开始循环播放。一次性音效请先把 `loop` 设为 false。

Woolang 脚本中可以读写 `Playing` 的成员，并通过 `Playing::set_buffer` / `get_buffer` 替换缓冲、`Source::get_source` 取得底层声源：

```woolang
UPDATE!
{
    // 移动时循环播放脚步声，停下时暂停
    system! footstep(
        playing: Audio::Playing,
        vel: Physics2D::LinearVelocity,
    )
    {
        let v = vel.value->get;
        playing.loop->set(true);
        playing.play->set(mathf::abs(v[0]) > 0.1);
    }
}
```

### 3D 空间化与听者

`Audio::Listener` 组件把听者绑定到实体，通常挂在[摄像机](camera.md)实体上。成员 `volume`（1）为听者音量，`face`（0, 0, 1）与 `up`（0, 1, 0）是实体局部空间中的前方与上方，会随实体的世界旋转一起转动。

* 声源与听者的速度由相邻两帧的位置差自动推算，用于多普勒效应，无需手动设置；
* 听者全局唯一：存在多个 `Listener` 组件时，它们会先后写入同一个全局听者、相互覆盖，**请只保留一个**；
* 引擎没有调整 OpenAL 的距离衰减模型，衰减按 openal-soft 的默认参数计算；
* Woolang 的 `je::audio::listener::update` 还能设置 `global_gain`（全局增益），实际音量为 `volume × global_gain`，组件不会改动它，适合用作总音量。

⚠️ 注意 openal-soft 默认**只对单声道音频做空间化**，立体声缓冲会忽略位置直接播放。需要 3D 定位的音效请使用单声道 WAV，背景音乐等可以使用立体声。

### 效果器与滤波器

除了直达的声音，声源还可以经由*辅助发送*（send）把声音送进*效果槽*（effect slot），效果槽中装载一个*效果器*（effect）：

```
声源 ──直达声（可挂滤波器）─────────────────→ 听者
  └──辅助发送 0..N（每路可挂滤波器）→ 效果槽（装载效果器）→ 听者
```

一个效果槽可以被多个声源共享；一个声源可以同时使用多路发送（引擎上限 8 路，实际可用路数取决于音频设备）。可用的效果器：

| 类别 | 效果器（Woolang 名） |
| --- | --- |
| 空间环境 | 混响 `reverb`、EAX 混响 `eaxreverb`、回声 `echo` |
| 调制 | 合唱 `chorus`、镶边 `flanger`、环形调制 `ring_modulator`、自动哇音 `autowah` |
| 音高与频率 | 移调 `pitch_shifter`、移频 `frequency_shifter`、人声变形 `vocal_morpher` |
| 音色与动态 | 失真 `distortion`、均衡 `equalizer`、压缩 `compressor` |

*滤波器*（filter）分为低通、高通、带通三种，参数为整体增益 `gain` 与低频/高频增益 `gain_lf` / `gain_hf`；它可以挂在直达声路径上，也可以挂在某一路辅助发送上。

⚠️ 注意 修改效果器参数后，需要**重新把效果器绑定到效果槽**，新参数才会作用到已经绑定的效果槽与声源上。效果器与滤波器依赖设备的 EFX 扩展，设备不支持时引擎会给出警告，效果不生效。

#### Woolang（`je::audio`）

不经过组件、直接用声源播放，并挂上混响与低通滤波：

```woolang
import je::audio;

let buf = je::audio::buffer::load("@/sounds/bgm.wav")->unwrap;
let src = je::audio::source::create();
src->set_buffer(buf);
src->update(func(info){ info.loop = true; info.gain = 0.5; });
src->play();

// 混响效果 → 效果槽 → 声源的第 0 路辅助发送
let reverb = je::audio::effect::create:<je::audio::effect::reverb>();
reverb->update(func(r){ r.decay_time = 3.0; });
let slot = je::audio::effect_slot::create();
slot->bind_effect(reverb);
src->bind_effect_slot_and_filter(option::value(slot), option::none, 0);

// 低通滤波器挂在直达声路径上
let lowpass = je::audio::filter::create();
lowpass->update(func(f){ f.gain_hf = 0.2; });
src->set_filter(option::value(lowpass));
```

#### C++（`jeecs::audio`，头文件 `jeecs/audio.hpp`）

C++ 封装类为 `buffer`、`source`、`listener`、`effect<T>`、`effect_slot`、`filter`，参数修改统一用 `update(lambda)` 提交：

```cpp
using namespace jeecs;

auto buf = audio::buffer::load("@/sounds/hit.wav");      // 失败时为 nullopt
auto src = audio::source::create();
src->set_playing_buffer(buf.value());
src->update([](je_audio_source_t* s) { s->m_loop = false; });
src->play();

auto echo = audio::effect<je_audio_effect_echo_t>::create();
echo->update([](je_audio_effect_echo_t* e) { e->m_delay = 0.2f; });
auto slot = audio::effect_slot::create();
slot->bind_effect(echo);
src->bind_effect_slot(0, slot, std::nullopt);             // 第 0 路发送，不挂滤波器
```

### 相关术语

参见[术语表](README.md)：

* [摄像机](camera.md)——听者组件的常见挂载对象；
* [变换](transform.md)——声源与听者位置的来源；
* [系统](system.md)、[世界](world.md)——音频系统的注册与执行阶段。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。OpenAL 后端位于 `src/jeecs_audio_impl_openal.cpp`，组件同步位于 `src/jeecs_core_audio_system.cpp`，C ABI 与 C++ 封装见 `include/jeecs/audio.hpp`，Woolang 绑定见 `je/audio.wo`、`src/jeecs_audio_woolang_api.cpp` 以及 `je/towoo/towoo.wo`（组件辅助函数）。

### openal-soft 与 LGPL 约束

音频后端依赖子模块 `3rd/openal-soft`（1.24.3）。CMake 在 `JE4_SPECIFY_OPENAL_LIB_NAME` 为空时以 `BUILD_SHARED_LIBS ON`（FORCE）构建 openal-soft；该选项非空时改用指定名称的平台 OpenAL 库。

⚠️ 注意 openal-soft 采用 LGPL 授权，**必须保持为动态库，禁止静态链接**——这是仓库的硬性约束（见 `AGENTS.md`），调整构建脚本时不要把它并入静态链接的依赖中。

### 镜像结构与引用计数

`je_audio_source_t`、`je_audio_listener_t`、`je_audio_filter_t`、`je_audio_effect_slot_t` 以及各效果器结构体都是 **CPU 侧的参数镜像**：修改字段后调用对应的 `je_audio_*_update` 才会提交给 OpenAL。C++ 的 `update(lambda)` 与 Woolang 的 `update(func)` 都是“取镜像→修改→提交”的封装。效果器在数据结构体之前额外分配一个 `EffectHead`（原生实例、效果种类、引用计数），`je_audio_effect_*` 接口以 `void*` 统一处理各类效果器。

缓冲、效果器、效果槽、滤波器都带引用计数：声源通过 `ContextHelper::Holder` 持有当前的缓冲、直达滤波器与每路发送的槽/滤波器，效果槽持有绑定的效果器。`close` 只减少引用计数，计数归零才真正释放，因此关闭仍被绑定的资源是安全的。

### 流式播放（实现）

`je_audio_buffer_load` 解析缓存文档后，`streaming` 为 true 的缓冲区不读入 PCM：`m_data` 为 `nullptr`，`m_stream_instance` 持有已打开的 `.je4cache` 与 PCM 区偏移，`m_size` 仍是 PCM 总字节数（时长信息不变）。`je_audio_buffer_create` 创建的缓冲区始终整段载入。

播放时每个声源维护一个 4 块的 OpenAL 缓冲队列（块大小 ≈ `byte_rate / 4`，按样本帧对齐）：

* 播放/跳转（游戏线程）与设备更新线程都从 PCM 区按偏移 `je_file_seek` + `je_file_read` 读块，`alBufferData` 后 `alSourceQueueBuffers` 入队；
* 设备更新线程每 100ms 检查所有流式声源：出队 `AL_BUFFERS_PROCESSED` 的块并累计已播字节，再补读后续块；队列空且未播完时 `alSourcePlay` 重新开始（欠载恢复），流自然播完时清除播放意图；
* 流式声源不使用 `AL_LOOPING`（openal-soft 对 looping source 不报告 `AL_BUFFERS_PROCESSED`、也禁止 unqueue），循环由读位置回绕实现；
* 播放进度 = 已出队字节 + `AL_BYTE_OFFSET`（openal-soft 的 offset 相对当前队列头），循环时对总长取模；跳转会停源、清空队列并从新偏移补块；
* `je_audio_source_play_state_get` 对流式声源返回逻辑状态（播放意图 / 暂停标记），补块间隔内的欠载不会让 `Audio::Playing` 误判播放结束；
* 设备热插拔：转储时记录 `已出队字节 + AL_BYTE_OFFSET`，恢复时从该偏移重建队列；块缓冲名属于设备上下文，随转储释放、恢复时重新生成。

设备更新线程的定时唤醒使用独立的条件变量等待，唤醒后按 g2（缓冲区）→ g1（声源）→ g0（上下文，共享）的锁序补块并检查设备连接，避免与其它音频线程形成锁序反转。

### Context：设备、上下文与热插拔

后端只有一个全局 `Context`。它枚举播放设备（支持 `ALC_ENUMERATE_ALL_EXT` 时枚举全部设备），并为当前设备创建 OpenAL 上下文；设备支持 `ALC_EXT_EFX` 且最大辅助发送数大于 0 时，通过 `alcGetProcAddress` 加载 EFX 函数，否则关闭 EFX 并告警。

后台线程每 100ms 检查一次（同时为流式声源补块）：当前设备断开，或者正在使用的默认设备不再是系统默认设备时，重新枚举并切换到新的默认设备。切换流程：

1. `dump_and_close_playing_states`：为每个声源转储播放状态与字节偏移，删除所有 AL 对象；
2. 销毁旧上下文，在新设备上创建上下文；
3. `restore_playing_states`：重建 AL 对象，恢复缓冲、播放状态、播放进度、滤波器与各路发送。

`je_audio_devices_disconnect_all` 复用同一机制，供移动平台切到后台时彻底停止音频；恢复时用 `je_audio_devices_enumerate_and_do` 重新枚举即可。

提交位置、速度、朝向时，z 分量被取反——引擎坐标系与 OpenAL 坐标系的 z 轴方向相反。

### AudioUpdatingSystem：组件到声源

音频系统只实现 `CommitUpdate`（第 7 阶段，晚于 `LATE_UPDATE!`），因此脚本在同一帧内对组件的修改当帧生效。它依次处理三类切片：

1. **Listener + Translation**：速度 = 位置差 / `max(deltatime, 0.0001)`；`face` / `up` 乘以世界旋转；把位置、速度、朝向与 `m_gain = volume` 写入全局听者（不触碰 `m_global_gain`）；
2. **Source + Translation**：同法推算速度，写入增益、音调、位置、速度；
3. **Source + Playing**：驱动播放状态机。

`Playing` 内部状态 `state` 有 `STOPPED`、`PLAYING`、`LOOPED_PLAYING` 三种：

| 当前状态 | 条件 | 动作 |
| --- | --- | --- |
| 任意 | `buffer` 为空 | 停止声源，`state = STOPPED`、`play = false` |
| `STOPPED` | `play` 为 true | 按 `loop` 设置循环并开始播放，进入 `PLAYING` / `LOOPED_PLAYING` |
| 播放中 | `loop` 与状态不符 | 更新循环标记，在两种播放状态间切换 |
| 播放中 | 声源已停止且缓冲未变 | 播放自然结束：`state = STOPPED`、`play = false` |
| 播放中 | 缓冲刚被更换（底层已自动停止） | 若 `play` 为 true 立即重新播放 |
| 播放中 | 声源暂停 / 播放中 | 按 `play` 恢复或暂停 |

缓冲的绑定每帧都会调用 `set_playing_buffer`，底层只有在缓冲真正改变时才停止播放并返回 true。`Playing` 的拷贝构造把 `state` 重置为 `STOPPED`，`Source` 的拷贝构造创建新的 AL 声源，所以由预制体实例化出的实体都拥有独立的播放状态。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 音频组件 | `include/jeecs/components.hpp`（`namespace Audio`） |
| C ABI、效果器参数与默认值、C++ 封装 | `include/jeecs/audio.hpp`（逐函数说明见 [doc/capi/audio](../capi/audio/)） |
| OpenAL 后端（设备、热插拔、EFX、缓存加载） | `src/jeecs_audio_impl_openal.cpp` |
| 缓存生成器脚本（pkg::audio 解码） | `builtin/script/internal/cache/audio.wo` |
| 组件同步与播放状态机 | `src/jeecs_core_audio_system.cpp` |
| Woolang 绑定 | `je/audio.wo`、`src/jeecs_audio_woolang_api.cpp` |
| 组件辅助函数（towoo） | `je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| openal-soft 构建配置 | `CMakeLists.txt`（`JE4_SPECIFY_OPENAL_LIB_NAME`）、`3rd/openal-soft` |
