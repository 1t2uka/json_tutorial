# leptpjson:极简c语言json解释器
——基于json-tutorial

## 项目简介
`leptjsonp`是基于c语言实现的轻量级json解释器，根据github开源项目[json-tutorial](https://github.com/miloyip/json-tutorial)

本项目旨在手动编写完整的json解析器，学习中小型项目的开发过程，以及git在项目中正确地使用方法

**特点：**
+ 极简设计：代码结构简单，专注核心逻辑
+ c语言实现，不依赖外部库，轻量高效
+ 严格遵循json规范，依照原项目流程，尽可能符合json规范

## 项目结构

|文件名|描述|
|---|---|
|`leptjson.c`|JSON 解释器的核心实现文件。|
|`leptjson.h`|库的头文件，包含了解释器 API 定义。|
|`test.c`|包含 main 函数的测试驱动文件，用于演示库的使用和验证解析功能。|
|`README.md`|项目说明文档（即本文档）。|

🛠️ 待办事项 / 开发计划

以下是未来计划或当前未完全实现的特性：
+ [] 实现原项目中json解析器的全部功能
+ [] 以cpp语言改写，尽可能体现cpp特性


测试：需在当前sheLl下指定`export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH`
