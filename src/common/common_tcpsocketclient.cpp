
#include <common/localtcpsocketclient.hpp>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

using namespace common; 

LocalTcpClient::LocalTcpClient()
{
	//
}

LocalTcpClient::~LocalTcpClient()
{
}


int LocalTcpClient::ConnectToServerC(int a_port, int a_connectionTimeoutMs, const char* a_svrName)
{
	int nReturn;
	struct sockaddr_in saddr;
	fd_set rwfds;
	struct timeval  aTimeout;
	struct timeval* pTimeout;
	int maxsd;
	unsigned long ha;
#ifdef	_WIN32
	u_long on;
#else  /* #ifdef	WIN32 */
	int status;
#endif  /* #ifdef	WIN32 */

	if(!a_svrName){
		a_svrName = "localhost";
	}

	// 1. create socket
	m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (CHECK_FOR_SOCK_INVALID(m_socket)) {
		m_socket = -1;
		return -1;
	}

	// 2. If hostname provided, then make DNS query
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable:4996)
#endif
	if ((ha = inet_addr(a_svrName)) == INADDR_NONE) {
		struct hostent* hostent_ptr = gethostbyname(a_svrName);
		if (!hostent_ptr) { goto errorReturn; }
		a_svrName = inet_ntoa(*(struct in_addr*)hostent_ptr->h_addr_list[0]);
		if ((ha = inet_addr(a_svrName)) == INADDR_NONE) { goto errorReturn; }
#ifdef _MSC_VER
#pragma warning (pop)
#endif
	}

	// 3. prepare saddr
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(a_port);
	memcpy(&saddr.sin_addr, &ha, sizeof(ha));

	// 4. temporary make socket non blocking
#ifdef	_WIN32
	on = 1;
	ioctlsocket(m_socket, FIONBIO, &on);
#else  /* #ifdef	WIN32 */
	if ((status = fcntl(m_socket, F_GETFL, 0)) != -1) {
		status |= O_NONBLOCK;
		fcntl(m_socket, F_SETFL, status);
	}
#endif  /* #ifdef	_WIN32 */
	
	// 5. Initialize select properties
	FD_ZERO(&rwfds);
	FD_SET(m_socket, &rwfds);
	maxsd = static_cast<int>(m_socket) + 1;
	if (a_connectionTimeoutMs >= 0) {
		aTimeout.tv_sec = a_connectionTimeoutMs / 1000L;
		aTimeout.tv_usec = (a_connectionTimeoutMs % 1000L) * 1000L;
		pTimeout = &aTimeout;
	}
	else { pTimeout = nullptr; }

	// 6. connect to server
	nReturn = ::connect(m_socket, reinterpret_cast<struct sockaddr*>( & saddr), sizeof(saddr));
	if(!nReturn){ // we are succesfully connected
		goto goodReurnPoint;
	}

	// 7. If no connection, then check whethe pending connection is there
	if (!SOCKET_INPROGRESS(errno)) { goto errorReturn; }

	// 8. lets use select to wait to connect
	nReturn = ::select(maxsd,nullptr,&rwfds,nullptr,pTimeout);
	if(nReturn<=0){
		goto errorReturn;
	}
	if (!FD_ISSET(m_socket, &rwfds)){goto errorReturn;}
		
goodReurnPoint:
	// 9. let's get 2 Bytes from server, as a confirmation
	//FD_ZERO(&rwfds);
	//FD_SET(m_socket, &rwfds);
	//nReturn = ::select(maxsd,&rwfds,nullptr,nullptr,pTimeout);
	//if(nReturn<=0){
	//	goto errorReturn;
	//}
	//if (!FD_ISSET(m_socket, &rwfds)){goto errorReturn;}
	//nReturn = recv(m_socket,vcOk,2,0);
	//if((nReturn!=2)||(vcOk[0]!='o')||(vcOk[1]!='k')){
	//	goto errorReturn;
	//}
	
	
	// 10. make socket again blocking
#ifdef	_WIN32
	on = 0;
	ioctlsocket(m_socket, FIONBIO, &on);
#else  /* #ifdef	WIN32 */
	if ((status = fcntl(m_socket, F_GETFL, 0)) != -1) {
		status &= ~O_NONBLOCK;
		fcntl(m_socket, F_SETFL, status);
	}
#endif  /* #ifdef	_WIN32 */
	return 0;
errorReturn:
	CloseC();
	return -1;
}
