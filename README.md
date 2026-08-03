# ZTF - 同优计算优化设计软件

油浸式变压器电磁计算（长方形线圈非晶合金铁心）导线优化设计软件。

## 技术栈

- C++17 / Qt 6 Widgets
- CMake 构建
- 自定义 Ribbon 工具栏组件
- Material Design Rounded 风格 SVG 图标

## 架构

```
GUI (Qt Widgets) → Core (数据模型/状态) → Engine (计算接口)
                                         → Adapter (LLM 接口, 预留)
```

依赖关系: GUI 依赖 Core/Engine/Adapter; Engine 依赖 Core; Adapter 独立。

## 项目结构

```
qt-app/
├── CMakeLists.txt                      # CMake 构建配置（Qt6, C++17, Ninja）
├── app.rc                              # Windows 资源文件，嵌入 exe 图标
├── src/
│   ├── main.cpp                        # 程序入口，加载 QSS 样式表，设置窗口图标
│   │
│   ├── core/                           # 数据模型层
│   │   ├── TransformerParams.h         # 变压器输入参数结构体（容量/电压/损耗等23个字段）
│   │   ├── StructureConfig.h           # 结构配置枚举与字段（铁芯/绕组/工艺等29项）
│   │   ├── OptimizationResult.h        # 优化方案结果结构体（19列数据）
│   │   ├── PrintOutputData.h/cpp       # 输出打印双列数据（38行Mock数据）
│   │   └── UserStore.h/cpp             # 用户认证存储（硬编码 admin/user 账号）
│   │
│   ├── engine/                         # 计算引擎层
│   │   ├── ICalcEngine.h               # 计算引擎纯虚接口（calculate 方法）
│   │   ├── IOptimizer.h                # 优化器纯虚接口（start/pause/stop + Qt信号）
│   │   ├── MockCalcEngine.h/cpp        # Mock 计算引擎实现（返回默认数据）
│   │   └── MockOptimizer.h/cpp         # Mock 优化器（定时生成31个随机方案）
│   │
│   ├── adapter/                        # LLM 适配层（预留接口）
│   │   ├── IModelAdapter.h             # LLM 适配器接口（suggestParameters/evaluateScheme）
│   │   └── NullModelAdapter.h          # 空实现占位
│   │
│   └── gui/                            # 界面层
│       ├── MainWindow.h/cpp            # 主窗口，管理 QStackedWidget 页面导航
│       │
│       ├── pages/
│       │   ├── LoginPage.h/cpp         # 登录页（渐变背景/白色卡片/记住密码）
│       │   ├── MainDashboardPage.h/cpp # 主界面（导航图标/工具栏/操作按钮）
│       │   ├── OptimizeCalcPage.h/cpp  # 参数设置页（6组Ribbon选型/参数表/侧栏）
│       │   └── EnterCalcPage.h/cpp     # 计算主页（3个Tab: 优化计算/方案选择/输出打印）
│       │
│       └── widgets/
│           ├── RibbonBar.h/cpp         # Ribbon工具栏容器（蓝色渐变背景）
│           ├── RibbonGroup.h/cpp       # Ribbon分组（带标题/互斥单选模式）
│           ├── RibbonButton.h/cpp      # Ribbon按钮（图标+文字/可选中/可取消）
│           ├── ParamTableWidget.h/cpp  # 参数编辑表格（23行输入参数）
│           ├── SchemeTableWidget.h/cpp # 方案结果表格（19列优化结果）
│           ├── PrintTableWidget.h/cpp  # 输出打印表格（双列38行数据）
│           └── SidebarPanel.h/cpp      # 侧栏面板（带图标的垂直按钮组）
│
└── resources/
    ├── resources.qrc                   # Qt 资源文件索引
    ├── app_icon.ico                    # Windows exe 嵌入图标（多尺寸）
    ├── styles/
    │   └── ztf_theme.qss              # 全局样式表（Ribbon/表格/对话框等）
    └── icons/                          # Material Design Rounded SVG 图标
        ├── app_icon.svg                # 应用图标（变压器铁芯+线圈）
        ├── *_dark.svg                  # 深色版图标（用于浅色背景）
        └── *.svg                       # 白色图标（用于深色Ribbon/侧栏背景）
```

## 功能模块

- 登录认证（支持记住密码）
- 主界面导航（优化设计 / 产品报价 / 程序工具 / 数据查询）
- 参数设置页（Ribbon工具栏互斥选型 + 可编辑参数表 + 侧栏操作）
- 计算主页（三Tab切换：优化计算 / 方案选择 / 输出打印）
- 选型验证（所有分组必须有选中项才能进入计算）

## 编译运行

需要：
- Qt 6.5+（推荐 6.11）
- CMake 3.21+
- MinGW 13+ 或 MSVC

```bash
cd qt-app

# 配置（根据你的 Qt 安装路径调整）
cmake -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=D:/QT/6.11.1/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=D:/QT/Tools/Ninja/ninja.exe

# 编译
cmake --build build

# 部署 DLL（首次运行或添加新 Qt 模块后执行）
D:/QT/6.11.1/mingw_64/bin/windeployqt.exe build/ZTF-Designer.exe

# 运行
build/ZTF-Designer.exe
```

## 默认账号

| 用户名 | 密码 |
|--------|------|
| admin  | 123456 |
| user   | 123456 |

## 后续计划

- 实现真实电磁计算引擎（ICalcEngine / IOptimizer 接口）
- 接入远程 LLM API 辅助参数推荐（IModelAdapter 接口）
- 数据库支持（SQLite，用户管理/历史方案存储）
