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

// ��������
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

// ��̬����ָ���ʼ������֤�̰߳�ȫ�� 
template<class T>
T* Singleton<T>::_sInstance = new T();

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

// ���ù���
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

// �ڴ����
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

	*(int*)ptr = num;
	T* retPtr = (T*)((int)ptr + 4);

	//
	// ʹ��������ȡ�жϸ������Ƿ���Ҫ���ù��캯����ʼ��
	//��δ�Զ��幹�캯���������������Ͳ���Ҫ����
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
	// ʹ�á�������ȡ���жϸ������Ƿ���Ҫ������������
	//��δ�Զ��幹�캯���������������Ͳ���Ҫ����
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