// phupdate.h: interface for the CBaseThread class.
//
//////////////////////////////////////////////////////////////////////
/*! \file phupdate.h
*  \author skyvense
*  \date   2009-09-14
*  \brief PHDDNS �ͻ���ʵ��	
*/

#ifndef _CUPDATECORE_H
#define _CUPDATECORE_H

#include "common/PHSocket.h"
#include "PHGlobal.h"

//! ������DDNS�ͻ���ʵ�ֻ���
/*!
*/
class CUpdateBase
{
public:
	//! ����, ��ʼ����Ҫ�ı���
	CUpdateBase()
	{
		bNeed_connect = true;
		tmLastSend = 0;
	}
	
	//! �����������������������ʱ��Ϣ
	PHGlobal phglobal;
	
	//! �������ã����úò�������Ҫ��������˺��������������´���Ҫִ�б�������ʱ�䣨������
	int step();	

	//! ֹͣ������DDNS���£��������ò�����ɽ�����һ��
	void stop();
protected:
	//! ���ش˺����õ�״̬�������������
	/*! status���ܵ�ֵ��
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
		���У�
		1����statusΪokDomainsRegisteredʱ��data�����û�����0(���),1(רҵ),2(��ҵ)
		2����statusΪokKeepAliveRecvedʱ��data���ؿͻ���IP��ַ��������ʽ��
		3����������£�dataһֱΪ0
	*/
	virtual void OnStatusChanged(int status, long data) = 0;
	//! ���ش˺����õ�ע���������ÿ��������ִ��һ��
	virtual void OnDomainRegistered(std::string domain){}
	//! ���ش˺����õ��û���Ϣ��XML��ʽ
	virtual void OnUserInfo(std::string userInfo){}
	//! ���ش˺����õ��û�������Ϣ��XML��ʽ
	virtual void OnAccountDomainInfo(std::string domainInfo){}
private:
	//! ��������������ӵ�socket���
	CPHSocket m_tcpsocket,m_udpsocket;
	//! ��ʼ��socket
	bool InitializeSockets();
	//! �ر�����socket
	bool DestroySockets();
	//! ��DDNS���������ӵ�TCP������
	int ExecuteUpdate();
	//! ����UDP�����ӡ�
	bool BeginKeepAlive();
	//! ����һ��UDP������
	bool SendKeepAlive(int opCode);
	//! ��������������
	int RecvKeepaliveResponse();
	//! ��ǰ�Ƿ���Ҫ����TCP����
	bool bNeed_connect;
	//! ���һ�η�����������ʱ��	
	long tmLastSend;
};
#endif