/******************************************************************************************
MemoryManager.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: 声明统一内存管理接口

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/

#pragma once

// ps：当前动态库使用存在卡死的问题，后续再解决。先通过静态库方式使用。

#include <cstddef>
#include <string>
#include <vector>
#include <map>

#pragma warning(disable:4996)

#ifdef _WIN32
#define API_EXPORT _declspec(dllexport)
#else 
#define API_EXPORT
#endif // __WIN32

class SaveAdapter;
class MemoryAnalyse;

// 单例基类
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		return _sInstance;
	}
protected:
	Singleton()
	{}

	static T* _sInstance;
};

// 静态对象指针初始化，保证线程安全。 
template<class T>
T* Singleton<T>::_sInstance = new T();

// 配置选项
enum ConfigOptions
{
	CO_NONE = 0,						// 不做剖析
	CO_ANALYSE_MEMORY_LEAK = 1,			// 剖析内存泄露
	CO_ANALYSE_MEMORY_HOST = 2,			// 剖析内存热点
	CO_ANALYSE_MEMORY_ALLOC_INFO = 4,	// 剖析内存块分配情况
	CO_SAVE_TO_CONSOLE = 8,				// 保存到控制台
	CO_SAVE_TO_FILE = 16,				// 保存到文件
};

// 配置管理
class API_EXPORT ConfigManager : public Singleton<ConfigManager>
{
public:
	// 设置/获取配置选项信息
	void SetOptions(int flag);
	int GetOptions();
	void SetOutputPath(const string& path);
	const std::string& GetOutputPath();

private:
	// 默认情况下不开启剖析
	ConfigManager::ConfigManager()
		:_flag(CO_NONE)
		, _outputPath("MEMORY_ANALYSE_REPORT.txt")
	{}

	friend class Singleton<ConfigManager>;
private:
	int _flag;				 // 选项标记	
	std::string _outputPath; // 输出路径
};

// 内存块剖析信息
struct MemoryBlockInfo
{
	string	_type;			// 类型
	int		_num;			// 数据个数
	size_t	_size;			// 数据块大小
	void*	_ptr;			// 指向内存块的指针
	string	_filename;		// 文件名
	int		_fileline;		// 行号

	MemoryBlockInfo(const char* type = "", int num = 0, size_t size = 0,
		void * ptr = 0, const char* filename = "", int fileline = 0);

	// 序列化数据到适配器
	void Serialize(SaveAdapter& SA);
};

// 内存管理
class API_EXPORT MemoryManager : public Singleton<MemoryManager>
{
public:
	// 申请内存/更新剖析信息
	void* Alloc(const char* type, size_t size, int num,
		const char* filename, int fileline);

	// 释放内存/更新剖析信息
	void Dealloc(const char* type, void* ptr, size_t size);

private:
	// 定义为私有
	MemoryManager();

	// 定义为友元，否则无法创建单实例
	friend class Singleton<MemoryManager>;

	allocator<char> _allocPool;		// STL的内存池对象
};

/////////////////////////////////////////////////////////
// 内存管理处理函数

//template<typename T>
//inline T* _ALLOC(const char* filename, int fileline)
//{
//	return new (MemoryManager::GetInstance()->Alloc
//		(sizeof(T), typeid(T).name(), 1, filename, fileline))T();
//}

template<typename T>
inline void _DELETE(T* ptr)
{
	if (ptr)
	{
		ptr->~T();
		MemoryManager::GetInstance()->Dealloc(typeid(T).name(), ptr, sizeof(T));
	}
}

template<typename T>
inline T* _NEW_ARRAY(size_t num, const char* filename, int fileline)
{
	//
	// 多申请四个字节存储元素的个数
	//
	void* ptr = MemoryManager::GetInstance()->Alloc\
		(typeid(T).name(), sizeof(T)*num + 4, num, filename, fileline);

	*(int*)ptr = num;
	T* retPtr = (T*)((int)ptr + 4);

	//
	// 使用类型萃取判断该类型是否需要调用构造函数初始化
	//【未自定义构造函数析构函数的类型不需要调】
	//
	if (is_pod<T>::value == false)
	{
		for (size_t i = 0; i < num; ++i)
		{
			new(&retPtr[i])T();
		}
	}

	return retPtr;
}

template<typename T>
inline void _DELETE_ARRAY(T* ptr)
{
	T* selfPtr = (T*)((int)ptr - 4);
	int num = *(int*)(selfPtr);
	//
	// 使用【类型萃取】判断该类型是否需要调用析构函数
	//【未自定义构造函数析构函数的类型不需要调】
	//
	if (is_pod<T>::value == false)
	{
		for (int i = 0; i < num; ++i)
		{
			ptr[i].~T();
		}
	}

	MemoryManager::GetInstance()->Dealloc(typeid(T).name(), selfPtr, sizeof(T)*num);
}

//
// 分配对象
// @parm type 申请类型
// ps:NEW直接使用宏替换，可支持 NEW(int)(0)的使用方式
#define NEW(type)							\
	new(MemoryManager::GetInstance()->Alloc \
	(typeid(type).name(), sizeof(type), 1, __FILE__, __LINE__)) type

//
// 释放对象
// @parm ptr 待释放对象的指针
//
#define DELETE(ptr)						\
	_DELETE(ptr)

//
// 分配对象数组
// @parm type 申请类型
// @parm num 申请对象个数
//
#define NEW_ARRAY(type, num)			\
	_NEW_ARRAY<type>(num, __FILE__, __LINE__)

// 释放数组对象
// @parm ptr 待释放对象的指针
//
#define DELETE_ARRAY(ptr)				\
	_DELETE_ARRAY(ptr)

//
// 设置配置选项
// @parm flag 配置选项
//
#define SET_CONFIG_OPTIONS(flag)		\
	ConfigManager::GetInstance()->SetOptions(flag);