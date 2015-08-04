# UnifiedMemoryManager
统一内存管理

###功能说明：
    1：实现定置的统一内存管理接口，支持剖析内存泄露、内存热点、内存块分配情况。
    2：为了保证性能，默认不开启剖析功能，可通过接口配置、配置文件、在线控制等方式开启/关闭控制功能。
    3：提供NEW/DELETE/NEW_ARRAY/DELETE_ARRAY四个定置宏接口。提供SET_CONFIG_OPTIONS接口设置剖析选项。
    4：后台默认启动一个管道监听服务线程，监听客户端工具的消息，实现在线开启/关闭剖析选项，生成剖析报告。

详细的使用方式可参考UMM/Test.cpp下的测试用例。

框架设计说明：
###设计如下几个单例类
        内存管理类（负责分配内存/释放内存）
        内存剖析类（剖析内存泄露、内存热点、内存分配情况）
        配置管理类（管理剖析选项）。
代码UML类图
![image](https://github.com/changfeng777/UnifiedMemoryManager/raw/master/UMM/UMM.gif) 

###代码结构：
    UMM/IPC目录下为命名管道的实现代码。
    UMM/UMM目录下为内存管理实现代码。
    UMM/UMMTest目录下为内存管理相关接口测试的实现代码。
    UMM/MemoryAnalyseTool目录下为在线控制的管理实现代码。
    UMM/UML框架类图目录下为项目的框架类图文件，使用StartUML可以打开。
