/******************************************************************************************
MemoryManager.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: ����ͳһ�ڴ����ӿ�

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/

#pragma once

// ps����ǰ��̬��ʹ�ô��ڿ��������⣬�����ٽ������ͨ����̬�ⷽʽʹ�á�

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

// �����̬�⵼����̬���������⣬UMM�еĵ�����Ϊ��̬����
#if(defined(_WIN32) && defined(IMPORT))
	#define API_EXPORT _declspec(dllimport)
#elif _WIN32
	#define API_EXPORT _declspec(dllexport)
#else
	#define API_EXPORT
#endif

class SaveAdapter;
class MemoryAnalyse;

// ����������
class SaveAdapter
{
public:
	virtual int Save(char* format, ...) = 0;
};

// ����̨����������
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

// �ļ�����������
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

// ��������
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
//// ��̬����ָ���ʼ������֤�̰߳�ȫ�� 
//template<class T>
//T* Singleton<T>::_sInstance = new T();

//
// ���������Ǿ�̬�������ڴ����ĵ��������캯���л�����IPC����Ϣ�����̡߳�
// ������ɶ�̬��ʱ������main���֮ǰ�ͻ��ȼ��ض�̬�⣬��ʹ������ķ�ʽ����ʱ
// �ͻᴴ������������ʱ����IPC����Ϣ�����߳̾ͻῨ��������ʹ������ĵ���ģʽ��
// �ڵ�һ��ȡ��������ʱ��������������
//

// ��������
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		//
		// 1.˫�ؼ�鱣���̰߳�ȫ��Ч�ʡ�
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

	static T* _sInstance;	// ��ʵ������
	static mutex _mutex;			// ����������
};

template<class T>
T* Singleton<T>::_sInstance = NULL;

template<class T>
mutex Singleton<T>::_mutex;

// ����ѡ��
enum ConfigOptions
{
	CO_NONE = 0,						// ��������
	CO_ANALYSE_MEMORY_LEAK = 1,			// �����ڴ�й¶
	CO_ANALYSE_MEMORY_HOST = 2,			// �����ڴ��ȵ�
	CO_ANALYSE_MEMORY_ALLOC_INFO = 4,	// �����ڴ��������
	CO_SAVE_TO_CONSOLE = 8,				// ���浽����̨
	CO_SAVE_TO_FILE = 16,				// ���浽�ļ�
};

////////////////////////////////////////////////////////////////////////////////////
// IPC�ļ���̷߳������ʵ�ִ���ͻ��˹���MemoryAnalyseTool���͵�������Ϣ��
// ֧�����߿����ڴ���������
//
class IPCMonitorServer : public Singleton<IPCMonitorServer>
{
	typedef void(*CmdFunc) (string& reply);
	typedef map<string, CmdFunc> CmdFuncMap;

public:
	// ����IPC��Ϣ��������߳�
	void Start();
protected:
	// IPC����������Ϣ���̺߳���
	void OnMessage();

	//
	// ���¾�Ϊ�۲���ģʽ�У���Ӧ������Ϣ�ĵĴ�����
	//
	static void GetState(string& reply);
	static void Enable(string& reply);
	static void Disable(string& reply);
	static void Save(string& reply);
protected:
	IPCMonitorServer::IPCMonitorServer();

	friend class Singleton<IPCMonitorServer>;
private:
	thread	_onMsgThread;			// ������Ϣ�߳�
	CmdFuncMap _cmdFuncsMap;		// ��Ϣ����->��������ӳ���
};

// ���ù������
class API_EXPORT ConfigManager : public Singleton<ConfigManager>
{
public:
	// ����/��ȡ����ѡ����Ϣ
	void SetOptions(int flag);
	int GetOptions();
	void SetOutputPath(const string& path);
	const std::string& GetOutputPath();

private:
	// Ĭ������²���������
	ConfigManager::ConfigManager()
		:_flag(CO_NONE)
		, _outputPath("MEMORY_ANALYSE_REPORT.txt")
	{}

	friend class Singleton<ConfigManager>;
private:
	int _flag;				 // ѡ����	
	std::string _outputPath; // ���·��
};

// �ڴ��������Ϣ
struct MemoryBlockInfo
{
	string	_type;			// ����
	int		_num;			// ���ݸ���
	size_t	_size;			// ���ݿ��С
	void*	_ptr;			// ָ���ڴ���ָ��
	string	_filename;		// �ļ���
	int		_fileline;		// �к�

	MemoryBlockInfo(const char* type = "", int num = 0, size_t size = 0,
		void * ptr = 0, const char* filename = "", int fileline = 0);

	// ���л����ݵ�������
	void Serialize(SaveAdapter& SA);
};

// ͳ�Ƽ�����Ϣ
struct CountInfo
{
	int _addCount;		// ��Ӽ���
	int _delCount;		// ɾ������

	long long _totalSize;	// �����ܴ�С

	long long _usedSize;	// ����ʹ�ô�С
	long long _maxPeakSize;	// ���ʹ�ô�С(����ֵ)

	CountInfo::CountInfo()
		: _addCount(0)
		, _delCount(0)
		, _totalSize(0)
		, _usedSize(0)
		, _maxPeakSize(0)
	{}

	// ���Ӽ���
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

	// ɾ������
	void CountInfo::DelCount(size_t size)
	{
		_delCount++;
		_usedSize -= size;
	}

	// ���л�ͳ�Ƽ�����Ϣ��������
	void CountInfo::Serialize(SaveAdapter& SA)
	{
		SA.Save("Alloc Count:%d , Dealloc Count:%d, Alloc Total Size:%lld byte, NonRelease Size:%lld, Max Used Size:%lld\r\n",
			_addCount, _delCount, _totalSize, _usedSize, _maxPeakSize);
	}
};

// �ڴ���������
class MemoryAnalyse : public Singleton<MemoryAnalyse>
{
	//
	// unordered_map�ڲ�ʹ��hash_tableʵ�֣�ʱ�临�Ӷ�Ϊ����map�ڲ�ʹ�ú������
	// unordered_map������ʱ�临�Ӷ�ΪO(1)��mapΪO(lgN)��so! unordered_map����Ч��
	// http://blog.chinaunix.net/uid-20384806-id-3055333.html
	//
	typedef unordered_map<void*, MemoryBlockInfo> MemoryLeakMap;
	typedef unordered_map<string, CountInfo> MemoryHostMap;
	//typedef std::map<void*, MemoryBlockInfo> MemoryLeakMap;
	//typedef std::map<std::string, CountInfo> MemoryHostMap;
	typedef vector<MemoryBlockInfo> MemoryBlcokInfos;

public:
	// ������ص�������¼
	void AddRecord(const MemoryBlockInfo& memBlockInfo);
	void DelRecord(const MemoryBlockInfo& memBlockInfo);

	// ����������Ϣ��Ӧ�������������Ӧ�ı���������
	static void OutPut();
	void _OutPut(SaveAdapter& SA);
	void _OutPutLeakBlockInfo(SaveAdapter& SA);
	void _OutPutHostObjectInfo(SaveAdapter& SA);
	void _OutPutAllBlockInfo(SaveAdapter& SA);
private:
	MemoryAnalyse();

	friend class Singleton<MemoryAnalyse>;
private:
	//mutex			 _mutex;				// ����������
	MemoryLeakMap	 _leakBlockMap;			// �ڴ�й¶���¼
	MemoryHostMap	 _hostObjectMap;		// �ȵ����(�Զ�������typeΪkey����ͳ��)
	MemoryBlcokInfos _memBlcokInfos;		// �ڴ������¼
	CountInfo		 _allocCountInfo;		// ���������¼
};

// �ڴ�������
class API_EXPORT MemoryManager : public Singleton<MemoryManager>
{
public:
	// �����ڴ�/����������Ϣ
	void* Alloc(const char* type, size_t size, int num,
		const char* filename, int fileline);

	// �ͷ��ڴ�/����������Ϣ
	void Dealloc(const char* type, void* ptr, size_t size);

private:
	// ����Ϊ˽��
	MemoryManager();

	// ����Ϊ��Ԫ�������޷�������ʵ��
	friend class Singleton<MemoryManager>;

	allocator<char> _allocPool;		// STL���ڴ�ض���
};

/////////////////////////////////////////////////////////
// �ڴ��������

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
	// �������ĸ��ֽڴ洢Ԫ�صĸ���
	//
	void* ptr = MemoryManager::GetInstance()->Alloc\
		(typeid(T).name(), sizeof(T)*num + 4, num, filename, fileline);

	// �ڴ����ʧ�ܣ���ֱ�ӷ���
	if (ptr == NULL)
		return NULL;

	*(int*)ptr = num;
	T* retPtr = (T*)((int)ptr + 4);

	//
	// ʹ�á�������ȡ���жϸ������Ƿ���Ҫ������������
	//��δ�Զ��幹�캯���������������Ͳ���Ҫ����������������
	// ����ʹ����C++11��is_pod��ȥ�����Զ����������ȡ���������ʵ�ʵ����һ���ġ�
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
	// ptr == NULL����ֱ�ӷ���
	if (ptr == NULL)
		return;

	T* selfPtr = (T*)((int)ptr - 4);
	int num = *(int*)(selfPtr);
	//
	// ʹ�á�������ȡ���жϸ������Ƿ���Ҫ������������
	//��δ�Զ��幹�캯���������������Ͳ���Ҫ����������������
	// ����ʹ����C++11��is_pod��ȥ�����Զ����������ȡ���������ʵ�ʵ����һ���ġ�
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
// �������
// @parm type ��������
// ps:NEWֱ��ʹ�ú��滻����֧�� NEW(int)(0)��ʹ�÷�ʽ
#define NEW(type)							\
	new(MemoryManager::GetInstance()->Alloc \
	(typeid(type).name(), sizeof(type), 1, __FILE__, __LINE__)) type

//
// �ͷŶ���
// @parm ptr ���ͷŶ����ָ��
//
#define DELETE(ptr)						\
	_DELETE(ptr)

//
// �����������
// @parm type ��������
// @parm num ����������
//
#define NEW_ARRAY(type, num)			\
	_NEW_ARRAY<type>(num, __FILE__, __LINE__)

// �ͷ��������
// @parm ptr ���ͷŶ����ָ��
//
#define DELETE_ARRAY(ptr)				\
	_DELETE_ARRAY(ptr)

//
// ��������ѡ��
// @parm flag ����ѡ��
//
#define SET_CONFIG_OPTIONS(flag)		\
	ConfigManager::GetInstance()->SetOptions(flag);