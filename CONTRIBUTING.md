# ZTF 协作开发手册

## 项目概述

油浸式变压器电磁计算（长方形线圈非晶合金铁心）导线优化设计软件。

- 技术栈：C++17 / Qt 6.11 / CMake / MinGW
- 仓库地址：https://github.com/2646681657/zt-transformer-ai

---

## 环境准备

### 必需工具

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| Qt | 6.5+（推荐 6.11） | GUI 框架 |
| CMake | 3.21+ | 构建系统 |
| MinGW | 13+ 或 MSVC | C++ 编译器 |
| Git | 2.30+ | 版本控制 |
| Ninja | 任意 | 构建加速（可选） |

### 首次配置

```bash
git clone https://github.com/2646681657/zt-transformer-ai.git
cd zt-transformer-ai/qt-app

# 配置（根据你的 Qt 安装路径调整）
cmake -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=D:/QT/6.11.1/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=D:/QT/Tools/Ninja/ninja.exe

# 编译
cmake --build build

# 部署 Qt DLL（首次编译后执行一次）
D:/QT/6.11.1/mingw_64/bin/windeployqt.exe build/ZTF-Designer.exe

# 运行
build/ZTF-Designer.exe
```

---

## 分支管理

### 分支结构

```
main ← 稳定发布版本，仅通过 PR 合并
 └── dev ← 日常开发主线
      ├── feature/xxx ← 新功能分支
      ├── fix/xxx     ← Bug 修复分支
      └── refactor/xxx ← 重构分支
```

### 分支命名规范

| 类型 | 格式 | 示例 |
|------|------|------|
| 新功能 | `feature/简短描述` | `feature/calc-engine` |
| Bug 修复 | `fix/简短描述` | `fix/ribbon-crash` |
| 重构 | `refactor/简短描述` | `refactor/param-table` |
| 文档 | `docs/简短描述` | `docs/api-comments` |

### 规则

- **禁止**直接 push 到 `main` 分支
- 所有改动从 `dev` 创建功能分支，完成后通过 PR 合并回 `dev`
- `dev` 稳定后由负责人合并到 `main` 发布

---

## 日常开发流程

### 1. 开始工作

```bash
git checkout dev
git pull origin dev
git checkout -b feature/你的功能名
```

### 2. 开发提交

```bash
git add 具体文件名    # 不要用 git add .
git commit -m "类型: 简短描述"
```

### 3. 推送与合并

```bash
git push -u origin feature/你的功能名
```

在 GitHub 创建 Pull Request，目标分支选 `dev`，等待 review 后合并。

### 4. 合并后清理

```bash
git checkout dev
git pull origin dev
git branch -d feature/你的功能名
```

---

## 提交规范

### 格式

```
类型: 简短描述（不超过 50 字）

可选的详细说明（说明为什么改，而非改了什么）
```

### 类型关键词

| 类型 | 含义 |
|------|------|
| feat | 新功能 |
| fix | Bug 修复 |
| refactor | 重构（不改变功能） |
| style | 代码格式调整（不影响逻辑） |
| docs | 文档变更 |
| build | 构建/依赖变更 |
| test | 测试相关 |

### 示例

```
feat: 实现真实电磁计算引擎
fix: 修复 Ribbon 按钮互斥逻辑在快速点击时失效
refactor: 提取参数校验为独立方法
```

---

## 代码规范

### C++ 命名

| 类别 | 风格 | 示例 |
|------|------|------|
| 类名 | PascalCase | `RibbonButton` |
| 成员变量 | m_camelCase | `m_paramTable` |
| 局部变量 | camelCase | `titleLabel` |
| 方法/函数 | camelCase | `setupRibbon()` |
| 常量 | UPPER_SNAKE | `MAX_RETRY_COUNT` |
| 枚举值 | PascalCase | `CoreType::Amorphous` |

### 文件命名

- 头文件/源文件与类名一致：`RibbonButton.h` / `RibbonButton.cpp`
- 每个类单独一对 h/cpp 文件
- 头文件使用 `#ifndef` 防重复包含

### 代码风格

- 缩进：4 空格（不用 Tab）
- 大括号：开括号不换行
- 指针/引用：`*` 和 `&` 靠左（`auto *widget`）
- Qt 字符串：中文使用 `QStringLiteral("中文")`
- 信号槽：使用函数指针语法 `connect(obj, &Class::signal, ...)`

### 注释要求

- 每个类写一行头部注释说明用途
- 重要公有方法写简短注释说明功能
- 不写显而易见的注释（如 `// 设置布局`）
- 非显而易见的实现逻辑写注释解释**为什么**

---

## 项目目录结构

```
qt-app/
├── src/
│   ├── main.cpp          # 入口
│   ├── core/             # 数据模型（纯数据结构，不依赖 Qt GUI）
│   ├── engine/           # 计算引擎（接口 + 实现）
│   ├── adapter/          # LLM 适配层（预留）
│   └── gui/
│       ├── MainWindow.*  # 主窗口/页面导航
│       ├── pages/        # 各个页面
│       └── widgets/      # 可复用组件
├── resources/
│   ├── icons/            # SVG 图标
│   ├── styles/           # QSS 样式表
│   └── resources.qrc     # 资源索引
└── CMakeLists.txt
```

### 层级依赖

```
GUI → Core + Engine + Adapter
Engine → Core
Adapter → 独立（不依赖其他层）
```

新增代码必须遵守此依赖方向，禁止反向依赖。

---

## Pull Request 规范

### PR 标题

简短描述改动内容，不超过 70 字。

### PR 描述模板

```markdown
## 改动内容
- 简述改了什么（1-3 条）

## 为什么改
简述动机或关联的需求

## 测试情况
- [ ] 编译通过
- [ ] 手动测试了主流程
- [ ] 没有引入新的 warning
```

### Review 要求

- 至少 1 人 review 后才能合并
- 有冲突必须本地解决后再 push
- CI 编译失败不能合并

---

## 冲突解决

```bash
# 功能分支落后时，拉取最新 dev 合并
git checkout feature/xxx
git fetch origin
git merge origin/dev

# 手动解决冲突后
git add 冲突文件
git commit -m "fix: 解决与 dev 的合并冲突"
git push
```

优先使用 merge 而非 rebase，保留完整历史。

---

## 常见问题

### 编译报错 Permission denied

旧进程未关闭，先关闭程序：
```bash
taskkill //F //IM ZTF-Designer.exe
cmake --build build
```

### 缺少 DLL

重新部署 Qt 依赖：
```bash
D:/QT/6.11.1/mingw_64/bin/windeployqt.exe build/ZTF-Designer.exe
```

### 新增 Qt 模块后编译失败

在 `CMakeLists.txt` 的 `find_package(Qt6 REQUIRED COMPONENTS ...)` 中添加新模块名，然后重新 configure：
```bash
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=D:/QT/6.11.1/mingw_64
cmake --build build
```
