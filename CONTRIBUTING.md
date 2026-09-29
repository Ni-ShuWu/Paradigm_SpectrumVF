# 贡献指南

感谢你愿意改进《范式：起源》社区同人制谱器。本项目与击弦网络及《范式：起源》官方无任何关联，提交内容时请不要包含官方游戏资源、音乐或未授权素材。

## 开发环境

- Qt 6.5 或更新版本（Widgets 组件）
- C++17 编译器（MinGW 或 MSVC）
- CMake 3.21+ 与 Ninja

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM="E:\Qt\Tools\Ninja\ninja.exe" `
  -DCMAKE_PREFIX_PATH="E:\Qt\6.11.2\mingw_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

## 提交改动

1. 从 `main` 创建一个主题分支。
2. 保持改动聚焦，一次 PR 只解决一个问题。
3. 修改谱面读写逻辑时，同步更新 `tests/chart_tests.cpp`，保证新旧 JSON 格式都有覆盖。
4. 提交前运行：

   ```powershell
   cmake --build build
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

5. 打开拉取请求，说明改动动机、实现方式和验证结果。

## 代码约定

- 使用 Qt 命名风格：类名大驼峰、成员变量 `m_` 前缀。
- 新增字符串面向使用者时使用简体中文。
- 不引入与改动无关的重构或格式化调整。
- 许可为 GPL-3.0-only，贡献的代码按相同许可授权。
- 提交代码并发起合并请求时，应当在请求中附上你使用的AI编程代理或编辑工具、模型（可选）、及其当前系统（例如：opencode、kimik3、win）
  - 本项目拒绝使用移动端套壳或运行在移动端上的AI编程代理

## 行为准则

参与本项目即表示你同意遵守 [行为准则](CODE_OF_CONDUCT.md)。
