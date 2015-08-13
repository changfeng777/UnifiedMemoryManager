/******************************************************************************************
MemoryManager.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: 声明统一内存管理接口

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/

#pragma once

// ps：当前动态库使用存在卡死的问题，后续再解决，先通过静态库方式使用。

#include<stdarg.h>
#include<string.h>
#include<memory>
#include<assert.h>

#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// C++11
#include <mutex>
#include <thread>
#include <unordered_map>
using namespace std;

#pragma warning(disable:4996)

// 解决动态库导出静态变量的问题，UMM中的单例类为静态对象
#if(defined(_WIN32) && defined(IMPORT))
	#define API_EXPORT _declspec(dllimport)
#elif _WIN32
	#define API_EXPORT _declspec(dllexport)
#else
	#define API_EXPORT
#endif

class SaveAdapter;
class MemoryAnalyse;

// 保存适配器
class SaveAdapter
{
public:
	virtual int Save(char* format, ...) = 0;
};

// 控制台保存适配器
class ConsoleSaveAdapter : public SaveAdapter
{
public:
	virtual int Save(char* format, ...)
	{
		va_list argPtr;
		int cnt;

		va_start(argPtr, format);
		cnt = vfprintf(stdout, format, argPtr);
		va_end(argPtr);

		return cnt;
	}
};

// 文件保存适配器
class FileSaveAdapter : public SaveAdapter
{
public:
	FileSaveAdapter(const char* path)
		:_fOut(0)
	{
		_fOut = fopen(path, "w");
	}

	~FileSaveAdapter()
	{
		if (_fOut)
		{
			fclose(_fOut);
		}
	}

	virtual int Save(char* format, ...)
	{
		if (_fOut)
		{
			va_list argPtr;
			int cnt;

			va_start(argPtr, format);
			cnt = vfprintf(_fOut, format, argPtr);
			va_end(argPtr);

			return cnt;
		}

		return 0;
	}
private:
	FileSaveAdapter(const FileSaveAdapter&);
	FileSaveAdapter& operator==(const FileSaveAdapter&);

private:
	FILE* _fOut;
};

// 单例基类
//template<class T>
//class Singleton
//{
//public:
//	static T* GetInstance()
//	{
//		return _sInstance;
//	}
//protected:
//	Singleton()
//	{}
//
//	static T* _sInstance;
//};
//
//// 静态对象指针初始化，保证线程安全。 
//template<class T>
//T* Singleton<T>::_sInstance = new T();

//
// 单例对象是静态对象，在内存管理的单例对象构造函数中会启动IPC的消息服务线程。
// 当编译成动态库时，程序main入口之前就会先加载动态库，而使用上面的方式，这时
// 就会创建单例对象，这时创建IPC的消息服务线程就会卡死。所以使用下面的单例模式，
// 在第一获取单例对象时，创建单例对象。
//

// 单例基类
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		//
		// 1.双重检查保障线程安全和效率。
		//
		if (_sInstance == NULL)
		{
			unique_lock<mutex> lock(_mutex);
			if (_sInstance == NULL)
			{
				_sInstance = new T();
			}
		}

		return _sInstance;
	}
protected:
	Singleton()
	{}

	static T* _sInstance;	// 单实例对象
	static mutex _mutex;			// 互斥锁对象
};

template<class T>
T* Singleton<T>::_sInstance = NULL;

template<class T>
mutex Singleton<T>::_mutex;

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

////////////////////////////////////////////////////////////////////////////////////
// IPC的监控线程服务对象，实现处理客户端工具MemoryAnalyseTool发送的命令消息，
// 支持在线控制内存剖析功能
//
class IPCMonitorServer : public Singleton<IPCMonitorServer>
{
	typedef void(*CmdFunc) (string& reply);
	typedef map<string, CmdFunc> CmdFuncMap;

public:
	// 启动IPC消息处理服务线程
	void Start();
protected:
	// IPC处理命令消息的线程函数
	void OnMessage();

	//
	// 以下均为观察者模式中，对应命令消息的的处理函数
	//
	static void GetState(string& reply);
	static void Enable(string& reply);
	static void Disable(string& reply);
	static void Save(string& reply);
protected:
	IPCMonitorServer::IPCMonitorServer();

	friend class Singleton<IPCMonitorServer>;
private:
	thread	_onMsgThread;			// 处理消息线程
	CmdFuncMap _cmdFuncsMap;		// 消息命令->处理函数的映射表
};

// 配置管理对象
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

// 统计计数信息
struct CountInfo
{
	int _addCount;		// 添加计数
	int _delCount;		// 删除计数

	long long _totalSize;	// 分配总大小

	long long _usedSize;	// 正在使用大小
	long long _maxPeakSize;	// 最大使用大小(最大峰值)

	CountInfo::CountInfo()
		: _addCount(0)
		, _delCount(0)
		, _totalSize(0)
		, _usedSize(0)
		, _maxPeakSize(0)
	{}

	// 增加计数
	void CountInfo::AddCount(size_t size)
	{
		_addCount++;
		_totalSize += size;
		_usedSize += size;

		if (_usedSize > _maxPeakSize)
		{
			_maxPeakSize = _usedSize;
		}
	}

	// 删除计数
	void CountInfo::DelCount(size_t size)
	{
		_delCount++;
		_usedSize -= size;
	}

	// 序列化统计计数信息到适配器
	void CountInfo::Serialize(SaveAdapter& SA)
	{
		SA.Save("Alloc Count:%d , Dealloc Count:%d, Alloc Total Size:%lld byte, NonRelease Size:%lld, Max Used Size:%lld\r\n",
			_addCount, _delCount, _totalSize, _usedSize, _maxPeakSize);
	}
};

// 内存剖析对象
class MemoryAnalyse : public Singleton<MemoryAnalyse>
{
	//
	// unordered_map内部使用hash_table实现（时间复杂度为），map内部使用红黑树。
	// unordered_map操作的时间复杂度为O(1)，map为O(lgN)，so! unordered_map更高效。
	// http://blog.chinaunix.net/uid-20384806-id-3055333.html
	//
	typedef unordered_map<void*, MemoryBlockInfo> MemoryLeakMap;
	typedef unordered_map<string, CountInfo> MemoryHostMap;
	//typedef std::map<void*, MemoryBlockInfo> MemoryLeakMap;
	//typedef std::map<std::string, CountInfo> MemoryHostMap;
	typedef vector<MemoryBlockInfo> MemoryBlcokInfos;

public:
	// 更新相关的剖析记录
	void AddRecord(const MemoryBlockInfo& memBlockInfo);
	void DelRecord(const MemoryBlockInfo& memBlockInfo);

	// 根据配置信息相应的剖析结果到对应的保存适配器
	static void OutPut();
	void _OutPut(SaveAdapter& SA);
	void _OutPutLeakBlockInfo(SaveAdapter& SA);
	void _OutPutHostObjectInfo(SaveAdapter& SA);
	void _OutPutAllBlockInfo(SaveAdapter& SA);
private:
	MemoryAnalyse();

	friend class Singleton<MemoryAnalyse>;
private:
	//mutex			 _mutex;				// 互斥锁对象
	MemoryLeakMap	 _leakBlockMap;			// 内存泄露块记录
	MemoryHostMap	 _hostObjectMap;		// 热点对象(以对象类型type为key进行统计)
	MemoryBlcokInfos _memBlcokInfos;		// 内存块分配记录
	CountInfo		 _allocCountInfo;		// 分配计数记录
};

// 内存管理对象
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

	// 内存分配失败，则直接返回
	if (ptr == NULL)
		return NULL;

	*(int*)ptr = num;
	T* retPtr = (T*)((int)ptr + 4);

	//
	// 使用【类型萃取】判断该类型是否需要调用析构函数
	//【未自定义构造函数析构函数的类型不需要调构造析构函数】
	// 这里使用了C++11的is_pod，去掉了自定义的内型萃取，不过本质的实现是一样的。
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
	// ptr == NULL，则直接返回
	if (ptr == NULL)
		return;

	T* selfPtr = (T*)((int)ptr - 4);
	int num = *(int*)(selfPtr);
	//
	// 使用【类型萃取】判断该类型是否需要调用析构函数
	//【未自定义构造函数析构函数的类型不需要调构造析构函数】
	// 这里使用了C++11的is_pod，去掉了自定义的内型萃取，不过本质的实现是一样的。
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