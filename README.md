# ZTF - 同优计算优化设计软件

油浸式变压器电磁计算（长方形线圈非晶合金铁心）导线优化设计软件。

## 技术栈

- C++17 / Qt 6 Widgets
- CMake 构建
- 自定义 Ribbon 工具栏组件

## 架构

```
qt-app/src/
├── core/       数据模型（变压器参数、结构配置、优化结果）
├── engine/     计算引擎接口与 Mock 实现
├── adapter/    LLM 适配层接口（预留）
└── gui/        界面层（页面、Ribbon组件、表格控件）
```

## 功能模块

- 登录认证（支持记住密码）
- 主界面导航（优化设计 / 产品报价 / 程序工具 / 数据查询）
- 参数设置页（Ribbon工具栏 + 可编辑参数表 + 侧栏操作）
- 计算主页（三Tab切换：优化计算 / 方案选择 / 输出打印）

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

# 部署 DLL
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
