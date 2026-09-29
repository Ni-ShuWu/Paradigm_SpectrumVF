# 范式：起源 · 制谱器

使用 12×9 判面编辑《范式：起源》边线音符（`EdgeNote`）和区内音符（`SpaceNote`）的 Windows 桌面工具。标准 Python 安装附带 Tkinter；制谱器本身不需要第三方运行库。

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
