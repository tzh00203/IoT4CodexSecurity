/**************************************************\
 *	admin.c
 *	This is CGI callback function for admin.
\**************************************************/

/* ============================= */
/* Includes                      */
/* ============================= */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close */
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>


#include "boa.h"
#include "globals.h"
#include "apmib.h"
#include "apform.h"
#include "utility.h"
#include "mibtbl.h"
#include "asp_page.h"

#define PROV_FILE	"/tmp/provision.conf"

#define BUF_LEN_1024 1024
#define BUF_LEN_512 512
#define BUF_LEN_256 256
#define BUF_LEN_128	128
#define BUF_LEN_64 64
#define BUF_LEN_16 16

enum {NONEEDRX=0, SIGNUP_RX=1, ADDDEV_RX=2};
	
#define RET_ERR 1
#define RET_OK	0


static char serverUrl[256];
static char mydlinkNumber[9];
static char deviceFootPrint[33]; 
static char signinCookie[BUF_LEN_256];

#define  MIN(a, b)               (((a)<(b))?(a):(b))//eded test define


/* ============================= */
/* Global variable definition    */
/* ============================= */
char login_key[BUF_LEN_16];

/* ============================= */
/* Local function declaration    */
/* ============================= */

/* ============================= */
/* function implementation       */
/* ============================= */

static int read_prov_file()
{
	memset(serverUrl, 0, 256);
	memset(mydlinkNumber, 0, 9);
	memset(deviceFootPrint, 0, 33);
	
//#if DEBUG_PC == 1
#if 0
	snprintf(serverUrl, 256, "%s", "twqa.mydlink.com");
	snprintf(mydlinkNumber, 9, "%s", "12345678");
	snprintf(deviceFootPrint, 33, "%s", "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF");
	return;
#else
	FILE *fp = NULL;
	char buf[BUF_LEN_1024];

	fp = fopen(PROV_FILE, "r");
	if (fp == NULL)
		return RET_ERR;
		
	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf), fp))
	{
		char *strPtr;
		if(strPtr = strstr(buf, "username="))
		{	
			strPtr += strlen("username=");
			sprintf(mydlinkNumber, "%s", strPtr);
			mydlinkNumber[8]='\0';
		}
		else if(strPtr = strstr(buf, "portal_url=http://"))
		{	
			strPtr += strlen("portal_url=http://");
			snprintf(serverUrl,strlen(strPtr)-1, "%s", strPtr);
		}
		else if(strPtr = strstr(buf, "footprint="))
		{	
			strPtr += strlen("footprint=");
			sprintf(deviceFootPrint, "%s", strPtr);
			deviceFootPrint[32] = '\0';
		}
	}

	printf("number=%s, url=%s, footPrintf=%s\n",mydlinkNumber,serverUrl,deviceFootPrint);
	fclose(fp);
	return RET_OK;
#endif
}

void findLatestCookie(char *cookie, char *buf)
{
	char *curPtr	= NULL;
	char *strPtr 	= NULL;
	char *eol		= NULL;
	curPtr = buf;
	
	while(strPtr = strstr(curPtr, "Set-Cookie"))
	{
		eol = strstr(strPtr, ";");
		memset(signinCookie, 0, BUF_LEN_256);
		if(eol)
		{
			//if(strlen(signinCookie)>0)
				//snprintf(signinCookie+strlen(signinCookie)+1, (eol-strPtr)+1, "%s\r\n", strPtr);
			//else
			strPtr+=4;
			snprintf(signinCookie, (eol-strPtr)+1, "%s", strPtr);
			curPtr = eol + 2;
		}
		else
			curPtr += strlen("Set-Cookie");
	}
	printf("signinCookie=%s\n",signinCookie);
}


int send_http_req(char *http_serv, char *send_buf, int buf_len, int rx_hldr, char *ret_content)
{
	int	fd;
	int ret = RET_ERR;
	int sockRet;
	fd_set afdset;
	struct  hostent *host;
	struct  sockaddr_in addr;
	struct 	timeval timeval;
	struct hostent *servIp;	
	
	printf("[%s]..\n", __FUNCTION__);
	
	if (http_serv == NULL)
		return RET_ERR;

	// url string to ip address
	printf("mdl_reg:mydlink server url=%s\n", http_serv);
	servIp = gethostbyname(http_serv);

	if( servIp == NULL)
	{
		printf("mdl_reg:resolve domain error\n");
		return RET_ERR;
	}
	
 	addr.sin_family = servIp->h_addrtype;
  memcpy((char *) &addr.sin_addr.s_addr, servIp->h_addr_list[0], servIp->h_length);
  addr.sin_port = htons(80);

  /* create socket */
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd<0) {
		printf("mdl_reg:socket() failed\n");
		return RET_ERR;
  }

	
	
	fd = socket(servIp->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1) {
		printf("mdl_reg:socket() failed\n");
		return RET_ERR;
	}

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)  
	{
		printf("mdl_reg:connect() failed\n");
		close(fd);
		return RET_ERR;
	}

	// send http packets
	int num_sent = 0;
	while (num_sent < buf_len)
	{
		int n = 0;
		if((n=send(fd, &send_buf[num_sent], buf_len-num_sent, MSG_DONTWAIT|MSG_NOSIGNAL)) < 0) 
		{
			printf("mdl_reg:send()");
			close(fd);
			//write_result(MSG_FW_ERROR);
			return RET_ERR;
		}

		num_sent += n;
	}
	printf("rx_hldr at addr %p\n",rx_hldr);
	// no need to receive
	if (rx_hldr == NONEEDRX)
		return RET_OK;

	sleep(1); /*wait for recv buf*/
	
DO_SELECT:
	FD_ZERO(&afdset);
	FD_SET(fd, &afdset);
	
	timeval.tv_sec = 10;
	timeval.tv_usec = 0;		

	/* timeout error */
	sockRet = select(fd + 1, &afdset, NULL, NULL, &timeval);
	//if (sockRet == -1 && (errno == EINTR || errno == EAGAIN ))
		if (sockRet == -1)	
	{
//		printf("errno=%d\n", errno);
		goto DO_SELECT;
	}
	
	if(sockRet <= 0)
	{
		printf("mdl_reg:select() failed\n");
		close(fd);
		return RET_ERR;
	}
		
	/* receive fail */
	//http_rx_hldr hldr = (http_rx_hldr)rx_hldr;
	//hldr = rx_hldr;
	if(FD_ISSET(fd, &afdset))
	{
		int nread;	
		ioctl(fd, FIONREAD, &nread);	
		if (nread==0)
		{
			printf("socket read failed\n");
			close(fd);
			return RET_ERR;
		}

		//ret = hldr(fd);
		//ret = rx_hldr(fd);
		if (rx_hldr==SIGNUP_RX)
			ret = signup_rx_hldr(fd, ret_content);
		else
			ret = adddev_rx_hldr(fd, ret_content);
		printf("[%s](debug)ret_content=%s\n", __FUNCTION__, ret_content);
	}
	
	if( shutdown(fd, 2) < 0)
	{
		printf("mdl_reg:shutdown() failed\n");
		return RET_ERR;
	}
	close(fd);
	printf("ret=%d\n",ret);
	return ret;
}

int signup_rx_hldr(int fd, char *ret_content)
{
	char rx_buf[BUF_LEN_1024];
	char *content = NULL;
	char *key = NULL;
	int pos = 0;
	int ret = RET_ERR;
	//NccPkt 	packet;
	//memset(&packet, 0, sizeof(packet));
	
	if (fd < 0)
	{
		printf("bug on\n");
		goto SEND_REPLY;
	}

	memset(rx_buf, 0, sizeof(rx_buf));
	read(fd, rx_buf, sizeof(rx_buf)-1);

	//memset(signinCookie, 0, BUF_LEN_256);
	findLatestCookie(signinCookie, rx_buf);

	content = strstr(rx_buf, "\r\n\r\n");

	printf("rx_buf=%s\n", rx_buf);
	if (content == NULL || strlen(content) == 0)
	{
		printf("rx_buf=%s\n", rx_buf);
		//packet.data = "mdl_errmsg_01"; (Cannot login to mydlink.) //Server no response.
		//writeInfo(InfoMyDlinkState, SIGNUP_FAILURE);
		
		goto SEND_REPLY;
	}
	content += strlen("\r\n\r\n");
	
	//packet.data = content;
	if (strstr(content, "success") == NULL)
	{//can't find the string "success"
		printf("(s)content=%s\n", content);
		strcpy(ret_content, content);
		printf("[%s](debug)(f)ret_content=%s\n", __FUNCTION__, ret_content);
		//packet.data = content;		
		goto SEND_REPLY;
	}

	if ((pos=strchr(content, ":")) == -1)
	{
		printf("(f)content=%s\n", content);
		strcpy(ret_content, content);
		printf("[%s](debug)(f)ret_content=%s\n", __FUNCTION__, ret_content);
		//packet.data = content;
		goto SEND_REPLY;
	}
	content[pos] = '\0';
	
	key = &content[pos+1];
	printf("key=%s\n", key);
	snprintf(login_key, sizeof(login_key)-1, "%s", key);

	ret = RET_OK;
	
SEND_REPLY:

	if(ret != RET_OK)
	{
		/*
		send failure content back to DUT's web server(content is responsed from mydlink portal)
		and web server redirect failure content to user's browser
		user's browser should check the keyword to identify if success or not
		*/
	//	ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
	}
	else
	{
		/*
		send success content back to DUT's web server(content is responsed from mydlink portal)
		and web server redirect failure content to user's browser
		user's browser should check the keyword to identify if success or not
		*/
	}
	printf("[%s](debug)ret_content=%s\n", __FUNCTION__, ret_content);
	return ret;
}
int adddev_rx_hldr(int fd, char *ret_content)
{
	char rx_buf[BUF_LEN_1024];
	char *content = NULL;
	char *key = NULL;
	int pos = 0;
	int ret = RET_ERR;
//	NccPkt 	packet;
	int nread;	

	ioctl(fd, FIONREAD, &nread);
	//memset(&packet, 0, sizeof(packet));
	if (fd < 0)
	{
		printf("bug on\n");
		goto SEND_REPLY;
	}

	memset(rx_buf, 0, sizeof(rx_buf));
	recv(fd, rx_buf, MIN(sizeof(rx_buf)-1, nread), MSG_NOSIGNAL);
	content = strstr(rx_buf, "\r\n\r\n");

	if (content == NULL || strlen(content) == 0)
	{
		printf("rx_buf=%s\n", rx_buf);
		
		goto SEND_REPLY;
	}
	content += strlen("\r\n\r\n");
	
	//packet.data = content;
	if (strstr(content, modelName) == NULL)
	{
		printf("content(%d)=%s\n", strlen(content), content);
		strcpy(ret_content, content);
		printf("[%s](debug)(f)ret_content=%s\n", __FUNCTION__, ret_content);
		//if (strlen(content) == 0)
			//packet.data = "mdl_errmsg_01"(Cannot login to mydlink.);
		goto SEND_REPLY;
	}
	printf("content=%s\n",content);
	//packet.data = content;
	if ((pos=strchr(content, ":")) == -1)
	{
		ret = RET_ERR;
	}
	else
	{
		ret = RET_OK;
	}

	
SEND_REPLY:

	if(ret != RET_OK){
		/*
		send failure content back to DUT's web server(content is responsed from mydlink portal)
		and web server redirect failure content to user's browser
		user's browser should check the keyword to identify if success or not
		*/
		//ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
	}else if(ret==RET_OK){
		/*
		send success content back to DUT's web server(content is responsed from mydlink portal)
		and web server redirect failure content to user's browser
		user's browser should check the keyword to identify if success or not
		*/
		//ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
	}

	return ret;
}
//Ret online_signup(int sock)
int mydlink_signup(char *ret_content)
{	
	int ret = RET_ERR;
	char send_buf[BUF_LEN_1024];
	char req_line[BUF_LEN_512];
	char cur_lang[BUF_LEN_16];
	char req_host[BUF_LEN_512];
	
	char mib_mydlink_EmailAccount[BUF_LEN_128];		//new mydlink account input from gui
	char mib_mydlink_AccountPassword[BUF_LEN_128];	//new mydlink password input from gui
	char mib_mydlink_FirstName[BUF_LEN_128];			//user's first name input from gui
	char mib_mydlink_LastName[BUF_LEN_128];
	
	printf("[%s]..\n", __FUNCTION__);

	if(read_prov_file() == RET_ERR)
	{
		/*
		if provision.conf cannot be found,
		you should return "mdl_errmsg_01" (Cannot login to mydlink.)message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_01"(Cannot login to mydlink.) meaasage
		*/
		
		printf("cannot find provfile\n");
		/*packet.data = "mdl_errmsg_01"; (Cannot login to mydlink.)
		ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
		*/
		strcpy(ret_content, retMLS("multi153"));
		
		return RET_OK;
	}

	memset(send_buf, 0, sizeof(send_buf));
	memset(req_line, 0, sizeof(req_line));
	memset(cur_lang, 0, sizeof(cur_lang));
	memset(req_host, 0, sizeof(req_host));

	memset(mib_mydlink_EmailAccount, 0, sizeof(mib_mydlink_EmailAccount));
	memset(mib_mydlink_AccountPassword, 0, sizeof(mib_mydlink_AccountPassword));
	memset(mib_mydlink_FirstName, 0, sizeof(mib_mydlink_FirstName));
	memset(mib_mydlink_LastName, 0, sizeof(mib_mydlink_LastName));

	apmib_get( MIB_MYDLINK_EMAILACCOUNT,  (void *)mib_mydlink_EmailAccount);
	apmib_get( MIB_MYDLINK_ACCOUNTPASSWORD,  (void *)mib_mydlink_AccountPassword);
	apmib_get( MIB_MYDLINK_FIRSTNAME,  (void *)mib_mydlink_FirstName);
	apmib_get( MIB_MYDLINK_LASTNAME,  (void *)mib_mydlink_LastName);
	apmib_get( MIB_MYDLINK_WZ_LANG,  (void *)cur_lang);
	
	if (strlen(cur_lang)==0)
	{
		strcpy(cur_lang, "en");
	}

	//fill in portal_url
	sprintf(req_host, "%s", serverUrl);

	// prepare request line
	snprintf(req_line, sizeof(req_line)-1, 
			"client=wizard"
			"&wizard_version=%s_%s"
			"&lang=%s"
			"&action=sign-up"
			"&accept=accept"
			"&email=%s"
			"&password=%s"
			"&password_verify=%s"
			"&name_first=%s"
			"&name_last=%s",
			modelName,			//model name
			fwVersion,						//fw version
			cur_lang,					//current language
			mib_mydlink_EmailAccount,		//new mydlink account input from gui
			mib_mydlink_AccountPassword,	//new mydlink password input from gui
			mib_mydlink_AccountPassword,	//new mydlink password input from gui
			mib_mydlink_FirstName,			//user's first name input from gui
			mib_mydlink_LastName);			//user's last name input from gui
			
	// prepare send buffer
	snprintf(send_buf, sizeof(send_buf)-1, 
				"POST /signin/ HTTP/1.1\r\nAccept: text/html, */*\r\n"
				"Accept-Language:en\r\n"
				"x-requested-with: XMLHttpRequest\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
				"Host: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: Keep-Alive\r\n"
				"Cache-Control: no-cache\r\n\r\n"
				"%s",
				req_host,
				strlen(req_line),
				req_line);
		
	printf("mydlink_reg:send_buf =\n%s\n", send_buf);
		
	if (send_http_req(req_host, send_buf, strlen(send_buf), SIGNUP_RX, ret_content) == RET_ERR)
	{
		printf("send_http_req failed\n");

		/*
		if login fail,
		please do not save mydlink ever registered to DUT's parameter.
		return "mdl_errmsg_04" (Send signup request failed.) to DUT's web server
		and user's browser can pop-up "mdl_errmsg_04" message
		*/
		strcpy(ret_content, retMLS("multi156"));
		return RET_ERR;
	}

	ret = RET_OK;

SEND_REPLY:

	if(ret != RET_OK)
	{
		/*
		if signup fail,
		you should return "mdl_errmsg_05" (Cannot send signup) message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_05" meaasage
		*/
		strcpy(ret_content, retMLS("multi157"));
	}

    return ret;
}

//int online_signin(int sock)
int mydlink_signin(char *ret_content)
{
	int ret = RET_ERR;
	char send_buf[BUF_LEN_1024];
	char req_line[BUF_LEN_512];
	char cur_lang[BUF_LEN_16];
	char req_host[BUF_LEN_256];
	
	char mib_mydlink_EmailAccount[BUF_LEN_128];		//new mydlink account input from gui
	char mib_mydlink_AccountPassword[BUF_LEN_128];	//new mydlink password input from gui

	printf("[%s]..\n", __FUNCTION__);

	if(read_prov_file() == RET_ERR)
	{
		/*
		if provision.conf cannot be found,
		you should return "mdl_errmsg_01" (Cannot login to mydlink.) message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_01" meaasage
		*/
		
		printf("cannot find provfile\n");
		/*packet.data = "mdl_errmsg_01";
		ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
		*/
		strcpy(ret_content, retMLS("multi153"));
		return RET_ERR;
	}

	memset(send_buf, 0, sizeof(send_buf));
	memset(req_line, 0, sizeof(req_line));
	memset(cur_lang, 0, sizeof(cur_lang));
	memset(req_host, 0, sizeof(req_host));
	memset(mib_mydlink_EmailAccount, 0, sizeof(mib_mydlink_EmailAccount));
	memset(mib_mydlink_AccountPassword, 0, sizeof(mib_mydlink_AccountPassword));

	apmib_get( MIB_MYDLINK_EMAILACCOUNT,  (void *)mib_mydlink_EmailAccount);
	apmib_get( MIB_MYDLINK_ACCOUNTPASSWORD,  (void *)mib_mydlink_AccountPassword);
	apmib_get( MIB_MYDLINK_WZ_LANG,  (void *)cur_lang);
	
	if (strlen(cur_lang)==0)
	{
		strcpy(cur_lang, "en");
	}

	//fill in portal_url
	sprintf(req_host, "%s", serverUrl);

	// prepare request line
	snprintf(req_line, sizeof(req_line)-1, 
			"client=wizard"
			"&wizard_version=%s_%s"
			"&lang=%s"
			"&email=%s"
			"&password=%s",
			modelName,			//model name
			fwVersion,						//fw version
			cur_lang,					//current language
			mib_mydlink_EmailAccount,		//email account that input from gui
			mib_mydlink_AccountPassword);	//account password that input from gui

	snprintf(send_buf, sizeof(send_buf)-1, 
				"POST /account/?signin HTTP/1.1\r\nAccept: text/html, */*\r\n"
				"Accept-Language:en\r\n"
				"x-requested-with: XMLHttpRequest\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
				"Host: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: Keep-Alive\r\n"
				"Cache-Control: no-cache\r\n\r\n"
				"%s",
				req_host,
				strlen(req_line),
				req_line);
		
	printf("mydlink_reg:send_buf =\n%s\n", send_buf);
		
	//if (send_http_req(req_host, send_buf, strlen(send_buf), (void *)signup_rx_hldr) == RET_ERR)
	if (send_http_req(req_host, send_buf, strlen(send_buf), SIGNUP_RX, ret_content) == RET_ERR)		
	{
		printf("send_http_req failed\n");

		/*
		if login fail,
		please do not save mydlink ever registered to DUT's parameter.
		return "mdl_errmsg_02" to DUT's web server
		and user's browser can pop-up "mdl_errmsg_02" message
		*/
		strcpy(ret_content, retMLS("multi154"));
		return RET_ERR;
	}

	ret = RET_OK;

SEND_REPLY:	
	if(ret != RET_OK)
	{
		/*
		if login fail,
		you should return "mdl_errmsg_03" message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_03" meaasage
		*/
		strcpy(ret_content, retMLS("multi155"));
	}
	
    return ret;
}

//int online_adddev(int sock)
int mydlink_addev(char *ret_content)
{
	int ret = RET_ERR;
	char send_buf[BUF_LEN_1024];
	char req_line[BUF_LEN_512];
	char cur_lang[BUF_LEN_16];
	char req_host[BUF_LEN_256];
	char userpasswd[BUF_LEN_64];
	
	printf("[%s]..\n", __FUNCTION__);

	if(read_prov_file() == RET_ERR)
	{
		/*
		if provision.conf cannot be found,
		you should return "mdl_errmsg_01" (Cannot login to mydlink.)message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_01" meaasage
		*/
		
		printf("cannot find provfile\n");
		/*packet.data = "mdl_errmsg_01";
		ncc_rinf_send(sock, packet, RETC_OK, OPC_R1_CCP);
		*/
		strcpy(ret_content, retMLS("multi153"));
		return RET_ERR;
	}
	memset(send_buf, 0, sizeof(send_buf));
	memset(req_line, 0, sizeof(req_line));
	memset(cur_lang, 0, sizeof(cur_lang));
	memset(req_host, 0, sizeof(req_host));
	memset(userpasswd, 0, sizeof(userpasswd));

	apmib_get( MIB_USER_PASSWORD,  (void *)userpasswd);

	//fill in portal_url
	sprintf(req_host, "%s", serverUrl);

	// prepare request line
	snprintf(req_line, sizeof(req_line)-1, 
			"client=wizard"
			"&wizard_version=%s_%s"
			"&lang=%s"
			"&dlife_no=%s"
			"&device_password=%s"
			"&dfp=%s&", //Buck, 2012/04/23, add '&' at here( as myDlink RD's suggestion ) , fix content-length issue
			modelName,		//model name
			fwVersion,					//fw version
			cur_lang,				//current language 
			mydlinkNumber,			//mydlink number, can find out from provision.conf
			userpasswd, 		//password used to login DUT's admin account
			deviceFootPrint);		//foot print, can find out from provision.conf
				
	snprintf(send_buf, sizeof(send_buf)-1, 
				"POST /account/?add HTTP/1.1\r\nAccept: text/html, */*\r\n"
				"Accept-Language:en\r\n"
				"x-requested-with: XMLHttpRequest\r\n"
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
				"Host: %s\r\n"
				"%s\r\n"
				"Content-Length: %d\r\n"
				"Connection: Keep-Alive\r\n"
				"Cache-Control: no-cache\r\n\r\n"
				"%s",	//Jerry, remove the end '&'. why content-length did not count this '&'????
				req_host,			//url of mydlink portal
				signinCookie,		//cookie, parse from previous packet from portal
				strlen(req_line),
				req_line);
		
	printf("mydlink_reg:send_buf =\n%s\n", send_buf);
		
	//if (ret=(send_http_req(req_host, send_buf, strlen(send_buf), (void *)adddev_rx_hldr)) == RET_ERR)
	if (ret=(send_http_req(req_host, send_buf, strlen(send_buf), ADDDEV_RX, ret_content)) == RET_ERR)
	{
		printf("send_http_req failed\n");

		/*
		if login fail,
		please do not save mydlink ever registered to DUT's parameter.
		*/
		
		goto SEND_REPLY;
	}
	

SEND_REPLY:
	if(ret == RET_OK)
	{
		int tmp_buf_int = 1;
		printf("setting register_st\n");

		apmib_set(MIB_MYDLINK_REG_STAT, (void *)&tmp_buf_int);
		apmib_update(CURRENT_SETTING);	
		
		/*
		if login success,
		please save mydlink ever registered to DUT's parameter register_st,
		this parameter will be also referenced by agent via mdb command.
		*/
	}
	else
	{
		printf("adddev failed\n");
		/*
		if login fail,
		you should return "mdl_errmsg_01" (Cannot login to mydlink.)message to DUT's web server, 
		and user's browser should pop-up "mdl_errmsg_01" meaasage
		*/
	}
	
    return ret;
}

int check_internet()
{
	struct hostent *servIp = 0;	
	
	snprintf(serverUrl, 256, "%s", "twqa.mydlink.com");
	
	// url string to ip address
	servIp = gethostbyname(serverUrl);


	if( servIp == 0)
	{
		printf("mdl_reg:resolve domain error\n");
		system("date > /var/tmp/int_fail");
		return RET_ERR;
	}
	system("date > /var/tmp/int_ok");
	return RET_OK;
}


