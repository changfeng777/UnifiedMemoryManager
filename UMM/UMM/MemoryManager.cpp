/******************************************************************************************
MemoryManager.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: ʵ��ͳһ�ڴ����ӿ�

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/

#include"MemoryManager.h"
#include"../IPC/IPCManager.h"

///////////////////////////////////////////////////////////////////////
// ����->���ڴ�й¶�������������ȵ������ ����ֵ��¼�� ���ڴ���������
// MemoryAnalyse

// ��Ӽ�¼
void MemoryAnalyse::AddRecord(const MemoryBlockInfo& memBlockInfo)
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
void MemoryAnalyse::DelRecord(const MemoryBlockInfo& memBlockInfo)
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
void MemoryAnalyse::OutPut()
{
	int flag = ConfigManager::GetInstance()->GetOptions();

	if (flag & CO_SAVE_TO_CONSOLE)
	{
		ConsoleSaveAdapter CSA;
		MemoryAnalyse::GetInstance()->_OutPut(CSA);
	}

	if (flag & CO_SAVE_TO_FILE)
	{
		const string& path = ConfigManager::GetInstance()->GetOutputPath();
		FileSaveAdapter FSA(path.c_str());
		MemoryAnalyse::GetInstance()->_OutPut(FSA);
	}
}

// ��������������Ӧ��������
void MemoryAnalyse::_OutPut(SaveAdapter& SA)
{
	int flag = ConfigManager::GetInstance()->GetOptions();

	SA.Save("---------------------------��Analyse Report��---------------------------\r\n");

	if (flag & CO_ANALYSE_MEMORY_LEAK)
	{
		_OutPutLeakBlockInfo(SA);
	}
	if (flag & CO_ANALYSE_MEMORY_HOST)
	{
		_OutPutHostObjectInfo(SA);
	}
	if (flag & CO_ANALYSE_MEMORY_ALLOC_INFO)
	{
		_OutPutAllBlockInfo(SA);
	}

	SA.Save("-----------------------------��   end   ��----------------------------\r\n");
}

void MemoryAnalyse::_OutPutLeakBlockInfo(SaveAdapter& SA)
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

void MemoryAnalyse::_OutPutHostObjectInfo(SaveAdapter& SA)
{
	//SA.Save(BEGIN_LINE);

	// ���ȵ���Ϣ��������ıȽ���
	struct CompareHostInfo
	{
		bool operator() (MemoryHostMap::iterator lhs, MemoryHostMap::iterator rhs)
		{
			return lhs->second._totalSize > rhs->second._totalSize;
		}
	};
	vector<MemoryHostMap::iterator> vHostInofs;

	{
		unique_lock<mutex> lock(_mutex);
		MemoryHostMap::iterator it = _hostObjectMap.begin();
		for (; it != _hostObjectMap.end(); ++it)
		{
			vHostInofs.push_back(it);
		}
	}

	if (!vHostInofs.empty())
	{
		SA.Save("\r\n��Memory Host Object Statistics��\r\n");
	}

	// ���ȵ����Ͱ�����Ĵ�С���������ܴ�С�������ǰ�����
	sort(vHostInofs.begin(), vHostInofs.end(), CompareHostInfo());	
	for (int index = 0; index < vHostInofs.size(); ++index)
	{
		SA.Save("NO%d.\r\n", index + 1);
		SA.Save("type:%s , ", vHostInofs[index]->first.c_str());
		vHostInofs[index]->second.Serialize(SA);
	}

	//SA.Save(BEGIN_LINE);
}

void MemoryAnalyse::_OutPutAllBlockInfo(SaveAdapter& SA)
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

MemoryAnalyse::MemoryAnalyse()
{
	atexit(OutPut);
}

//////////////////////////////////////////////////////////////////////
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

// ����IPC��Ϣ��������߳�
void IPCMonitorServer::Start()
{}

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

void IPCMonitorServer::GetState(string& reply)
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

void IPCMonitorServer::Enable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(CO_ANALYSE_MEMORY_HOST
		| CO_ANALYSE_MEMORY_LEAK
		| CO_ANALYSE_MEMORY_ALLOC_INFO
		| CO_SAVE_TO_FILE);

	reply += "Enable Success";
}

void IPCMonitorServer::Disable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(CO_NONE);

	reply += "Disable Success";
}

void IPCMonitorServer::Save(string& reply)
{
	const string& path = ConfigManager::GetInstance()->GetOutputPath();
	FileSaveAdapter FSA(path.c_str());
	MemoryAnalyse::GetInstance()->_OutPut(FSA);

	reply += "Save Success";
}

IPCMonitorServer::IPCMonitorServer()
	:_onMsgThread(&IPCMonitorServer::OnMessage, this)
{
	printf("%s IPC Monitor Server Start\n", GetServerPipeName().c_str());

	_cmdFuncsMap["state"] = GetState;
	_cmdFuncsMap["save"] = Save;
	_cmdFuncsMap["disable"] = Disable;
	_cmdFuncsMap["enable"] = Enable;
}


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