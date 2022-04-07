// ThreadServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "CServer.h"

int main()
{
	char pCommend[128];
	char ip[16] = "192.168.137.164";
	int nPort = 8080;
	string error;

	Server pServer;
	do
	{
		if (!pServer.Start(ip, nPort, 100, error))
		{
			cout << error.c_str() << endl;
			break;
		}
			while (true)
			{
				memset(pCommend, 0, sizeof(pCommend));
				cout << "请输入命令:";
				cin >> pCommend;
				string str = pCommend;
				if (str == "s")
				{
					break;
				}
				else if (str == "c")
				{
				cout << "客户端数量:" << pServer.GetClinetNum()<<endl;
				}
			}
	} while (false);
	pServer.Stop();
	return 0;
}

