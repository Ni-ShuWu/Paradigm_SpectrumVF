# 范式：起源 · 制谱器

基于 **C++17 和 Qt 6 Widgets** 的社区同人制谱器。采用三栏深色编辑界面，提供五轨时间轴、可调 BPM / 每拍分音 / 每小节拍数，以及随曲包音乐进度同步的透视轨道式实时预览。可在判定面放置并拖动区内音符，也可新建包含音乐、曲绘和 JSON 谱面的独立曲包。预览是 Qt 绘制的 2.5D 编辑器可视化，不模拟游戏运行时或 Unity 渲染。

## 免责声明

本项目是社区同人制作的独立工具，仅供学习与交流使用，与击弦网络及《范式：起源》官方不存在任何隶属、合作、授权、赞助或其他关联；本项目亦未经官方认可、维护或背书。

《范式：起源》及其名称、标识、游戏内容与相关权利归各自权利人所有。本项目不提供原游戏资源、音乐或其他官方素材。第三方游戏名称仅用于说明本工具的用途，并不表示与相应权利人存在关联。使用者应自行确认其创建、导入、分享的谱面、素材及其他内容拥有必要的权利，并遵守适用的法律、服务条款和游戏规则。请勿将本项目或其生成的文件误认为官方产品，亦请勿利用本项目侵犯任何人的合法权益。

本项目按“现状”提供，不附带任何明示或默示保证。作者及贡献者不对因使用、无法使用本项目，或使用者创建、分享的内容而产生的损害、纠纷或责任承担保证或责任；适用法律不允许排除或限制的责任除外。

## 开源许可

本项目依据 **GNU General Public License v3.0（GPL-3.0-only）** 发布，完整许可文本见 [`LICENSE`](LICENSE)。修改、再发布或分发本项目时，请遵守许可证条款。项目开源许可不代表相关游戏、商标或任何第三方内容的权利归本项目或其贡献者所有，也不授予使用这些内容的额外许可。

## 构建

需要 Qt 6.5 或更新版本（Widgets）、C++17 编译器和 CMake 3.21+。Windows 上请使用与 Qt 套件匹配的 MinGW 或 MSVC。

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM="E:\Qt\Tools\Ninja\ninja.exe" `
  -DCMAKE_PREFIX_PATH="E:\Qt\6.11.2\mingw_64" `
  -DCMAKE_C_COMPILER="E:\Qt\Tools\mingw1310_64\bin\gcc.exe" `
  -DCMAKE_CXX_COMPILER="E:\Qt\Tools\mingw1310_64\bin\g++.exe"
cmake --build build
ctest --test-dir build --output-on-failure
```

将示例路径替换为本机 Qt 和 MinGW 安装位置。程序位于 `build\ParadigmOriginScoreEditor.exe`。部署运行时可使用 Qt 套件自带的 `windeployqt`。

## 使用方式

- 点击“新建曲包”，输入曲包名称，依次选择 MP3/OGG 音乐、JPG/PNG 曲绘和保存位置。应用会复制素材并在新建目录中写入同名 JSON；目录已存在时不会覆盖。
- 时间轴包含 0、1、2、3 和判定区五条固定轨道。选择“放置音符”后点击轨道与分音格即可创建音符；轨道 0–3 对应四边音符，判定区轨道对应区内音符。右侧宽幅透视赛道预览展示轨道结构、曲名、BPM 和谱面进度；其中 12×9 判定面可放置区内音符，也可在“选取 / 移动”模式下拖动已有区内音符调整坐标，判面下方向对应坐标 Y=0。播放音乐时预览音符从远处驶向判定面，并按当前 BPM 与每拍分音数同步位置；播放时编辑锁定，暂停或停止后可编辑。该场景是 Qt 绘制的 2.5D 编辑器预览，不是游戏运行时的 3D 渲染。
- 可分别设置 BPM、每拍分音数和每小节拍数；节拍线及小节线按设置更新，每个分音格的 `tick` 加 1。音符列表支持选取、复制和删除；“选取 / 移动”可拖动音符；属性栏可设置 `kind`、`tick` 和假音符。
- 右侧播放栏可试听曲包音乐、拖动进度；当前播放位置按 `floor(毫秒 × BPM × 每拍分音数 / 60000)` 映射到 Tick，并在预览中高亮对应音符。音频起点视为 Tick 0；播放依赖 Qt Multimedia 的系统音频后端。
- 支持打开 JSON、保存和保存修改。快捷键：`Ctrl+O` 打开、`Ctrl+S` 保存、`Delete` 删除、`Ctrl+D` 复制。

## JSON 结构

时间轴有 0、1、2、3、判定区五条轨道：前四条映射到 `EdgeNote.edge`（左、右、上、下），边线位置固定在对应边的中点；判定区映射到坐标 (6, 4.5) 的 `SpaceNote`。`bpm` 为 1～1000 的整数，`subdivision` 为每拍分音数（1～64），`beatsPerMeasure` 为每小节拍数（1～32）；每个分音格对应一个整数 `tick`。未包含时序字段的旧谱默认使用 BPM 120、每拍 4 分音、每小节 4 拍。`isFake` 代表不参与判定的假音符。曲包谱面可选包含相对路径 `assets.music` 和 `assets.jacket`。

```json
{
  "format": "ParadigmOriginChart",
  "version": 1,
  "title": "我的谱面",
  "bpm": 120,
  "subdivision": 4,
  "beatsPerMeasure": 4,
  "assets": { "music": "music.ogg", "jacket": "jp.png" },
  "notes": [
    { "type": "EdgeNote", "kind": "tap", "edge": 0, "pos": 4.5, "tick": 0, "isFake": false },
    { "type": "SpaceNote", "kind": "tap", "x": 6.0, "y": 4.5, "tick": 48, "isFake": false }
  ]
}
```

程序兼容旧版 JSON 谱面。附带的逆向分析文档没有提供 `kind` 的完整枚举、`tick` 与音乐时间的换算，或可确认的完整 ParSa 输入语法。因此，本工具使用有版本的 JSON 保存已知音符数据，不将其宣称为可直接导入游戏的原版谱面文件。

## 测试

```powershell
ctest --test-dir build --output-on-failure
```
