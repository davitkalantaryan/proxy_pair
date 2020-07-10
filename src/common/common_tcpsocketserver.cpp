

//class NamedPipeServer 
//{
//public:
//	NamedPipeServer();
//	virtual ~NamedPipeServer(){}
//
//	int CreateServer(const char* pipe_name);
//	int WaitForConnection()const;
//
//};

#include <common/localtcpsocketserver.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bbs_client_server_common.h>
#ifdef _WIN32
#else
#endif

namespace common{

struct LocalTcpListItem {
	LocalTcpListItem(){this->prev=this->next=nullptr;this->server=nullptr;this->serverSock=INVALID_SOCKET;}
	LocalTcpListItem	*prev, *next;
	LocalTcpServer*		server;
	SOCKET_T			serverSock;
};

} // namespace common {

union{
	struct{
		uint64_t	exitStarted : 1;
		uint64_t	serversStopped : 1;
		uint64_t	cleaned2 : 1;
	}bit;
	uint64_t	allBits;
}static s_statuses;

//static struct sigaction s_oldSigIintAction;

static ::common::MUTEX_T	s_mutexForList;
static ::common::LocalTcpListItem*	s_pFirstServer = nullptr;
static void ServerInitializerRoutine (void);
static void ServersCleaupRoutine(void);

struct ServerInitializer {
	ServerInitializer(){ServerInitializerRoutine();}
	~ServerInitializer(){ServersCleaupRoutine();}
}static s_serverInitializer;

static void NewPeerDummy(void*, common::TcpSocketPeerForServer& a_peer)
{
	a_peer.CloseC();
}

using namespace common;

void TcpSocketPeerForServer::CloseC()
{
	TcpSocketPeer::CloseC();
}


/**/

LocalTcpServer::LocalTcpServer()
{
	m_pItem = new LocalTcpListItem;
	m_serverThreadHandle = 0;
	m_statuses.allBits = 0;
	m_statuses.bit.shouldRun = 1;
	m_pItem->server = this;
	m_pItem->serverSock = -1;

	:: PTHREAT_MUTEX_LOCK(s_mutexForList);
	
	if(BBS_LIKELY(s_pFirstServer)){
		s_pFirstServer->prev=m_pItem;
	}
	m_pItem->next = s_pFirstServer;
	s_pFirstServer = m_pItem;
	
	:: PTHREAT_MUTEX_UNLOCK(s_mutexForList);
}


LocalTcpServer::~LocalTcpServer()
{
	//printf("detrctor!\n");

	::PTHREAT_MUTEX_LOCK(s_mutexForList);

	if(m_pItem){

		if (s_pFirstServer == m_pItem) {
			s_pFirstServer = m_pItem->next;
		}
		if (m_pItem->prev) {
			m_pItem->prev->next = m_pItem->next;
		}
		if (m_pItem->next) {
			m_pItem->next->prev = m_pItem->prev;
		}
	}

	::PTHREAT_MUTEX_UNLOCK(s_mutexForList);
}


void LocalTcpServer::StopThisFromSignalContext(bool a_bWait)
{
	if(m_statuses.bit.shouldRun){
		m_statuses.bit.shouldRun=0;
		
#if 1

		if (m_pItem && (m_pItem->serverSock >= 0)) {
			closesocket(m_pItem->serverSock);
			m_pItem->serverSock = -1;
		}
		
		if (a_bWait) {
			m_statuses.bit.serverThreadFinished = 0;
		}
		m_statuses.bit.serverThreadStarted = 0;

#else 
		if(!m_statuses.bit.serverThreadFinished){
			PTHREAD_T thisThread = pthread_self();
			if((thisThread!=m_serverThreadHandle)&&m_serverThreadHandle){
#ifdef _WIN32
				if(m_statuses.bit.serverThreadStarted){
					QueueUserAPC([](ULONG_PTR) {},m_serverThreadHandle,0);
				}
				else{
					TerminateThread(m_serverThreadHandle,0);
				}
				if(a_bWait){
					WaitForSingleObject(m_serverThreadHandle,INFINITE);
					CloseHandle(m_serverThreadHandle);
				}
#else
				pthread_kill(m_serverThreadHandle,SIGPIPE);
				if(a_bWait){pthread_join(m_serverThreadHandle,nullptr);}
#endif
				if (a_bWait) {
					m_statuses.bit.serverThreadFinished = 0;
				}
				m_statuses.bit.serverThreadStarted = 0;
			}
			else {
				if(m_pItem && (m_pItem->serverSock>=0)){
					closesocket(m_pItem->serverSock);
					m_pItem->serverSock = -1;
				}
			}
		}
#endif  // #if 0
	}	
}


int LocalTcpServer::CreateServerAndWaitForConnections(int a_nPort, void* a_clbkData, TypeNewPeer a_clbkFnc)
{
	int nReturn;
	struct sockaddr_in saddr;
	SOCKET_T msgsock;
	int option;
	TcpSocketPeerForServer clbkPipe;
//#ifdef	_WIN32
//	u_long blockingMode;
//#else
//	int  blockingMode;
//#endif

	// 1. Let's create socket
	m_pItem->serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (CHECK_FOR_SOCK_INVALID(m_pItem->serverSock)) { return(-1); }

	// 2. let's make port reusable
	option = 1;
	setsockopt(m_pItem->serverSock, SOL_SOCKET, SO_REUSEADDR,reinterpret_cast<const char*>(&option),sizeof(option));

	// 3. prepare socket address
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(a_nPort);
	//saddr.sin_addr.s_addr = htonl((a_bLoopback ? INADDR_LOOPBACK : INADDR_ANY));
	// we do not wait connection from any host
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	// 4. let's make server socket non blocking
//#ifdef	_WIN32
//	blockingMode = 1;
//	ioctlsocket(serverSocket, FIONBIO, &blockingMode);
//#else
//	if ((blockingMode = fcntl(m_socket, F_GETFL, 0)) != -1)
//	{
//		blockingMode |= O_NONBLOCK;
//		fcntl(m_socket, F_SETFL, blockingMode);
//	}
//#endif

	// 5. Now let's bind to the port
	nReturn = bind(m_pItem->serverSock,reinterpret_cast<struct sockaddr*>(& saddr), sizeof(saddr));
	if (CHECK_FOR_SOCK_ERROR(nReturn)) { goto errorReturnPoint; }

	// 6. Listen to incoming connections
	nReturn = ::listen(m_pItem->serverSock, 1);  // queue size 1 is enought
	if (CHECK_FOR_SOCK_ERROR(nReturn)) { goto errorReturnPoint; }

	m_serverThreadHandle = pthread_self();
	
	// 7. finally let's start loop of accept
	a_clbkFnc = a_clbkFnc?a_clbkFnc:NewPeerDummy;
	while(m_statuses.bit.shouldRun){
		msgsock = accept(m_pItem->serverSock, nullptr, 0);
		if (msgsock == -1){
			perror("accept");
			continue;
		}
		clbkPipe.SetPipeFromNativeSocket(msgsock);
		(*a_clbkFnc)(a_clbkData,clbkPipe);
		clbkPipe.CloseC();
	}
	
	nReturn = 0;
returnPoint2:
	if(m_pItem && (m_pItem->serverSock >=0)){
		closesocket(m_pItem->serverSock);
		m_pItem->serverSock = -1;
	}
	m_statuses.bit.serverThreadFinished = 1;
	return nReturn;

errorReturnPoint:
	nReturn = -1;
	goto returnPoint2;
	return -1;
}


static void SigInterruptFunction(int)
{
	printf("SigInterrupt stopping all servers!\n");
	ServersCleaupRoutine();
}


static void ServersCleaupRoutine(void)
{
	LocalTcpListItem* pItem, * pItemNext;

	PTHREAT_MUTEX_LOCK(s_mutexForList);
	s_statuses.bit.exitStarted = 1;
	pItem = s_pFirstServer;

	while (pItem) {
		pItemNext = pItem->next;
		pItem->server->StopThisFromSignalContext(true);
		pItem->server->m_pItem = BBS_NULL;
		if (pItem->serverSock >= 0) {
			closesocket(pItem->serverSock);
			pItem->serverSock = -1;
		}
		delete pItem; 
		pItem = pItemNext;
	}

	PTHREAT_MUTEX_UNLOCK(s_mutexForList);
}


static void ServerInitializerRoutine(void)
{
#ifdef _WIN32
	signal(SIGINT, SigInterruptFunction);
#else
	struct sigaction newSigAction;
	
	s_statuses.allBits = 0;
	pthread_mutex_init(&s_mutexForList,nullptr);

	memset(&newSigAction,0,sizeof(struct sigaction));
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;
	
	newSigAction.sa_handler2 = [](int){
		printf("SIGPIPE!\n");
	};
	sigaction(SIGPIPE,&newSigAction,nullptr);
	
	newSigAction.sa_handler2 = &SigInterruptFunction;
	//sigaction(SIGINT,&newSigAction,&s_oldSigIintAction);
	newSigAction.sa_flags = SA_RESETHAND;
	sigaction(SIGINT,&newSigAction,nullptr);
#endif
}
