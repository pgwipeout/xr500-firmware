// PHGlobal.h: interface for the CBaseThread class.
//
//////////////////////////////////////////////////////////////////////
/*! \file PHGlobal.h
*  \author skyvense
*  \date   2009-09-14
*  \brief PHDDNS �ͻ���ʵ��	
*/

#ifndef __PHGLOBAL__H__
#define __PHGLOBAL__H__
#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#endif

#ifdef WIN32
#include <winsock.h>
#define sleep(x) Sleep(x*1000)
#endif

#include <list>
#include <string>

using namespace std;
#define MAX_TCP_PACKET_LEN	8192

//! �ͻ���״̬
enum MessageCodes
{
	okConnected = 0,
	okAuthpassed,
	okDomainListed,
	okDomainsRegistered,
	okKeepAliveRecved,
	okConnecting,
	okRetrievingMisc,
	okRedirecting,

	errorConnectFailed = 0x100,
	errorSocketInitialFailed,
	errorAuthFailed,
	errorDomainListFailed,
	errorDomainRegisterFailed,
	errorUpdateTimeout,
	errorKeepAliveError,
	errorRetrying,
	errorAuthBusy,
	errorStatDetailInfoFailed,
	

	okNormal = 0x120,
	okNoData,
	okServerER,
	errorOccupyReconnect,
};

//! TCPָ���
#define COMMAND_AUTH	"auth router6\r\n"
#define COMMAND_REGI    "regi a"
#define COMMAND_CNFM    "cnfm\r\n"
#define COMMAND_STAT_USER    "stat user\r\n"
#define COMMAND_STAT_DOM     "stat domain\r\n"
#define COMMAND_QUIT    "quit\r\n"

//! ����������ָ��
#define UDP_OPCODE_UPDATE_VER2		0x2010
//! ��������������������
#define UDP_OPCODE_UPDATE_OK		0x2050
//! ���������������ش���
#define UDP_OPCODE_UPDATE_ERROR		1000

//! ������ע����¼
#define UDP_OPCODE_LOGOUT			11

//! ���������Ĵ�С
#define KEEPALIVE_PACKET_LEN	20

//! ת��״̬�뵽�ı���
const char *convert_status_code(int nCode);

//! ת��IP��ַ���ı���
const char *my_inet_ntoa(int ip);

//! �������ṹ
struct DATA_KEEPALIVE 
{
	//! �Ի�ID
	int lChatID;
	//! ������
	int lOpCode;
	//! ��ID
	int lID;
	//! У���
	int lSum;
	//! ������
	int lReserved;
};

//! ���°���չ�ṹ�����ڷ���������ʱIP��ַ
struct DATA_KEEPALIVE_EXT
{
	DATA_KEEPALIVE keepalive;
	int ip;
};

//! ȫ�ֱ���
struct PHGlobal
{
	PHGlobal();
	~PHGlobal();
//basic system info
	//! Ƕ��ʽ�ͻ�����Ϣ��4λ�ͻ���ID + 4λ�汾��
	long clientinfo;
	//! Ƕ��ʽ��֤��
	long challengekey;

	//! ��������ַ
	char szHost[256];
	//! �������˿ڣ�6060�̶�
	short nPort;
	//! �û��˺���
	char szUserID[256];
	//! �û���������
	char szUserPWD[256];
	//! ���ذ󶨵�ַ��Ĭ����գ����ڶ������ʱָ������
	char szBindAddress[256];
//run-time controll variables
	//! �û�����
	int nUserType;
	bool bTcpUpdateSuccessed;
	char szChallenge[256];
	int nChallengeLen;
	int nChatID,nStartID,nLastResponseID;
	int tmLastResponse;
	int nAddressIndex;
	char szTcpConnectAddress[32];

	int cLastResult;
	int ip;

	int uptime;
	int lasttcptime;

	char szActiveDomains[255][255];

	std::string szUserInfo;
	std::string szDomainInfo;

	void init();
};


#endif