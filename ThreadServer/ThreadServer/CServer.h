#ifndef _CSERVER_H
#define _CSERVER_H
#include <iostream>
#include <WinSock2.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <mutex> //线程互斥需要包含此头文件
#pragma comment(lib,"ws2_32.lib")
using namespace std;
#endif

class Server
{
private:
	enum
	{
		// 可以接受的最大缓冲区
		RECEIVE_MAX_BUF = 4096,
	};
	// 用于记录客户端套接字节点
	struct ClientNode
	{
		// 客户端套接字
		SOCKET nClinet;
		// 记录线程句柄
		HANDLE pThread;
		// 用来记录客户端的消息列表
		list<string> pTextList;
		ClientNode() : pThread(0),nClinet(INVALID_SOCKET) {};
		ClientNode(HANDLE pHandle, SOCKET nSocket) : pThread(pHandle),nClinet(nSocket) {};
	};

private:
	// 给客户端接收线程提供的节点信息
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
	// 启动服务端Start(ip地址,端口号,最多接受客户端的数量);
	bool Start(const char* ip,unsigned short port,unsigned int ClientNum,string& Error);
	// 终止服务端的服务
	void Stop();
	// 获取服务状态
	bool IsServer();

private:
	// 设置服务状态
	void SetServerState(bool on);
	// 处理监听工作
	void HandleListen();
	// 处理客户端接收工作(用于知道说哪个客户端)
	void HandleClient(ClientNode* pClient);

public:
	// 获取客户端数量
	int GetClinetNum();

private:
	// 关闭所有客户端
	bool CloseAllClient(HANDLE** pThredList, int nCount);
	// 添加新客户端(客户端套接字,结构体)
	bool AppendClient(SOCKET nClient,ClientNode** pNode);
	// 为已有客户端添加他的套接字
	bool SetClinetThread(SOCKET nClient,HANDLE pTheread);
	// 为客户端添加收到的数据
	bool AppendClientBuffer(SOCKET nClinet,const char* pBuffer);
	// 删除指定的客户端
	void RemoveClient(SOCKET nClient);
	// 清理客户端
	void ClearAllClient();

private:
// 线程函数执行体
static DWORD WINAPI _ListenFun(LPVOID Args);
// 处理客户端的接收工作
static DWORD WINAPI _ReceiveFun(LPVOID Args);

private:
	// 用于保护服务状态的
	mutex m_pStateLock;
	// 用于保护客户端列表
	mutex m_pClientLock;
private:
	// 服务器套接字
	SOCKET m_ServerSocket;
	// 检测服务是否已经启动
	bool m_isStartServer;
	// 连接服务器的客户端数量
	map<SOCKET,ClientNode*> m_pClientList;
	// 专门记录监听线程的句柄
	HANDLE m_pListenThread;
};

