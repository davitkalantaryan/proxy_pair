#
# File				: proxy_pair_client.pro
# File created		: 2020 Jul 10
# Created by		: Davit Kalantaryan (davit.kalantaryan@desy.de)
#


message("file:  proxy_pair_client.pro  ")

include( $${PWD}/../../common/common_qt/sys_common.pri )

DEFINES += BBS_USING_STATIC_LIB_OR_OBJECTS

QT -= gui
QT -= widgets
#CONFIG -= qt
QT += core

INCLUDEPATH +=	$${PWD}/../../../include

win32{
	LIBS += -lWs2_32
	LIBS += -lAdvapi32
	CONFIG += console
	SOURCES +=		\
		$${PWD}/../../../src/core/windows_serviceentry_emp_psa_service.cpp
} else {
	LIBS += -pthread
	SOURCES +=		\
}

SOURCES		+=		\


HEADERS		+=		\
	

OTHER_FILES	+=		\
