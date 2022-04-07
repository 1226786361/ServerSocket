#include "CServer.h"

Server::Server()
{
	m_ServerSocket = INVALID_SOCKET;
	WSADATA data;
	// 第二个参数需要给地址
	WSAStartup(MAKEWORD(2, 2), &data);
}

Server::~Server()
{
	WSACleanup();
}

bool Server::Start(const char* ip, unsigned short port, unsigned int ClientNum, string& Error)
{
	bool nRef = false;
	do
	{
		if (m_isStartServer)
		{
			nRef = true;
			Error = "服务已经启动";
			break;
		}
		if (ip == 0 && port < 1024 || ClientNum == 0)
		{
			Error = "传入数据有误,请检查";
			break;
		}

		if (m_ServerSocket == INVALID_SOCKET)
		{
			m_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			if (m_ServerSocket == INVALID_SOCKET)
			{
				Error = "套接字创建失败";
				break;
			}
		}

		// 开始绑定
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(&sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.S_un.S_addr = inet_addr(ip);
		// 绑定地址以及端口号
		if (SOCKET_ERROR == bind(m_ServerSocket, (const struct sockaddr*)&sin, sizeof(sin)))
		{
			Error = "绑定服务失败";
			break;
		}
		// 启用监听，等待客户端发送connect请求
		if (SOCKET_ERROR == listen(m_ServerSocket, 200))
		{
			Error = "监听启动失败";
			break;
		}
		SetServerState(true);
		// 启动客户端链接监听线程
		m_pListenThread = CreateThread(0, 0, Server::_ListenFun, this, 0, 0);
		if (m_pListenThread == 0)
		{
			Error = "启动监听线程失败";
			break;
		}
		nRef = true;
	} while (false);

	// 跳出循环后判断是真还是假
	if (!nRef)
	{
		Stop();
	}

	return nRef;
}

void Server::Stop()
{
	// 如果是运行状态，才去关闭服务器
	if (IsServer())
	{
		SetServerState(false);
		shutdown(m_ServerSocket, SD_BOTH);
		Sleep(1);
		closesocket(m_ServerSocket);
		// 等待监听线程退出
		WaitForSingleObject(m_pListenThread, INFINITE);
		// 释放多线程客户端
		HANDLE* pThreadList = 0;
		int nThreadCount = 0;
		// 如果多线程存在才开始等待所有线程推出
		if (CloseAllClient(&pThreadList, nThreadCount))
		{
			WaitForMultipleObjects(nThreadCount, pThreadList, TRUE, INFINITE);
			delete[nThreadCount] pThreadList;
		}
	}

	// 清理所有客户端
	ClearAllClient();
	// 服务器是否开始
	m_isStartServer = false;
	// 清理线程数据
	m_pListenThread = 0;


}

bool Server::IsServer()
{
	bool nRef = false;
	m_pStateLock.lock();
	nRef = m_isStartServer;
	m_pStateLock.unlock();
	return nRef;
}

int Server::GetClinetNum()
{
	int nRef = 0;
	m_pClientLock.lock();
	nRef = m_pClientList.size();
	m_pClientLock.unlock();
	return nRef;
}

void Server::HandleListen()
{
	struct sockaddr_in sin;
	int nLen = 0;
	// 检查服务器是否为启动状态
	while (IsServer())
	{
		memset(&sin, 0, sizeof(sin));
		nLen = sizeof(sin);
		// 连接指定服务端
		SOCKET nClinet = accept(m_ServerSocket, (struct sockaddr*)&sin, &nLen);
		if (nClinet != INVALID_SOCKET)
		{
			bool isRef = false;
			// 截回数据
			ClientNode* pClientNode = 0;
			// 初始化句柄
			HANDLE pThread = 0;

			// 如果客户端插入成功了
			if (AppendClient(nClinet, &pClientNode))
			{
				// 准备启动线程，传入构建的结构数据
				ClientThread* nParam = new ClientThread(this, pClientNode);
				if (nParam != nullptr)
				{
					// 启动线程 创建线程
					pThread = CreateThread(0, 0, Server::_ReceiveFun, (LPVOID)nParam, 0, 0);
					if (pThread != nullptr)
					{
						if (SetClinetThread(nClinet, pThread))
						{
							isRef = true;
						}
					}
					else
					{
						// 线程启动失败了
						delete nParam;
					}
				}
			}
			// 根据返回值判定我们的线程启动方法是否正常(套接字启动成功了，但是线程没启动成功)
			if (!isRef)
			{
				shutdown(nClinet, SD_BOTH);
				closesocket(nClinet);
				// 线程已经存在了
				if (pThread != 0)
				{
					// 等待线程退出,怕SetClinetThread(nClinet, pThread)这个地方出问题
					WaitForSingleObject(pThread, 10000);
				}
				RemoveClient(nClinet);
			}
		}

	}
}



void Server::HandleClient(ClientNode* pClient)
{
	// 防止整个对象被移除
	SOCKET nClient = pClient->nClinet;
	int nRef = 0;
	char* pBuffer = new char[RECEIVE_MAX_BUF];
	// 循环收取数据
	while (nRef >= 0 && pBuffer != nullptr)
	{
		memset(pBuffer, RECEIVE_MAX_BUF, 0);
		// 接收数据
		nRef = recv(nClient, pBuffer, RECEIVE_MAX_BUF, 0);
		if (nRef > 0)
		{
			// 插入数据的时候可能在读取数据，所以要特殊处理
			//将数据放入缓存
			AppendClientBuffer(nClient, pBuffer);
		}
	}
	RemoveClient(nClient);
	if (pBuffer != nullptr)
	{
		delete[RECEIVE_MAX_BUF] pBuffer;
	}
}

void Server::SetServerState(bool on)
{
	bool nRef = false;
	m_pStateLock.lock();
	m_isStartServer = on;
	m_pStateLock.unlock();
}

bool Server::CloseAllClient(HANDLE** pThredList, int nCount)
{
	// 记录所有的线程
	HANDLE* pList = 0;
	int index = 0;
	// 客户端再关闭线程的时候不能被别的事情打扰
	m_pClientLock.lock();
	if (m_pClientList.size() > 0)
	{
		// new一个同类型的是数组
		pList = new HANDLE[m_pClientList.size()];
		// 遍历所有线程依次关闭
		map<SOCKET, ClientNode*>::iterator it = m_pClientList.begin();
		for (; it != m_pClientList.end(); ++it)
		{
			SOCKET key = it->first;
			shutdown(key, SD_BOTH);
			closesocket(key);
			// 将线程添加至数组
			pList[index++] = it->second->pThread;
		}
	}
	m_pClientLock.unlock();
	*pThredList = pList;
	nCount = index;

	return pList != 0 ? false : true;
}

bool Server::AppendClient(SOCKET nClient, ClientNode** pClientNode)
{
	ClientNode* pNode = 0;
	int isRef = false;
	m_pClientLock.lock();
	// 检查有没有这个Key值
	if (m_pClientList.count(nClient) == 0)
	{
		// 增加这个Key
		pNode = new ClientNode(0, nClient);
		// 对象是否开辟成功
		if (pNode != 0)
		{
			// 给Key赋值
			m_pClientList.insert(make_pair(nClient, pNode));
			// 再次检查是否追加成功
			if (m_pClientList.count(nClient) != 0)
			{
				isRef = true;
			}
		}
	}
	m_pClientLock.unlock();
	// 对象在堆中开辟失败
	if (!isRef && pNode != 0)
	{
		delete pNode;
		pNode = 0;
	}
	// 不管成不成功都要拿上结构中的数据
	*pClientNode = pNode;
	return isRef;
}

bool Server::SetClinetThread(SOCKET nClient, HANDLE pTheread)
{
	bool isRef = false;
	m_pClientLock.lock();
	if (m_pClientList.count(nClient) != 0)
	{
		map<SOCKET, ClientNode*>::iterator it = m_pClientList.find(nClient);
		if (it != m_pClientList.end())
		{
			// 将此Key的Value赋值,值为线程
			it->second->pThread = pTheread;
			isRef = true;
		}
	}
	m_pClientLock.unlock();
	return isRef;
}

bool Server::AppendClientBuffer(SOCKET nClinet, const char* pBuffer)
{
	bool isRef = false;
	m_pClientLock.lock();
	// 数据读取
	if (m_pClientList.count(nClinet) != 0 && pBuffer != nullptr)
	{
		map<SOCKET, ClientNode*>::iterator it = m_pClientList.find(nClinet);
		if (it != m_pClientList.end())
		{
			it->second->pTextList.push_back(string(pBuffer));
			isRef = true;
		}
	}
	m_pClientLock.unlock();
	return isRef;
}

void Server::RemoveClient(SOCKET nClient)
{
	m_pClientLock.lock();
	if (m_pClientList.count(nClient) != 0)
	{
		map<SOCKET, ClientNode*>::iterator valueit = m_pClientList.find(nClient);
		// second为value值
		ClientNode* pNode = (*valueit).second;
		pNode->pTextList.clear();
		delete pNode;
		// erase为删除
		m_pClientList.erase(nClient);
	}
}

void Server::ClearAllClient()
{
	if (m_pClientList.size() > 0)
	{
		// 遍历所有线程依次关闭
		map<SOCKET, ClientNode*>::iterator it = m_pClientList.begin();
		for (; it != m_pClientList.end(); ++it)
		{
			ClientNode* pNode = it->second;

			if (pNode != 0)
			{
				pNode->pTextList.clear();
				delete pNode;
			}
		}
	}
	m_pClientList.clear();
}

DWORD WINAPI Server::_ListenFun(LPVOID Args)
{
	Server* pThis = (Server*)Args;
	pThis->HandleListen();
	return 0;
}

DWORD WINAPI Server::_ReceiveFun(LPVOID Args)
{
	ClientThread* nParam = static_cast<ClientThread*>(Args);
	if (nParam != nullptr && nParam->pClientNode != 0)
	{
		nParam->pThis->HandleClient(nParam->pClientNode);
		delete nParam;
	}
	return 0;
}
