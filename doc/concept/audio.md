# 音频（Audio）

本文档是[术语表](README.md)中「音频」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述音频的概念与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 音频是什么

音频（Audio）负责播放声音，并模拟发声源处于场景中特定位置时的听感（远近、左右方位、多普勒效应），以及遮挡、混响、回声等环境效果。JoyEngine 的音频后端基于 OpenAL（openal-soft）。

挂在实体上的音频组件由*音频系统*（类型名 `Audio::AudioUpdatingSystem`）驱动，它和其它[系统](system.md)一样需要注册进[世界](world.md)才会工作。编辑器新建的世界会自动添加音频系统。

### 三个基本概念

| 概念 | 含义 | 数量 |
| --- | --- | --- |
| *音频缓冲*（buffer） | 一段解码好的音频数据，可被多个声源共享 | 任意 |
| *声源*（source） | 一个播放实例：绑定缓冲后播放、暂停、停止，带位置、速度、音量、音调、循环与距离衰减属性 | 任意 |
| *听者*（listener） | “耳朵”：所有声源都相对它的位置与朝向被听到 | **全局唯一** |

音频缓冲从 **WAV** 文件加载，支持 8/16 位整数 PCM 与 32 位浮点、单声道或立体声；C++ 侧也可以由原始采样数据直接创建。

加载走与纹理相同的缓存管线：引擎按 `<音频路径>.je4cache` 缓存加载（裸 PCM 波形区开头，后跟解码信息与配置文档），缓存缺失或失效时经 **pkg::audio** 解码源文件并生成；可选的 `<音频路径>.je4audio`（TOML，编辑器“音频属性”窗口编辑）可配置 `streaming`（流式播放，见下文）与 `format`（缓存格式 "i8" 缺省 / "i16" / "f32"，PCM 转换后存储——因此任何源编码的单/双声道 WAV 都能加载）。相同路径的重复加载共享同一个缓冲区实例（引用计数），缓存内容变化后重新加载会得到新实例，因此播放同一音效的多个实体不会重复占用内存。

### 流式播放

默认情况下音频整段解码进内存。对时长较长的音频（如背景音乐），可在编辑器“音频属性”窗口勾选“流式播放”（或直接编辑 `<音频路径>.je4audio` 的 `streaming = true`）：

* 整段 PCM 不驻留内存：缓冲区只保持 `.je4cache` 打开，声源播放时按需分块读取（每块约 0.25 秒，每个声源排队 4 块，约 1 秒余量），内存占用与音频时长无关；
* 循环、暂停/继续、停止、跳转与整段载入的缓冲区表现一致，进度接口返回的仍是 PCM 字节偏移；
* 同一路径的重复加载共享同一个流式缓冲区（因而共享同一个缓存文件句柄）；缓冲区用毕仍用 `je_audio_buffer_close`（Woolang 的 `buffer::close`）释放，最后一份引用释放时缓存文件随之关闭。

短促音效一般不必开启流式播放：分块读取会带来额外的磁盘 I/O，且每个播放中的声源各占一份 4 块的缓冲。

### 实体音源：`Audio::Source`

一个实体一个持久声部：`Audio::Source` 同时承载“发什么”与“怎么发”。循环的环境声、BGM、机器嗡鸣用它；一次性的角色事件音见下一节的“一次性播放”。

| 成员 | 默认值 | 说明 |
| --- | --- | --- |
| `buffer` | 空 | 音频资源路径（Inspector 拖放 `.wav`）；为空时声源停止 |
| `volume` | 1 | 声源增益 |
| `pitch` | 1 | 播放速度/音调 |
| `play` | true | 播放/暂停开关：false 暂停，再 true 从暂停处继续 |
| `loop` | true | 是否循环 |
| `spatial` | true | false = 相对听者（UI/环境声），不做距离衰减与左右声像 |
| `bus` | `SE` | 音量总线分组（BGM/BGS/ME/SE/VOICE + 自定义槽位）；`NONE` = 不经过总线 |
| `reference_distance` | 1 | 参考距离：此距离内为满增益（钳制模型） |
| `max_distance` | 32 | 最大距离：线性钳制模型下衰减到此距离归零 |
| `rolloff` | 1 | 衰减率 |
| `min_gain` | 0 | 距离衰减的下限 |
| `max_gain` | 1 | 距离衰减的上限 |
| `direction` | (0, 0, 1) | 声源局部朝向；乘实体世界旋转后作为锥形指向的轴 |
| `cone_inner_angle` | 360 | 锥形内角（度），此角度内为满增益 |
| `cone_outer_angle` | 360 | 锥形外角（度），此角度外为外角增益 |
| `cone_outer_gain` | 0 | 外角增益 |
| `air_absorption` | 0 | 空气吸收因子：随距离衰减高频，0 = 关闭；需要 EFX |

锥形指向默认内外角都是 360°，即无方向性；要模拟喇叭、声源朝向明显的角色时再设置。2D 场景里使用锥形时，请把 `direction` 设成 XY 平面内的方向（例如 `(1, 0, 0)`），否则默认的 +Z 朝向在 XY 平面上没有意义。

`play` 的行为约定：

* `play` 相当于播放/暂停开关：置为 false 时**暂停**，再置为 true 时从暂停处继续；
* **非循环播放结束后，`play` 会被自动置回 false**，想再播一次只需重新置为 true；
* 更换 `buffer` 会先停止当前播放，若 `play` 为 true 则从头播放新音频；`buffer` 为空时停止播放。

⚠️ `play` 与 `loop` 默认都是 true：一个设置了 `buffer` 的新 `Source` 会立即开始循环播放。一次性音效请把 `loop` 设为 false，或改用一次性播放接口。

实体没有 `Transform::Translation`（或 `spatial = false`）时，声源按“相对听者”播放：不做距离衰减、不做左右声像，适合挂在 UI 实体上的音效。

### 2D 与 3D：`Audio::Listener`

`Audio::Listener` 组件把听者绑定到实体，通常挂在[摄像机](camera.md)实体上。系统不读取摄像机组件，2D/3D 完全由组件成员决定：

| 成员 | 默认值 | 说明 |
| --- | --- | --- |
| `volume` | 1 | 听者增益 |
| `use_2d` | true | true = 纯 XY 听音空间；false = 完整 3D |
| `distance_model` | `linear_clamped` | 距离衰减模型，见下表 |
| `doppler_factor` | 1 | 多普勒系数；0 关闭 |
| `face` | (0, 0, 1) | 听者局部前方，随实体世界旋转 |
| `up` | (0, 1, 0) | 听者局部上方，随实体世界旋转 |

`use_2d = true` 时，听者与所有空间化声源的 **z 一律按 0 提交**，距离与左右声像只由 XY 决定。横板（摄像机在 -z 看向 +z）与俯视角（摄像机带 Z 轴旋转）都使用这一模式：声源在屏幕右侧就从右声道出声；摄像机绕 Z 旋转时音场随视角一起旋转。摄像机与游戏物体的 z 可以任意摆放，不再影响衰减。

`use_2d = false` 时使用完整 3D 空间（位置、锥形、多普勒都参与），适合 3D 场景。

距离衰减模型（`Audio::Listener::distance_model`）：

| 值 | 曲线 |
| --- | --- |
| `none` | 不衰减 |
| `inverse` | 反比衰减 |
| `inverse_clamped` | 反比衰减，参考距离内不放大（openal-soft 默认） |
| `linear` | 线性衰减 |
| `linear_clamped` | 线性衰减，参考距离内不放大（**默认**，2D 下最直观） |
| `exponent` | 指数衰减 |
| `exponent_clamped` | 指数衰减，参考距离内不放大 |

* 声源与听者的速度由相邻两帧的位置差自动推算，用于多普勒效应，首帧不会产生速度尖峰；
* 听者全局唯一：存在多个 `Listener` 组件时，它们会先后写入同一个全局听者、相互覆盖，**请只保留一个**；
* Woolang 的 `je::audio::listener::update` 还能设置 `global_gain`（全局增益），实际音量为 `volume × global_gain`，组件不会改动它，适合用作总音量。

⚠️ openal-soft 默认**只对单声道音频做空间化**。本引擎在设备支持 `AL_SOFT_source_spatialize` 时会给 `spatial = true` 的声源强制开启空间化（立体声缓冲会先下混），但定位音效仍建议使用单声道 WAV；`spatial = false` 的立体声（BGM 等）保持原样。

耳机下 openal-soft 可以启用 HRTF 双耳渲染。引擎创建 context 时允许后端自动选择（`ALC_SOFT_HRTF` 的 `DONT_CARE`），并在日志中记录实际状态；是否真正启用取决于设备、耳机检测与用户的 openal 配置。

### 一个实体发出多种声音

角色通常会同时拥有脚步、语音、受击、武器等多种声音，而 ECS 中同一组件类型每个实体只能有一个。推荐两层做法：

1. **事件音**（脚步、金币、弹壳、受击等）：用一次性播放接口，不创建实体、不需要声源句柄。脚本预加载 buffer 后反复播放：
   `je::audio::play_once_at(buf, x, y)`（2D）或 `play_once_at_3d(buf, x, y, z)`（3D）。
2. **持久/循环音**（BGM、机器嗡鸣、环境声）：每个实体一个 `Audio::Source`；需要多个同时循环时，用子实体（ECS 标准做法，父变换自动跟随）。
3. 角色可以只挂一个 `Audio::Source` 作为“该实体的声音参数模板”，再调用
   `source->play_oneshot(buffer)`：用组件的 `bus`/音量/距离参数 + 实体当前世界位置发起一次性播放，并自动套用该实体上一帧的遮挡增益与闷音。

一次性播放由引擎分配临时声部并在播放结束后自动回收，同时最多 32 个，超出时回收最早创建的。它还会在 `lowpass_hf < 1` 时自动挂低通滤波器模拟闷音。全局一次性播放接口（`play_once*`）不知道场景遮挡与混响，只播放直达声；需要遮挡/混响的场景音请用实体 `Audio::Source`。

### 音量总线

`bus` 是预设的分组枚举，用于按类别统一调音量：`BGM`（背景音乐）/ `BGS`（背景环境声）/ `ME`（音乐效果）/ `SE`（音效）/ `VOICE`（语音）加 `CUSTOM_0..3` 自定义槽位；`NONE` 不经过总线（增益恒为 1）。各总线增益进程内共享，启动时全部为 1。

最终增益为：

```
声源增益（volume × 遮挡增益） × 总线增益 × 听者增益（volume × global_gain）
```

Woolang：

```woolang
je::audio::bus::SE->set_gain(0.8);
je::audio::bus::BGM->set_gain(0.5);
let sfx_volume = je::audio::bus::SE->gain();
```

C/C++：`je_audio_bus_gain_set` / `je_audio_bus_gain_get`，总线取 `JE_AUDIO_BUS_*` 枚举值。

### 声音遮挡

#### 自定义遮挡实体：`Audio::Occluder`

挂在任意实体上的盒形遮挡体：

| 成员 | 默认值 | 说明 |
| --- | --- | --- |
| `size` | (1, 1, 1) | 局部全尺寸，乘实体比例尺 |
| `occlusion` | 0.6 | 0..1；每命中一次增益 ×= (1 - 值) |
| `muffle` | 0.4 | 0..1；每命中一次高频 ×= (1 - 值) |

系统对每个空间化声源做“听者 → 声源”的线段测试：2D 模式用 XY 投影的旋转矩形，3D 模式用完整 OBB。多个遮挡体同时命中时相乘；线段起点在盒内不算命中。遮挡只影响 `Audio::Source` 实体音源，不影响全局一次性播放。

#### 瓦片地图遮挡

瓦片的音频遮挡/闷音是**内置元数据**（与“是否可通行”同级，不是通用属性）：每个图集瓦片可设置两个系数：

| 系数 | 默认值 | 含义 |
| --- | --- | --- |
| `occlusion` | 0 | 0..1；穿过的格子增益 ×= (1 - 值) |
| `muffle` | 0 | 0..1；穿过的格子高频 ×= (1 - 值) |

默认均为 0，因此**默认不遮挡**。编辑器瓦片面板新增“音频遮挡”区块，可用两个滑条为选中/框选的瓦片设置系数；批量拖动只改被拖动的那一项，另一项保留各瓦片原值。

系统把“听者 → 声源”线段转到地图根实体的局部 XY 网格，用 DDA 遍历穿过的格子；同一格内所有图层取各系数的最大值后累乘，跳过起点格。空格不遮挡。瓦片遮挡在 2D 与 3D 模式下都按 XY 平面计算。

性能上，音频系统会按地图/图集版本（`map_stamp`）把整张地图的系数平整化成两张网格缓存，只有编辑地图或图集时才重建；每帧的射线只做数组索引，不再逐格查询瓦片元数据。

### 混响区域：`Audio::ReverbZone`

一个区域代表一个声学空间（房间、洞穴、走廊、室外）。系统按**声源所在区域**而不是听者所在区域选择混响：洞穴里的怪叫带洞穴混响，即使听者站在走廊里。

| 成员 | 默认值 | 说明 |
| --- | --- | --- |
| `size` | (8, 8, 8) | 区域局部全尺寸，乘实体比例尺 |
| `fade` | 1 | 区域外侧发送权重的衰减距离；0 = 硬边界 |
| `send_gain` | 1 | 区域混响的湿声发送上限 |
| `preset` | `custom` | EAX 混响预设；`custom` 时使用下面四个成员 |
| `decay_time` | 1.49 | 混响衰减时间（秒），仅 `custom` |
| `gain` | 0.32 | 混响湿声增益，仅 `custom` |
| `gain_hf` | 0.89 | 混响高频增益，仅 `custom` |
| `diffusion` | 1.0 | 扩散度，仅 `custom` |

`preset` 提供 EAX 标准预设：`generic`、`padded_cell`、`room`、`bathroom`、`living_room`、`stone_room`、`auditorium`、`concert_hall`、`cave`、`arena`、`hangar`、`carpeted_hallway`、`hallway`、`stone_corridor`、`alley`、`forest`、`city`、`mountains`、`quarry`、`plain`、`parking_lot`、`sewer_pipe`、`underwater`、`drugged`、`dizzy`、`psychotic`。预设值来自 EFX/EAX 规范，比手工调四个浮点更接近真实空间。

系统行为：

* **按声源选区域**：对每个 `spatial` 声源计算它到各区域的有符号距离。区域内权重为 1，向外在 `fade` 距离内平滑降到 0；最多取权重最高的两个区域同时发送，重叠区域因此可以自然交叉淡化，不再“体积最小者通吃”。
* **权重即发送增益**：EFX 没有 per-send gain，系统用该路发送的低通滤波器 `gain` 表示区域权重，用 `gain_hf` 复用遮挡的高频衰减。声源整体增益已经包含遮挡增益，所以墙后的混响尾巴既更轻、也更闷。
* **听者房间的二次激发**：声源不在听者所在区域时，额外把它的一小部分声音送进听者区域的混响槽（上限 `0.25 × 听者区域权重`），模拟穿过墙/门洞的声音激发室内混响。声源已经在听者区域内时不会重复添加。
* **湿声遮挡**：发送滤波器的高频与直达声使用同一个遮挡高频系数；墙后的洞穴混响是闷的，不会出现“直达声闷、混响亮”的割裂。
* **按需创建与回收**：只有被声源使用的区域才会创建 EFX 效果与效果槽；离开后释放，区域被删除时一并清理。区域创建时 context 会请求 4 路辅助发送，系统混响占用第 0..2 路。
* **2D 模式**：`Audio::Listener::use_2d = true` 时区域权重完全忽略 z，`size.z` 不参与判断；`size` 的 XY 才是有效范围。

注意：

* 设备不支持 EFX 时静默降级为无混响；
* 第 0..2 路发送由系统接管，脚本若手动给同一个 ECS 音源绑定这几路会被覆盖；剩余路数可用于自定义发送；
* 全局一次性播放不享受混响区域；
* 同一区域的效果参数变化时只需更新效果并重绑效果槽，不会重建整个区域。

### 门洞：`Audio::Portal`

`Audio::ReverbZone` 表示房间，`Audio::Portal` 表示房间之间的开口（门、窗、走廊接口）。二者都是普通 ECS 组件；音频系统只读取组件与实体变换，**不读取 tilemap**。tilemap 工具将来可以把房间/门洞生成为这些实体，但引擎的音频环境不依赖 tilemap。

| 成员 | 默认值 | 说明 |
| --- | --- | --- |
| `size` | (2, 2, 0.25) | 门洞局部全尺寸；x/y 为开口，z 为厚度（用于两侧采样） |
| `normal` | (0, 0, 1) | 局部穿透方向，随实体世界旋转 |
| `transmission` | 1 | 通过门洞的整体声能，0..1 |
| `muffle` | 0 | 通过门洞的高频损失，0..1 |
| `enabled` | true | false 时烘焙与实时环境图都忽略该门洞 |

系统沿门洞世界法线在中心两侧各取一个采样点，用房间 OBB 的权重自动解析两侧房间；两侧是同一房间或同为室外时忽略。作者只需把门洞放在开口处，并让 `normal` 指向穿透方向。没有门洞连接的房间之间仍会通过直接路径的遮挡衰减/闷音听到声音，但不会参与房间图路径。

### 音频环境图与烘焙

系统把房间与门洞构建成一张图：

* 节点 = 房间（`ReverbZone`）+ 室外；
* 边 = 门洞，权重为 `transmission` / `muffle`；
* 预计算所有房间对的路径：路径 transmission 为沿途乘积，muffle 为 `1 - Π(1 - muffle)`，并记录经过的门洞数。

运行时按声源/听者所在房间查路径表：

* 同房间：只发送该房间混响；
* 不同房间且有路径：源房间发送按路径 transmission 衰减、muffle 闷音，听者房间以 `0.25 × 听者权重 × 路径 transmission` 二次激发；
* 无路径（封闭墙）：保留直接遮挡 fallback，墙后的混响仍会随遮挡增益减弱。

**世界级烘焙 sidecar**：每个 `.je4world` 可以在同目录生成 `<world>.je4cache`，payload 是 binary cache document，其中 `audio_environment` 字段保存房间、门洞与路径表。sidecar 以世界文件 CRC64 为缓存键，世界保存后自动失效。运行时优先使用有效期内的 sidecar；sidecar 缺失/版本不符/`use_2d` 不一致时，从 `ReverbZone`/`Portal` 组件实时构建同样的环境，因此不烘焙也能正常工作。

烘焙入口：

* 世界视口菜单 **工具 → 烘焙音频环境**（会先保存世界）；
* 主菜单 **工具 → 烘焙当前世界音频环境**；
* **导出项目时自动对项目内全部 `.je4world` 烘焙**：已打开的世界先保存，未打开的世界临时加载、烘焙后关闭；sidecar 随项目一起打包。

烘焙是组件图计算（房间/门洞数量通常很少），同步执行；烘焙结果自包含，运行时不需要再用组件重建房间图。

### 效果器与滤波器

除了直达的声音，声源还可以经由*辅助发送*（send）把声音送进*效果槽*（effect slot），效果槽中装载一个*效果器*（effect）：

```
声源 ──直达声（可挂滤波器）─────────────────→ 听者
  └──辅助发送 0..N（每路可挂滤波器）→ 效果槽（装载效果器）→ 听者
```

一个效果槽可以被多个声源共享；一个声源可以同时使用多路发送。引擎创建 context 时最多请求 4 路（不请求时 openal-soft 只启用默认的 2 路）；系统混响占用第 0..2 路，脚本可以使用剩余路数。设备实际启用的路数可能少于请求值，超出路数的绑定不会生效。可用的效果器：

| 类别 | 效果器（Woolang 名） |
| --- | --- |
| 空间环境 | 混响 `reverb`、EAX 混响 `eaxreverb`、回声 `echo` |
| 调制 | 合唱 `chorus`、镶边 `flanger`、环形调制 `ring_modulator`、自动哇音 `autowah` |
| 音高与频率 | 移调 `pitch_shifter`、移频 `frequency_shifter`、人声变形 `vocal_morpher` |
| 音色与动态 | 失真 `distortion`、均衡 `equalizer`、压缩 `compressor` |

*滤波器*（filter）分为低通、高通、带通三种，参数为整体增益 `gain` 与低频/高频增益 `gain_lf` / `gain_hf`；它可以挂在直达声路径上，也可以挂在某一路辅助发送上。

⚠️ 修改效果器参数后，需要**重新把效果器绑定到效果槽**，新参数才会作用到已经绑定的效果槽与声源上。效果器与滤波器依赖设备的 EFX 扩展，设备不支持时引擎会给出警告，效果不生效。

### 视口 gizmo

音频实体在编辑器视口中使用程序化图标（无需图标资源）：

| 组件 | 图标 | 选中后的辅助线与手柄 |
| --- | --- | --- |
| `Audio::Source` | 音箱 + 声波 | 最大距离/参考距离圆环与距离手柄、朝向箭头 |
| `Audio::Listener` | 耳机 | 听者朝向箭头与朝向手柄 |
| `Audio::Occluder` | 砖墙 | 盒子线框与 ±X/±Y/±Z 六面尺寸手柄 |
| `Audio::ReverbZone` | 房间 + 混响弧 | 盒子线框、`fade` 外扩线框与尺寸/淡出距离手柄 |
| `Audio::Portal` | 门框 + 门扇 + 法线箭头 | 盒子线框、开口矩形、法线箭头与尺寸/法线手柄 |

* 工具栏的编辑层选择在选中音频实体时出现「音频」按钮（快捷键 `5`）；「全部」模式同时显示音频线框，选中实体显示亮色线框与手柄。
* 拖拽手柄直接写回组件成员：遮挡/房间/门洞的 `size`、房间的 `fade`、门洞的 `normal`、音源的 `max_distance`/`reference_distance`、听者的 `face`。按住 `Ctrl` 按 0.1 吸附。
* 一次拖拽在松开时发布为一条 `ModifyComponentMember` 撤销记录；数值的完整编辑仍可在 Inspector 中进行。
* 纯音频实体没有渲染形状，图标同时作为点击拾取目标，因此可以在视口内直接选中。

### 编辑器工作流

1. 新建世界时会自动添加 `Audio::AudioUpdatingSystem`；
2. “添加组件 → 音频”提供五个预设：**音源**（`Audio::Source`）、**音频监听器**（`Audio::Listener`）、**声音遮挡**（`Audio::Occluder`）、**混响区域**（`Audio::ReverbZone`）、**音频门洞**（`Audio::Portal`）；
3. 把 `Audio::Listener` 挂到摄像机实体，`use_2d` 保持默认 true 即可用于横板/俯视角；
4. 音源实体拖入 `.wav`，按需调整 `bus`、`reference_distance`、`max_distance`；
5. 在瓦片地图编辑器的瓦片面板中设置“音频遮挡/闷音”（这是独立的实时遮挡路径，不参与音频环境烘焙）；
6. 需要混响的房间/洞穴放一个 `Audio::ReverbZone`，调整尺寸覆盖房间；在 Inspector 里选一个预设（`room`/`cave`/`hallway` 等），按需调整 `fade` 与 `send_gain`。多个区域可以重叠，系统会按权重交叉淡化；
7. 在两个房间/室外之间的门口放 `Audio::Portal`，`size` 覆盖开口，`normal` 指向穿透方向；用 `transmission`/`muffle` 调整通过声能与高频损失；
8. 需要固定导出结果或避免运行时建图时：保存世界后使用 **世界视口 → 工具 → 烘焙音频环境**（或主菜单 **工具 → 烘焙当前世界音频环境**）。导出项目时会自动烘焙全部世界；
9. 不烘焙也能工作：sidecar 缺失时系统从 `ReverbZone`/`Portal` 组件实时构建同样的房间图。

旧版本的 `Audio::Playing` 已移除，旧世界中的音源需要把 `buffer`/`play`/`loop` 重新配置到 `Audio::Source` 上。

> 项目模板说明：`Audio::Listener::distance_model` 是一个脚本枚举，序列化/编辑器解析器位于
> `builtin/script/types.wo` 与 `builtin/script/editor.wo`。用旧模板创建、且没有
> `resource/editor/` 下这两个文件对应条目的工程，Inspector 会显示该成员缺少编辑器，
> 存档也不会写出该成员（读档时回落到默认的 `linear_clamped`）。把这两个文件里的
> `Audio::Listener::distance_model` 命名空间复制到工程的 `resource/editor/` 下即可。

### Woolang 示例

```woolang
import je::audio;

// 事件音：预加载后反复播放
let coin = je::audio::buffer::load("@/sounds/coin.wav")->unwrap;

UPDATE! {
    // 2D 位置播放（z=0，SE 总线）
    je::audio::play_once_at(coin,
        player_pos[0], player_pos[1]);
}

// 实体音源：读取组件并控制播放
system! bgm(src: Audio::Source)
{
    src.bus->set(je::audio::bus::BGM);
    src.volume->set(0.6);
    src.loop->set(true);
    src.play->set(true);
}

// 用角色的音源参数播放任意事件音
system! voice(src: Audio::Source, hurt: Audio::HurtFlag)
{
    let scream = je::audio::buffer::load("@/sounds/hurt.wav")->unwrap;
    if (hurt.value->get)
        do src->play_oneshot(scream);
}

// 总线音量
je::audio::bus::SE->set_gain(0.8);
```

不经过组件、直接用声源播放并挂混响/滤波：

```woolang
let buf = je::audio::buffer::load("@/sounds/bgm.wav")->unwrap;
let src = je::audio::source::create();
src->set_buffer(buf);
src->update(func(info){
    info.loop = true;
    info.gain = 0.5;
    info.reference_distance = 1.;
    info.max_distance = 32.;
    info.rolloff = 1.;
});
src->set_bus(je::audio::bus::BGM);
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

### C++ 示例

```cpp
using namespace jeecs;

auto buf = audio::buffer::load("@/sounds/hit.wav");      // 失败时为 nullopt
auto src = audio::source::create();
src->set_playing_buffer(buf.value());
src->update([](je_audio_source_t* s) {
    s->m_loop = false;
    s->m_reference_distance = 1.0f;
    s->m_max_distance = 32.0f;
    s->m_rolloff = 1.0f;
});
src->set_bus(JE_AUDIO_BUS_SE);
src->play();

// 一次性播放
je_audio_oneshot_desc_t desc;
desc.m_buffer = buf.value()->handle();
desc.m_bus = JE_AUDIO_BUS_SE;
desc.m_spatial = true;
desc.m_location[0] = 1.0f;
desc.m_location[1] = 2.0f;
desc.m_location[2] = 0.0f;
desc.m_gain = 1.0f;
desc.m_pitch = 1.0f;
desc.m_reference_distance = 1.0f;
desc.m_max_distance = 32.0f;
desc.m_rolloff = 1.0f;
desc.m_lowpass_hf = 1.0f;
audio::play_oneshot(desc);
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

⚠️ openal-soft 采用 LGPL 授权，**必须保持为动态库，禁止静态链接**——这是仓库的硬性约束（见 `AGENTS.md`），调整构建脚本时不要把它并入静态链接的依赖中。

### 镜像结构与引用计数

`je_audio_source_t`、`je_audio_listener_t`、`je_audio_filter_t`、`je_audio_effect_slot_t` 以及各效果器结构体都是 **CPU 侧的参数镜像**：修改字段后调用对应的 `je_audio_*_update` 才会提交给 OpenAL。C++ 的 `update(lambda)` 与 Woolang 的 `update(func)` 都是“取镜像→修改→提交”的封装。效果器在数据结构体之前额外分配一个 `EffectHead`（原生实例、效果种类、引用计数），`je_audio_effect_*` 接口以 `void*` 统一处理各类效果器。

`je_audio_source_t` 的空间参数包括：`reference_distance` / `max_distance` / `rolloff` / `min_gain` / `max_gain`、`relative`、锥形参数（`direction` / `cone_*`）与 `m_bus` 音量总线。距离模型与多普勒系数是**上下文级**状态，放在 `je_audio_listener_t` 上，由 `je_audio_listener_update` 通过 `alDistanceModel` / `alDopplerFactor` 提交。

缓冲、效果器、效果槽、滤波器都带引用计数：声源通过 `ContextHelper::Holder` 持有当前的缓冲、直达滤波器与每路发送的槽/滤波器，效果槽持有绑定的效果器。`close` 只减少引用计数，计数归零才真正释放，因此关闭仍被绑定的资源是安全的。

`je_audio_buffer_load` 还在 `Context` 内维护一个 **路径 + 缓存版本 → 缓冲区** 的弱引用缓存（`m_buffer_cache`，受 g2 保护，与其它缓冲操作同一把锁）：缓存版本是缓存头里的 crc64，标识 wav 与 `.je4audio` 的内容。路径与版本都命中时只增加引用计数并返回同一实例（不再读取缓存文档与 PCM），未命中才解码并登记；并发加载同一路径时，后完成的一方丢弃自己解码出的数据，共享先到的实例。缓存重建后 crc64 随之改变，新加载会得到新实例，不会复用旧数据。缓存本身不持有引用，最后一份引用释放（`close_buffer_lockfree`）时条目随之移除——既不需要像强引用池那样手动失效，也不会让已加载的音频常驻内存。`je_audio_buffer_create` 创建的匿名缓冲区不参与缓存。

### 音量总线（实现）

`Context` 内维护按 `je_audio_bus_t` 索引的进程级增益数组 `m_bus_gains`（`NONE` 槽位恒为 1.0）。增益的读写与使用该总线的声源增益重提交都由声源锁 g1 保护：`je_audio_bus_gain_set` 持 g1 遍历 `m_sources`，对 `m_bus` 匹配的声源重新提交；`update_source_lockfree` 提交 `AL_GAIN = m_gain × m_bus_gains[m_bus]`。总线不参与设备热插拔的 dump/restore。

### 一次性播放（实现）

`je_audio_oneshot_play` 复用普通声源：从 `m_sources` 创建一个临时 `je_audio_source_t`，设置缓冲、总线、空间参数与低通（如 `m_lowpass_hf < 1`），播放后加入 `Context::m_oneshot_voices` 队列。规则：

* 队列上限 32，超出时关闭最早创建的声部；
* 设备更新线程每 100ms 在补流式块之后回收已结束的声部（非流式看 `AL_SOURCE_STATE`，流式看 `m_should_play`）；
* 声部与普通声源一样参与设备热插拔的 dump/restore；
* 当前没有播放设备时直接返回 false，不创建声部；
* 队列本身由叶锁 `m_oneshot_mx` 保护，关源在该锁之外进行。

### 流式播放（实现）

`je_audio_buffer_load` 解析缓存文档后，`streaming` 为 true 的缓冲区不读入 PCM：`m_data` 为 `nullptr`，`m_stream_instance` 持有已打开的 `.je4cache` 与 PCM 区偏移，`m_size` 仍是 PCM 总字节数（时长信息不变）。`je_audio_buffer_create` 创建的缓冲区始终整段载入。

播放时每个声源维护一个 4 块的 OpenAL 缓冲队列（块大小 ≈ `byte_rate / 4`，按样本帧对齐）：

* 播放/跳转（游戏线程）与设备更新线程都从 PCM 区按偏移 `je_file_seek` + `je_file_read` 读块，`alBufferData` 后 `alSourceQueueBuffers` 入队；
* 设备更新线程每 100ms 检查所有流式声源：出队 `AL_BUFFERS_PROCESSED` 的块并累计已播字节，再补读后续块；队列空且未播完时 `alSourcePlay` 重新开始（欠载恢复），流自然播完时清除播放意图；
* 流式声源不使用 `AL_LOOPING`（openal-soft 对 looping source 不报告 `AL_BUFFERS_PROCESSED`、也禁止 unqueue），循环由读位置回绕实现；
* 播放进度 = 已出队字节 + `AL_BYTE_OFFSET`（openal-soft 的 offset 相对当前队列头），循环时对总长取模；跳转会停源、清空队列并从新偏移补块；
* `je_audio_source_play_state_get` 对流式声源返回逻辑状态（播放意图 / 暂停标记），补块间隔内的欠载不会让 `Audio::Source` 误判播放结束；
* 设备热插拔：转储时记录 `已出队字节 + AL_BYTE_OFFSET`，恢复时从该偏移重建队列；块缓冲名属于设备上下文，随转储释放、恢复时重新生成。

设备更新线程的定时唤醒使用独立的条件变量等待，唤醒后按 g2（缓冲区）→ g1（声源）→ g0（上下文，共享）的锁序补块、回收一次性声部并检查设备连接，避免与其它音频线程形成锁序反转。

### Context：设备、上下文与热插拔

后端只有一个全局 `Context`。它枚举播放设备（支持 `ALC_ENUMERATE_ALL_EXT` 时枚举全部设备），并为当前设备创建 OpenAL 上下文；设备支持 `ALC_EXT_EFX` 且最大辅助发送数大于 0 时，通过 `alcGetProcAddress` 加载 EFX 函数，否则关闭 EFX 并告警。创建上下文时会请求最多 4 路辅助发送（`ALC_MAX_AUXILIARY_SENDS`；不请求时 openal-soft 只启用默认的 2 路），并在支持时允许后端自动选择 HRTF（`ALC_SOFT_HRTF` + `ALC_DONT_CARE_SOFT`）；实际生效的发送路数从设备上下文查询后记入 `m_active_auxiliary_sends`，日志同时记录 HRTF 状态。

后台线程每 100ms 检查一次（同时为流式声源补块）：当前设备断开，或者正在使用的默认设备不再是系统默认设备时，重新枚举并切换到新的默认设备。切换流程：

1. `dump_and_close_playing_states`：为每个声源转储播放状态与字节偏移，删除所有 AL 对象；
2. 销毁旧上下文，在新设备上创建上下文；
3. `restore_playing_states`：重建 AL 对象，恢复缓冲、播放状态、播放进度、滤波器与各路发送；
4. 在新上下文上重新提交听者状态（距离模型、多普勒、朝向与增益）。

`je_audio_devices_disconnect_all` 复用同一机制，供移动平台切到后台时彻底停止音频；恢复时用 `je_audio_devices_enumerate_and_do` 重新枚举即可。

提交位置、速度、朝向、锥形方向时，z 分量被取反——引擎坐标系与 OpenAL 坐标系的 z 轴方向相反。

### AudioUpdatingSystem：组件到声源

音频系统只实现 `CommitUpdate`（第 7 阶段，晚于 `LATE_UPDATE!`），因此脚本在同一帧内对组件的修改当帧生效。处理顺序：

1. **Listener + Translation**：速度 = 位置差 / `max(deltatime, 0.0001)`（首帧不产生速度尖峰）；`use_2d` 时位置/速度的 z 归零；`face` / `up` 乘以世界旋转；提交位置、速度、朝向、`m_gain = volume`、距离模型与多普勒系数（不触碰 `m_global_gain`）。系统**不查询 Camera 组件**。
2. **音频环境**：收集 `Audio::ReverbZone + Translation`（房间）与 `Audio::Portal + Translation`（门洞，跳过 `enabled = false`）的组件快照；`World::Path` 组件的 `cache_path` 变化或收到显式重载请求时读取缓存中的 `audio_environment`。缓存有效且 `use_2d` 与当前 Listener 一致时使用烘焙图，否则用组件快照实时构建同一张图（输入签名变化时才重建）。图包含房间 OBB、门洞连接和全房间对路径表；区域效果槽不在这里创建，而是等声源真正用到时按需创建。
3. **遮挡快照**：一次性收集所有 `Audio::Occluder + Translation`；对每个 `Tilemap::Map + Translation` 通过 `Tilemap::documents::observe_map` 取得文档并 `ensure_tilesets`，本帧持有 `shared_ptr` 保活。
4. **Source + Translation（可选）**：
   * `spatial = false` 或没有 Translation → relative，位置/速度写 0；
   * 2D 听者下把声源位置/速度的 z 归零；
   * 对每个空间化声源做线段测试：Occluder 的 2D 矩形/3D OBB + 瓦片地图 DDA，得到 `occlusion.m_gain` 与 `occlusion.m_hf`；
   * `m_gain = volume × occlusion.m_gain`；`occlusion.m_hf < 1` 时按需创建/更新 per-source 低通滤波器并绑定（`AL_DIRECT_FILTER`），恢复时解绑；
   * `bus` 变化时经 `je_audio_source_bus_set` 提交；
   * **混响发送**：用环境房间权重取声源所在的前两个房间；同房间直接发送，不同房间按路径表的 `transmission`/`muffle` 调整发送权重与高频，并给听者房间加入二次激发（最多 3 路）。每路发送使用独立的低通滤波器（`gain = 发送权重`、`gain_hf = 遮挡 hf × 路径 muffle`），绑定到该房间的 EAX 效果槽。房间效果参数变化时更新效果并重绑效果槽；发送的房间或槽 epoch 变化时才重新绑定发送；
   * 最后提交空间参数（位置、速度、方向、锥形、空气吸收、min/max gain）并驱动播放状态机。

播放状态机（`Audio::Source::play_state` 只有 `STOPPED` / `PLAYING`）：

| 当前状态 | 条件 | 动作 |
| --- | --- | --- |
| 任意 | `buffer` 为空 | 停止声源，`state = STOPPED`（不修改 `play`） |
| `STOPPED` | `play` 为 true | 开始播放，进入 `PLAYING` |
| 播放中 | 声源已停止且缓冲未变 | 播放自然结束：`state = STOPPED`、`play = false` |
| 播放中 | 缓冲刚被更换（底层已自动停止） | 若 `play` 为 true 立即重新播放 |
| 播放中 | 声源暂停 / 播放中 | 按 `play` 恢复或暂停 |

`loop` 每帧写入镜像，因此运行中切换循环立即生效。缓冲的绑定每帧都会调用 `set_playing_buffer`，底层只有在缓冲真正改变时才停止播放并返回 true。`Audio::Source` 的拷贝构造创建新的 AL 声源，所以由预制体实例化出的实体都拥有独立的播放状态；运行时状态（上次位置、遮挡滤波器、总线缓存、混响发送槽等）不反射、不存档。

### 音频环境烘焙（世界级 sidecar）

`<world>.je4cache` 使用 `WORLD_CACHE_VERSION`，payload 是 binary cache document，`audio_environment` 字段包含：

* `format`（sidecar 内部 schema 版本）、`use_2d`；
* `rooms`：每个房间的 OBB、`fade`、EAX 预设/自定义参数、`send_gain`；
* `portals`：解析后的两侧房间、中心/法线、`transmission`/`muffle`；
* `paths`：`(rooms + 1)^2` 的路径表（transmission、muffle、门洞数），室外节点在末尾。

烘焙入口 `je_audio_environment_bake(world, world_path)`：

1. 遍历世界实体（未启用/未 update 的临时世界先处理命令缓冲区），收集 `ReverbZone`/`Portal`；
2. 从 `Anchor` / `LocalToParent` / `LocalToWorld` / 局部变换组件解析实体世界变换（不依赖 `Translation` 是否已更新，因此未启用/未 update 的临时世界也能烘焙）；
3. 用与运行时相同的 builder 解析门洞两侧房间并计算全房间对路径；
4. 以世界文件 CRC64 为键写 `<world>.je4cache`，并通知音频系统重载（`je_audio_environment_reload`）。

缓存头 CRC 是世界文件本身的 CRC，世界保存后缓存自动失效。缓存路径是世界级派生数据，由持有 `World::Path` 组件的内部实体（`cache_path`，配对 `Editor::Invisible`，不显示、不随世界存档）提供：加载世界时按路径创建该实体，编辑器手动烘焙时也会创建；运行时 `AudioUpdatingSystem` 按该实体加载缓存，实体不存在、缓存缺失/无效或 `use_2d` 不匹配时直接走组件实时构建。缓存自包含，运行时不需要再用 `ReverbZone`/`Portal` 组件重建图；需要动态修改房间/门洞时重新烘焙，或删除缓存使用实时图。

### 瓦片遮挡遍历

瓦片的音频系数是 `TileDef` 的内置字段（`audio_occlusion` / `audio_muffle`，默认 0），与 `walkable` 一样随图集缓存（`.je4tileset` TOML → `.je4cache`，缓存版本 v2）保存；图集 TOML 的 `tiles` 条目新增 `audio_occlusion` / `audio_muffle` 两个可选浮点键。

`AudioUpdatingSystem` 用地图根实体的 `world_position` / `world_rotation` / `local_scale` 把世界坐标转到地图局部网格：`gx = local.x`、`gy = -local.y`（地图格 y 向下，1 格 = 1 单位）。系统按 `map_stamp`（地图与图集版本合并戳）把整张地图的系数平整化成两张 `width*height` 浮点网格并缓存：同一格内所有图层取最大值，空格为 0；只有地图/图集版本变化时才重建，编辑态下随编辑重建，运行时不再逐格查询瓦片元数据。射线遍历用 Amanatides-Woo DDA（线段先裁剪到地图矩形），跳过起点格，每格直接索引缓存网格并累乘。瓦片遮挡在 2D 与 3D 模式下都按 XY 平面计算。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 音频组件（Source/Listener/Occluder/ReverbZone/Portal） | `include/jeecs/components.hpp`（`namespace Audio`） |
| 世界级缓存路径组件（World::Path） | `include/jeecs/components.hpp`（`namespace World`） |
| C ABI、效果器参数与默认值、C++ 封装 | `include/jeecs/audio.hpp`（逐函数说明见 [doc/capi/audio](../capi/audio/)） |
| OpenAL 后端（设备、热插拔、EFX、总线、一次性声部、缓存加载） | `src/jeecs_audio_impl_openal.cpp` |
| 缓存生成器脚本（pkg::audio 解码） | `builtin/script/internal/cache/audio.wo` |
| 组件同步、2D 空间、遮挡、混响与播放状态机 | `src/jeecs_core_audio_system.cpp` |
| 音频环境图、路径表、`.je4world.je4cache` 读写 | `src/jeecs_core_audio_environment.hpp/.cpp` |
| 缓存组件维护、编辑器手动烘焙/导出烘焙 | `builtin/editor/Editor/WorldEditorViewport/WorldSaving.wo`、`builtin/editor/Editor/ExportProject.wo` |
| 加载世界时创建 World::Path 实体 | `builtin/gaming/Loading/World.wo` |
| 瓦片属性解析与遮挡遍历支持 | `src/jeecs_core_tilemap_model.hpp/.cpp` |
| Woolang 绑定 | `je/audio.wo`、`src/jeecs_audio_woolang_api.cpp` |
| 组件辅助函数（towoo） | `je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| 打开/生成音频缓存 | `je/cache.wo`（`generate_audio_cache`） |
| openal-soft 构建配置 | `CMakeLists.txt`（`JE4_SPECIFY_OPENAL_LIB_NAME`）、`3rd/openal-soft` |
