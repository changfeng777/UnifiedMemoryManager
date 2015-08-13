/******************************************************************************************
UMMTest:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: ����UMM��ؽӿڽ��и��ֲ��ԡ�

Author: xjh

Created Time: 2015-4-26
******************************************************************************************/

#include <iostream>
using namespace std;

#include "../UMM/MemoryManager.h"
#ifdef _WIN32
#pragma comment(lib, "../Debug/UMM.lib")
#endif // _WIN32

class CustomType
{
public:
	CustomType(int i = 0, int j = 0)
	{
		//cout << "CustomType():" <<i<<" ,"<<j<< endl;
	}

	~CustomType()
	{
		//cout << "~CustomType()" << endl;
	}
};
// ���Ի����ķ�����ͷ�
void Test1()
{
	cout << "���Ի��������Ƿ�����--begin" << endl;

	int* p1 = NEW(int)(1);
	*p1 = 2;
	DELETE(p1);

	int* a1 = NEW_ARRAY(int, 10);
	a1[0] = 0;
	a1[9] = 0;
	DELETE_ARRAY(a1);

	CustomType* p2 = NEW(CustomType)(1, 2);
	DELETE(p2);

	CustomType* a2 = NEW_ARRAY(CustomType, 10);
	DELETE_ARRAY(a2);

	cout << "���Ի��������Ƿ�����--end" << endl;
	cout << endl;
}

// �����ڴ�й¶���ڴ�������ع���
void Test2()
{
	cout << "�����ڴ�������ع���--begin" << endl;

	SET_CONFIG_OPTIONS(CO_ANALYSE_MEMORY_LEAK
		| CO_ANALYSE_MEMORY_HOST
		| CO_ANALYSE_MEMORY_ALLOC_INFO
		| CO_SAVE_TO_CONSOLE
		| CO_SAVE_TO_FILE);

	int* p1 = NEW(int)(1);
	*p1 = 2;
	int* a1 = NEW_ARRAY(int, 10);
	a1[0] = 0;
	a1[9] = 0;

	CustomType* p2 = NEW(CustomType)(1, 2);
	CustomType* a2 = NEW_ARRAY(CustomType, 10);
	
	//DELETE(p1);
	DELETE(p2);
	DELETE_ARRAY(a1);
	//DELETE_ARRAY(a2);

	cout << "�����ڴ������Ƿ�����--end" << endl;
	cout << endl;
}
#define __CPLUSPLUS11__
#ifdef  __CPLUSPLUS11__
#include <thread>

void New_Delete()
{
	SET_CONFIG_OPTIONS(CO_ANALYSE_MEMORY_LEAK
		| CO_ANALYSE_MEMORY_HOST
		| CO_SAVE_TO_CONSOLE);

	int count = 10;
	while (1)
	{
		int* p1 = NEW(int)(1);
		int* a1 = NEW_ARRAY(int, 10);

		this_thread::sleep_for(std::chrono::milliseconds(100));
		if (count-- == 0)
		{
			cout << "New_Delete->" <<this_thread::get_id()<<endl;
			count = 30;
		}

		DELETE(p1);
		DELETE_ARRAY(a1);
	}
}

// ���Զ��̰߳�ȫ����
void Test3()
{
	thread t1(New_Delete);
	thread t2(New_Delete);
	thread t3(New_Delete);

	t1.join();
	t2.join();
	t3.join();
}

// ����IPC���߿���
void Test4()
{
	int count = 10;
	while (1)
	{
		int* p1 = NEW(int)(1);
		CustomType* p2 = NEW(CustomType)(1, 2);

		int* a1 = NEW_ARRAY(int, 10);
		CustomType* a2 = NEW_ARRAY(CustomType, 10);

		this_thread::sleep_for(std::chrono::milliseconds(1000));

		if (--count == 0)
		{
			count = 10;
			continue;
		}

		DELETE(p1);
		DELETE(p2);
		DELETE_ARRAY(a1);
		DELETE_ARRAY(a2);
	}
}

#endif

int main()
{
	//Test1();
	//Test2();
	//Test3();
	Test4();

	return 0;
}