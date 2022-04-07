#ifndef _CSERVER_H
#define _CSERVER_H
#include <iostream>
#include <WinSock2.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <mutex> //�̻߳�����Ҫ������ͷ�ļ�
#pragma comment(lib,"ws2_32.lib")
using namespace std;
#endif

class Server
{
private:
	enum
	{
		// ���Խ��ܵ���󻺳���
		RECEIVE_MAX_BUF = 4096,
	};
	// ���ڼ�¼�ͻ����׽��ֽڵ�
	struct ClientNode
	{
		// �ͻ����׽���
		SOCKET nClinet;
		// ��¼�߳̾��
		HANDLE pThread;
		// ������¼�ͻ��˵���Ϣ�б�
		list<string> pTextList;
		ClientNode() : pThread(0),nClinet(INVALID_SOCKET) {};
		ClientNode(HANDLE pHandle, SOCKET nSocket) : pThread(pHandle),nClinet(nSocket) {};
	};

private:
	// ���ͻ��˽����߳��ṩ�Ľڵ���Ϣ
	struct ClientThread
	{
		Server* pThis;
		ClientNode* pClientNode;
		ClientThread(Server* pClass, ClientNode* pNode) : pThis(pClass), pClientNode(pNode) {};
	};
public:
	Server();
	~Server();
public:
	// ���������Start(ip��ַ,�˿ں�,�����ܿͻ��˵�����);
	bool Start(const char* ip,unsigned short port,unsigned int ClientNum,string& Error);
	// ��ֹ����˵ķ���
	void Stop();
	// ��ȡ����״̬
	bool IsServer();

private:
	// ���÷���״̬
	void SetServerState(bool on);
	// �����������
	void HandleListen();
	// ����ͻ��˽��չ���(����֪��˵�ĸ��ͻ���)
	void HandleClient(ClientNode* pClient);

public:
	// ��ȡ�ͻ�������
	int GetClinetNum();

private:
	// �ر����пͻ���
	bool CloseAllClient(HANDLE** pThredList, int nCount);
	// ����¿ͻ���(�ͻ����׽���,�ṹ��)
	bool AppendClient(SOCKET nClient,ClientNode** pNode);
	// Ϊ���пͻ�����������׽���
	bool SetClinetThread(SOCKET nClient,HANDLE pTheread);
	// Ϊ�ͻ�������յ�������
	bool AppendClientBuffer(SOCKET nClinet,const char* pBuffer);
	// ɾ��ָ���Ŀͻ���
	void RemoveClient(SOCKET nClient);
	// ����ͻ���
	void ClearAllClient();

private:
// �̺߳���ִ����
static DWORD WINAPI _ListenFun(LPVOID Args);
// ����ͻ��˵Ľ��չ���
static DWORD WINAPI _ReceiveFun(LPVOID Args);

private:
	// ���ڱ�������״̬��
	mutex m_pStateLock;
	// ���ڱ����ͻ����б�
	mutex m_pClientLock;
private:
	// �������׽���
	SOCKET m_ServerSocket;
	// �������Ƿ��Ѿ�����
	bool m_isStartServer;
	// ���ӷ������Ŀͻ�������
	map<SOCKET,ClientNode*> m_pClientList;
	// ר�ż�¼�����̵߳ľ��
	HANDLE m_pListenThread;
};

