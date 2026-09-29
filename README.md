# 范式：起源 · 制谱器

使用 12×9 判面编辑《范式：起源》边线音符（`EdgeNote`）和区内音符（`SpaceNote`）的 Windows 桌面工具。标准 Python 安装附带 Tkinter；制谱器本身不需要第三方运行库。

## 免责声明

本项目是社区同人制作的独立工具，仅供学习与交流使用，与击弦网络及《范式：起源》官方不存在任何隶属、合作、授权、赞助或其他关联；本项目亦未经官方认可、维护或背书。

《范式：起源》及其名称、标识、游戏内容与相关权利归各自权利人所有。本项目不提供原游戏资源、音乐或其他官方素材。第三方游戏名称仅用于说明本工具的用途，并不表示与相应权利人存在关联。使用者应自行确认其创建、导入、分享的谱面、素材及其他内容拥有必要的权利，并遵守适用的法律、服务条款和游戏规则。请勿将本项目或其生成的文件误认为官方产品，亦请勿利用本项目侵犯任何人的合法权益。

本项目按“现状”提供，不附带任何明示或默示保证。作者及贡献者不对因使用、无法使用本项目，或使用者创建、分享的内容而产生的损害、纠纷或责任承担保证或责任；适用法律不允许排除或限制的责任除外。

## 开源许可

本项目依据 **GNU General Public License v3.0（GPL-3.0-only）** 发布，完整许可文本见 [`LICENSE`](LICENSE)。修改、再发布或分发本项目时，请遵守许可证条款。项目开源许可不代表相关游戏、商标或任何第三方内容的权利归本项目或其贡献者所有，也不授予使用这些内容的额外许可。

## 运行与打包

- 源码运行：`python main.py`
- 在 Windows 上生成单文件、无控制台窗口的可执行程序：

  ```powershell
  python -m pip install pyinstaller
  python -m PyInstaller --noconfirm --clean --onefile --windowed --name ParadigmOriginScoreEditor main.py
  ```

- 可执行文件生成于 `dist\ParadigmOriginScoreEditor.exe`。`.exe` 不包含或依赖 Python 源码文件，也无须安装 Python。

## 使用方式

- 选择“判面音符”或“边线音符”，再点击画布放置；边线工具可以选择左、右、上或下边线。
- 在属性栏设置 `kind`、整数 `tick` 和“假音符”。点击“选取 / 移动”，可选取画布音符并拖动位置；用“应用属性”修改选中音符。列表也可以选取或删除音符。
- JSON 文件支持新建、打开和另存为。快捷键：`Ctrl+O` 打开、`Ctrl+S` 保存、`Delete` 删除、`Ctrl+D` 复制。

## JSON 结构

坐标采用判面 12×9 的游戏坐标；`EdgeNote.edge` 沿用分析文档中的编号（0 左、1 右、2 上、3 下），`pos` 是沿对应边的位置。`SpaceNote.x` 范围为 0～12，`y` 范围为 0～9。`tick` 以整数原样保存，`isFake` 代表不参与判定的假音符。

```json
{
  "format": "ParadigmOriginChart",
  "version": 1,
  "title": "我的谱面",
  "notes": [
    { "type": "EdgeNote", "kind": "tap", "edge": 0, "pos": 4.5, "tick": 0, "isFake": false },
    { "type": "SpaceNote", "kind": "tap", "x": 6.0, "y": 4.5, "tick": 48, "isFake": false }
  ]
}
```

附带的逆向分析文档没有提供 `kind` 的完整枚举、`tick` 与音乐时间的换算，或可确认的完整 ParSa 输入语法。因此，制谱器使用有版本的 JSON 保存文档已确认的音符几何和原始 tick 值，不将 JSON 宣称为可直接导入游戏的原版谱面文件。

## 测试

`python -m unittest discover -s tests -v`
