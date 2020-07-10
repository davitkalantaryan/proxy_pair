/*
 *	File: localtcpsocketbase.cpp
 *
 *	Created on: Sep 24, 2016
 *	Author    : Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 * 
 *  mainURL:	https://linux.die.net/man/3/mkfifo
 *
 *  This file implements all functions ...
 *		1) ...
 *		2) ...
 *		...
 *
 *
 */


#include <common/localtcpsocketbase.hpp>
#include <sys/types.h>
#include <sys/stat.h>


using namespace common;

static void SocketBaseInitializerRoutine(void);
static void SocketBaseCleanupRoutine(void);

struct SocketBaseInitializer {
	SocketBaseInitializer() { SocketBaseInitializerRoutine(); }
	~SocketBaseInitializer() { SocketBaseCleanupRoutine(); }
}static s_socketBaseInitializer;


TcpSocketPeer::TcpSocketPeer()
{
	m_socket = INVALID_SOCKET;
}


TcpSocketPeer::~TcpSocketPeer()
{
	CloseC();
}


void TcpSocketPeer::CloseC()
{
	if(m_socket!=INVALID_SOCKET){
		:: closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
}


int TcpSocketPeer::readC(void* a_buffer, size_t a_buffer_length)const
{
	return :: recv(m_socket,static_cast<char*>(a_buffer),static_cast<int>(a_buffer_length), MSG_WAITALL);
}


int TcpSocketPeer::writeC(const void* a_buffer, size_t a_buffer_length)const
{
	return :: send(m_socket,static_cast<const char*>(a_buffer),static_cast<int>(a_buffer_length),0);
}


/*/////////////////////////////////////////////*/

static void SocketBaseInitializerRoutine(void)
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0){
		return ;
	}

	/* Confirm that the WinSock DLL supports 2.2.		*/
	/* Note that if the DLL supports versions greater	*/
	/* than 2.2 in addition to 2.2, it will still return*/
	/* 2.2 in wVersion since that is the version we		*/
	/* requested.										*/

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2){
		WSACleanup();
		return ;
	}

#endif
}


static void SocketBaseCleanupRoutine(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}
