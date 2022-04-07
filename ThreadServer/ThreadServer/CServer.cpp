#include "CServer.h"

Server::Server()
{
	m_ServerSocket = INVALID_SOCKET;
	WSADATA data;
	// �ڶ���������Ҫ����ַ
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
			Error = "�����Ѿ�����";
			break;
		}
		if (ip == 0 && port < 1024 || ClientNum == 0)
		{
			Error = "������������,����";
			break;
		}

		if (m_ServerSocket == INVALID_SOCKET)
		{
			m_ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			if (m_ServerSocket == INVALID_SOCKET)
			{
				Error = "�׽��ִ���ʧ��";
				break;
			}
		}

		// ��ʼ��
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(&sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.S_un.S_addr = inet_addr(ip);
		// �󶨵�ַ�Լ��˿ں�
		if (SOCKET_ERROR == bind(m_ServerSocket, (const struct sockaddr*)&sin, sizeof(sin)))
		{
			Error = "�󶨷���ʧ��";
			break;
		}
		// ���ü������ȴ��ͻ��˷���connect����
		if (SOCKET_ERROR == listen(m_ServerSocket, 200))
		{
			Error = "��������ʧ��";
			break;
		}
		SetServerState(true);
		// �����ͻ������Ӽ����߳�
		m_pListenThread = CreateThread(0, 0, Server::_ListenFun, this, 0, 0);
		if (m_pListenThread == 0)
		{
			Error = "���������߳�ʧ��";
			break;
		}
		nRef = true;
	} while (false);

	// ����ѭ�����ж����滹�Ǽ�
	if (!nRef)
	{
		Stop();
	}

	return nRef;
}

void Server::Stop()
{
	// ���������״̬����ȥ�رշ�����
	if (IsServer())
	{
		SetServerState(false);
		shutdown(m_ServerSocket, SD_BOTH);
		Sleep(1);
		closesocket(m_ServerSocket);
		// �ȴ������߳��˳�
		WaitForSingleObject(m_pListenThread, INFINITE);
		// �ͷŶ��߳̿ͻ���
		HANDLE* pThreadList = 0;
		int nThreadCount = 0;
		// ������̴߳��ڲſ�ʼ�ȴ������߳��Ƴ�
		if (CloseAllClient(&pThreadList, nThreadCount))
		{
			WaitForMultipleObjects(nThreadCount, pThreadList, TRUE, INFINITE);
			delete[nThreadCount] pThreadList;
		}
	}

	// �������пͻ���
	ClearAllClient();
	// �������Ƿ�ʼ
	m_isStartServer = false;
	// �����߳�����
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
	// ���������Ƿ�Ϊ����״̬
	while (IsServer())
	{
		memset(&sin, 0, sizeof(sin));
		nLen = sizeof(sin);
		// ����ָ�������
		SOCKET nClinet = accept(m_ServerSocket, (struct sockaddr*)&sin, &nLen);
		if (nClinet != INVALID_SOCKET)
		{
			bool isRef = false;
			// �ػ�����
			ClientNode* pClientNode = 0;
			// ��ʼ�����
			HANDLE pThread = 0;

			// ����ͻ��˲���ɹ���
			if (AppendClient(nClinet, &pClientNode))
			{
				// ׼�������̣߳����빹���Ľṹ����
				ClientThread* nParam = new ClientThread(this, pClientNode);
				if (nParam != nullptr)
				{
					// �����߳� �����߳�
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
						// �߳�����ʧ����
						delete nParam;
					}
				}
			}
			// ���ݷ���ֵ�ж����ǵ��߳����������Ƿ�����(�׽��������ɹ��ˣ������߳�û�����ɹ�)
			if (!isRef)
			{
				shutdown(nClinet, SD_BOTH);
				closesocket(nClinet);
				// �߳��Ѿ�������
				if (pThread != 0)
				{
					// �ȴ��߳��˳�,��SetClinetThread(nClinet, pThread)����ط�������
					WaitForSingleObject(pThread, 10000);
				}
				RemoveClient(nClinet);
			}
		}

	}
}



void Server::HandleClient(ClientNode* pClient)
{
	// ��ֹ���������Ƴ�
	SOCKET nClient = pClient->nClinet;
	int nRef = 0;
	char* pBuffer = new char[RECEIVE_MAX_BUF];
	// ѭ����ȡ����
	while (nRef >= 0 && pBuffer != nullptr)
	{
		memset(pBuffer, RECEIVE_MAX_BUF, 0);
		// ��������
		nRef = recv(nClient, pBuffer, RECEIVE_MAX_BUF, 0);
		if (nRef > 0)
		{
			// �������ݵ�ʱ������ڶ�ȡ���ݣ�����Ҫ���⴦��
			//�����ݷ��뻺��
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
	// ��¼���е��߳�
	HANDLE* pList = 0;
	int index = 0;
	// �ͻ����ٹر��̵߳�ʱ���ܱ�����������
	m_pClientLock.lock();
	if (m_pClientList.size() > 0)
	{
		// newһ��ͬ���͵�������
		pList = new HANDLE[m_pClientList.size()];
		// ���������߳����ιر�
		map<SOCKET, ClientNode*>::iterator it = m_pClientList.begin();
		for (; it != m_pClientList.end(); ++it)
		{
			SOCKET key = it->first;
			shutdown(key, SD_BOTH);
			closesocket(key);
			// ���߳����������
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
	// �����û�����Keyֵ
	if (m_pClientList.count(nClient) == 0)
	{
		// �������Key
		pNode = new ClientNode(0, nClient);
		// �����Ƿ񿪱ٳɹ�
		if (pNode != 0)
		{
			// ��Key��ֵ
			m_pClientList.insert(make_pair(nClient, pNode));
			// �ٴμ���Ƿ�׷�ӳɹ�
			if (m_pClientList.count(nClient) != 0)
			{
				isRef = true;
			}
		}
	}
	m_pClientLock.unlock();
	// �����ڶ��п���ʧ��
	if (!isRef && pNode != 0)
	{
		delete pNode;
		pNode = 0;
	}
	// ���ܳɲ��ɹ���Ҫ���Ͻṹ�е�����
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
			// ����Key��Value��ֵ,ֵΪ�߳�
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
	// ���ݶ�ȡ
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
		// secondΪvalueֵ
		ClientNode* pNode = (*valueit).second;
		pNode->pTextList.clear();
		delete pNode;
		// eraseΪɾ��
		m_pClientList.erase(nClient);
	}
}

void Server::ClearAllClient()
{
	if (m_pClientList.size() > 0)
	{
		// ���������߳����ιر�
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
