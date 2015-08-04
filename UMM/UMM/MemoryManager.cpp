/******************************************************************************************
MemoryManager.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: ʵ��ͳһ�ڴ����ӿ�

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/


#include<stdarg.h>
#include<string.h>
#include<memory>
#include<assert.h>

// C++11
//����C++11��ز��ַŵ�CPP�У��������UMM�⣬�ڲ�֧��C++11�ı����������ɿ���ʹ�á�
#include <mutex>
#include <thread>
#include <unordered_map>
using namespace std;

#include"MemoryManager.h"
#include"../IPC/IPCManager.h"

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

///////////////////////////////////////////////////////////////////////
// ����->���ڴ�й¶�������������ȵ������ ����ֵ��¼�� ���ڴ���������
// MemoryAnalyse

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

// �ڴ�����
class MemoryAnalyse : public Singleton<MemoryAnalyse>
{
	typedef unordered_map<void*, MemoryBlockInfo> MemoryLeakMap;
	typedef unordered_map<string, CountInfo> MemoryHostMap;
	//typedef std::map<void*, MemoryBlockInfo> MemoryLeakMap;
	//typedef std::map<std::string, CountInfo> MemoryHostMap;
	typedef vector<MemoryBlockInfo> MemoryBlcokInfos;

public:
	// ��Ӽ�¼
	void AddRecord(const MemoryBlockInfo& memBlockInfo)
	{
		int flag = ConfigManager::GetInstance()->GetOptions();
		if (flag == CO_NONE)
			return;

		void* ptr = memBlockInfo._ptr;
		const string& type = memBlockInfo._type;
		const size_t& size = memBlockInfo._size;

		unique_lock<mutex> lock(_mutex);

		// �����ڴ�й¶���¼
		if (flag & CO_ANALYSE_MEMORY_LEAK)
		{
			_leakBlockMap[ptr] = memBlockInfo;
		}

		// �����ڴ��ȵ��¼
		if (flag & CO_ANALYSE_MEMORY_HOST)
		{
			_hostObjectMap[type].AddCount(memBlockInfo._size);
		}
		// �����ڴ�������Ϣ(�ڴ���¼&�������)
		if (flag & CO_ANALYSE_MEMORY_ALLOC_INFO)
		{
			_memBlcokInfos.push_back(memBlockInfo);

			_allocCountInfo.AddCount(size);
		}
	}

	// ɾ����¼
	void DelRecord(const MemoryBlockInfo& memBlockInfo)
	{
		// ��ȡ������Ϣ
		int flag = ConfigManager::GetInstance()->GetOptions();
		if (flag == CO_NONE)
			return;

		void* ptr = memBlockInfo._ptr;
		const string& type = memBlockInfo._type;
		const size_t& size = memBlockInfo._size;

		unique_lock<mutex> lock(_mutex);

		// �����ڴ�й¶���¼
		if (flag & CO_ANALYSE_MEMORY_LEAK)
		{
			MemoryLeakMap::iterator it = _leakBlockMap.find(ptr);
			if (it != _leakBlockMap.end())
			{
				_leakBlockMap.erase(it);
			}
			else
			{
				perror("��Leak��Record Memory Block No Match");
			}
		}

		// �����ڴ��ȵ��¼
		if (flag & CO_ANALYSE_MEMORY_HOST)
		{
			MemoryHostMap::iterator it = _hostObjectMap.find(type);
			if (it != _hostObjectMap.end())
			{
				it->second.DelCount(size);
			}
			else
			{
				perror("��host��Record Memory Block No Match");
			}
		}

		// �����ڴ��������
		if (flag & CO_ANALYSE_MEMORY_ALLOC_INFO)
		{
			_allocCountInfo.DelCount(memBlockInfo._size);
		}
	}

	// ���������������õ���������
	static void Save()
	{
		int flag = ConfigManager::GetInstance()->GetOptions();

		if (flag & CO_SAVE_TO_CONSOLE)
		{
			ConsoleSaveAdapter CSA;
			MemoryAnalyse::GetInstance()->OutPut(CSA);
		}

		if (flag & CO_SAVE_TO_FILE)
		{
			const string& path = ConfigManager::GetInstance()->GetOutputPath();
			FileSaveAdapter FSA(path.c_str());
			MemoryAnalyse::GetInstance()->OutPut(FSA);
		}
	}

	// ��������������Ӧ��������
	void OutPut(SaveAdapter& SA)
	{
		int flag = ConfigManager::GetInstance()->GetOptions();

		SA.Save("---------------------------��Analyse Report��---------------------------\r\n");

		if (flag & CO_ANALYSE_MEMORY_LEAK)
		{
			OutPutLeakBlockInfo(SA);
		}
		if (flag & CO_ANALYSE_MEMORY_HOST)
		{
			OutPutHostObjectInfo(SA);
		}
		if (flag & CO_ANALYSE_MEMORY_ALLOC_INFO)
		{
			OutPutAllBlockInfo(SA);
		}

		SA.Save("-----------------------------��   end   ��----------------------------\r\n");
	}

	// ����������
	void Clear()
	{
		unique_lock<mutex> lock(_mutex);

		_leakBlockMap.clear();
		_hostObjectMap.clear();
		_memBlcokInfos.clear();
	}

	void OutPutLeakBlockInfo(SaveAdapter& SA)
	{
		//SA.Save(BEGIN_LINE);
		{
			unique_lock<mutex> lock(_mutex);

			if (!_leakBlockMap.empty())
			{
				SA.Save("\r\n��Memory Leak Block Info��\r\n");
			}

			int index = 1;
			MemoryLeakMap::iterator it = _leakBlockMap.begin();
			for (; it != _leakBlockMap.end(); ++it)
			{
				SA.Save("NO%d.\r\n", index++);
				it->second.Serialize(SA);
			}
		}

		//SA.Save(BEGIN_LINE);
	}
	void OutPutHostObjectInfo(SaveAdapter& SA)
	{
		//SA.Save(BEGIN_LINE);

		{
			unique_lock<mutex> lock(_mutex);

			if (!_hostObjectMap.empty())
			{
				SA.Save("\r\n��Memory Host Object Statistics��\r\n");
			}

			int index = 1;
			MemoryHostMap::iterator it = _hostObjectMap.begin();
			for (; it != _hostObjectMap.end(); ++it)
			{
				SA.Save("NO%d.\r\n", index++);
				SA.Save("type:%s , ", it->first.c_str());
				it->second.Serialize(SA);
			}
		}

		//SA.Save(BEGIN_LINE);
	}

	void OutPutAllBlockInfo(SaveAdapter& SA)
	{
		if (_memBlcokInfos.empty())
			return;

		//SA.Save(BEGIN_LINE);

		{
			unique_lock<mutex> lock(_mutex);

			SA.Save("\r\n��Alloc Count Statistics��\r\n");
			_allocCountInfo.Serialize(SA);

			SA.Save("\r\n��All Memory Blcok Info��\r\n");
			int index = 1;
			MemoryBlcokInfos::iterator it = _memBlcokInfos.begin();
			for (; it != _memBlcokInfos.end(); ++it)
			{
				SA.Save("NO%d.\r\n", index++);
				it->Serialize(SA);
			}
		}

		//SA.Save(BEGIN_LINE);
	}

private:
	MemoryAnalyse()
	{
		atexit(Save);
	}

	friend class Singleton<MemoryAnalyse>;
private:
	mutex			 _mutex;				// ����������
	MemoryLeakMap	 _leakBlockMap;			// �ڴ�й¶���¼
	MemoryHostMap	 _hostObjectMap;		// �ȵ����(�Զ�������typeΪkey����ͳ��)
	MemoryBlcokInfos _memBlcokInfos;		// �ڴ������¼
	CountInfo		 _allocCountInfo;		// ���������¼
};

//////////////////////////////////////////////////////////////////////
// IPC���߿��Ƽ�������
// IPCMonitorServer

int GetProcessId()
{
	int processId = 0;

#ifdef _WIN32
	processId = GetProcessId(GetCurrentProcess());
#else
	processId = getpid();
#endif

	return processId;
}

#ifdef _WIN32
const char* SERVER_PIPE_NAME = "\\\\.\\Pipe\\ServerPipeName";
#else
const char* SERVER_PIPE_NAME = "tmp/memory_manager/_fifo";
#endif

string GetServerPipeName()
{
	string name = SERVER_PIPE_NAME;
	char idStr[10];
	_itoa(GetProcessId(), idStr, 10);
	name += idStr;

	return name;
}

class IPCMonitorServer : public Singleton<IPCMonitorServer>
{
	typedef void(*CmdFunc) (string& reply);
	typedef map<string, CmdFunc> CmdFuncMap;

public:
	// ����IPC��Ϣ��������߳�
	void IPCMonitorServer::Start()
	{}

protected:
	// IPC�����̴߳�����Ϣ�ĺ���
	void IPCMonitorServer::OnMessage()
	{
		const int IPC_BUF_LEN = 1024;
		char msg[IPC_BUF_LEN] = { 0 };
		IPCServer server(GetServerPipeName().c_str());

		while (1)
		{
			server.ReceiverMsg(msg, IPC_BUF_LEN);
			printf("Receiver Cmd Msg: %s\n", msg);

			string reply;
			string cmd = msg;
			CmdFuncMap::iterator it = _cmdFuncsMap.find(cmd);
			if (it != _cmdFuncsMap.end())
			{
				CmdFunc func = it->second;
				func(reply);
			}
			else
			{
				reply = "Invalid Command";
			}

			server.SendReplyMsg(reply.c_str(), reply.size());
		}
	}

	//
	// ���¾�Ϊ��Ϣ������
	//

	static void GetState(string& reply)
	{
		reply += "State:";
		int flag = ConfigManager::GetInstance()->GetOptions();

		if (flag == CO_NONE)
		{
			reply += "Analyse None\n";
			return;
		}

		if (flag & CO_ANALYSE_MEMORY_LEAK)
		{
			reply += "Analyse Memory Leak\n";
		}

		if (flag & CO_ANALYSE_MEMORY_HOST)
		{
			reply += "Analyse Memory Host\n";
		}

		if (flag & CO_ANALYSE_MEMORY_ALLOC_INFO)
		{
			reply += "Analyse All Memory Alloc Info\n";
		}

		if (flag & CO_SAVE_TO_CONSOLE)
		{
			reply += "Save To Console\n";
		}

		if (flag & CO_SAVE_TO_FILE)
		{
			reply += "Save To File\n";
		}
	}

	static void Enable(string& reply)
	{
		ConfigManager::GetInstance()->SetOptions(CO_ANALYSE_MEMORY_HOST
			| CO_ANALYSE_MEMORY_LEAK
			| CO_ANALYSE_MEMORY_ALLOC_INFO
			| CO_SAVE_TO_FILE);

		reply += "Enable Success";
	}

	static void Disable(string& reply)
	{
		ConfigManager::GetInstance()->SetOptions(CO_NONE);

		reply += "Disable Success";
	}

	static void Save(string& reply)
	{
		const string& path = ConfigManager::GetInstance()->GetOutputPath();
		FileSaveAdapter FSA(path.c_str());
		MemoryAnalyse::GetInstance()->OutPut(FSA);

		reply += "Save Success";
	}

	static void Print(string& reply)
	{
		// ����Ӧ�ðѽ�������Ϣ���ظ��ܵ��ͻ��ˣ�
		ConsoleSaveAdapter CSA;
		MemoryAnalyse::GetInstance()->OutPut(CSA);

		reply += "Print Success";
	}

	static void Clear(string& reply)
	{
		MemoryAnalyse::GetInstance()->Clear();

		reply += "Clear Success";
	}
protected:
	IPCMonitorServer::IPCMonitorServer()
		:_onMsgThread(&IPCMonitorServer::OnMessage, this)
	{
		printf("%s IPC Monitor Server Start\n", GetServerPipeName().c_str());

		_cmdFuncsMap["state"] = GetState;
		_cmdFuncsMap["save"] = Save;
		_cmdFuncsMap["print"] = Print;
		_cmdFuncsMap["disable"] = Disable;
		_cmdFuncsMap["enable"] = Enable;
		_cmdFuncsMap["clear"] = Clear;
	}

	friend class Singleton<IPCMonitorServer>;
private:
	thread	_onMsgThread;			// ������Ϣ�߳�
	CmdFuncMap _cmdFuncsMap;		// ��Ϣ���ִ�к�����ӳ���
};


////////////////////////////////////////////////////////////
// ConfigManager

// ����/��ȡ����ѡ����Ϣ
void ConfigManager::SetOptions(int flag)
{
	_flag = flag;
}

int ConfigManager::GetOptions()
{
	return _flag;
}

void ConfigManager::SetOutputPath(const string& path)
{
	_outputPath = path;
}

const string& ConfigManager::GetOutputPath()
{
	return _outputPath;
}

////////////////////////////////////////////////////////////////////////////
// MemoryManager

//��ȡ·���������ļ���
static string GetFileName(const string& path)
{
	char ch = '/';

#ifdef _WIN32
	ch = '\\';
#endif

	size_t pos = path.rfind(ch);
	if (pos == string::npos)
	{
		return path;
	}
	else
	{
		return path.substr(pos + 1);
	}
}

MemoryBlockInfo::MemoryBlockInfo(const char* type, int num, size_t size,
	void * ptr, const char* filename, int fileline)
	: _type(type)
	, _ptr(ptr)
	, _num(num)
	, _size(size)
	, _filename(GetFileName(filename))
	, _fileline(fileline)
{}

// ���л����ݵ�������
void MemoryBlockInfo::Serialize(SaveAdapter& SA)
{
	SA.Save("Blcok->Ptr:0X%x , Type:%s , Num:%d, Size:%d\r\nFilename:%s , Fileline:%d\r\n",
		_ptr, _type.c_str(), _num, _size, _filename.c_str(), _fileline);
}

MemoryManager::MemoryManager()
{
	// ���������MemoryManager���������߿��Ʒ���
	// ps������ᵼ�¶�̬�⿨������Ľ���
	IPCMonitorServer::GetInstance()->Start();
}

// �����ڴ�/����������Ϣ
void* MemoryManager::Alloc(const char* type, size_t size, int num,
	const char* filename, int fileline)
{
	void* ptr = _allocPool.allocate(size);

	if (ConfigManager::GetInstance()->GetOptions() != CO_NONE)
	{
		// �������������4���ֽڴ洢���������������ͳ������
		size_t realSize = num > 1 ? size - 4 : size;
		MemoryBlockInfo info(type, num, realSize, ptr, filename, fileline);
		MemoryAnalyse::GetInstance()->AddRecord(info);
	}

	return ptr;
}

// �ͷ��ڴ�/����������Ϣ
void MemoryManager::Dealloc(const char* type, void* ptr, size_t size)
{
	if (ptr)
	{
		_allocPool.deallocate((char*)ptr, size);

		if (ConfigManager::GetInstance()->GetOptions() != CO_NONE)
		{
			MemoryBlockInfo info(type, 0, size, ptr);
			MemoryAnalyse::GetInstance()->DelRecord(info);
		}
	}
}