#include "__codeql_compat.h"
/*
 *      Web server handler routines for management (password, save config, f/w update)
 *
 *      Authors: David Hsu	<davidhsu@realtek.com.tw>
 *
 *      $Id: fmmgmt.c,v 1.3 2005/05/13 07:53:51 tommy Exp $
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "../webs.h"
#include "../um.h"
#include "apmib.h"
#include "apform.h"
#include "utility.h"

#define DEFAULT_GROUP		T("administrators")
#define ACCESS_URL		T("/")
#define RFTEST_VERSION T("V1.03")

static char superName[MAX_NAME_LEN]={0}, superPass[MAX_NAME_LEN]={0};
static char userName[MAX_NAME_LEN]={0}, userPass[MAX_NAME_LEN]={0};

	int isServerAlive=0;
	char zip_url[128], fw_url[128], correct_md5[125],downloadfw_md5[125],fw_version[30],product_name[100];
int UpgradeProcessCount=0;
int wep_used;
extern int connectIF;
extern int SSIDexist;
////////////////////////////////////////////////////////////////////////////////
static int flash_write(char *buf, int offset, int len)
{
	int fh;
	int ok=1;

	fh = open(FLASH_DEVICE_NAME, O_RDWR);

	if ( fh == -1 )
		return 0;

	lseek(fh, offset, SEEK_SET);

	if ( write(fh, buf, len) != len)
		ok = 0;

	close(fh);
	sync();

	return ok;
}
////////////////////////////////////////////////////////////////////////////////
static void swap_mib_word_value(APMIB_Tp pMib)
{
	pMib->fragThreshold = WORD_SWAP(pMib->fragThreshold);
	pMib->rtsThreshold = WORD_SWAP(pMib->rtsThreshold);
	pMib->supportedRates = WORD_SWAP(pMib->supportedRates);
	pMib->basicRates = WORD_SWAP(pMib->basicRates);
	pMib->beaconInterval = WORD_SWAP(pMib->beaconInterval);
	pMib->inactivityTime = DWORD_SWAP(pMib->inactivityTime);
	pMib->leaseTime = DWORD_SWAP(pMib->leaseTime);

	pMib->wpaGroupRekeyTime = DWORD_SWAP(pMib->wpaGroupRekeyTime);
	pMib->rsPort = WORD_SWAP(pMib->rsPort);
	pMib->accountRsPort = WORD_SWAP(pMib->accountRsPort);
	pMib->accountRsUpdateDelay = WORD_SWAP(pMib->accountRsUpdateDelay);
	pMib->rsIntervalTime = WORD_SWAP(pMib->rsIntervalTime);
	pMib->accountRsIntervalTime = WORD_SWAP(pMib->accountRsIntervalTime);

{
	int i,j;
	pMib->TimeZoneSel = WORD_SWAP(pMib->TimeZoneSel);
	pMib->startMonth = WORD_SWAP(pMib->startMonth);
	pMib->startDay = WORD_SWAP(pMib->startDay);
	pMib->endMonth = WORD_SWAP(pMib->endMonth);
	pMib->endDay = WORD_SWAP(pMib->endDay);
	pMib->reManPort = WORD_SWAP(pMib->reManPort);
	pMib->pppMTU = WORD_SWAP(pMib->pppMTU);
	pMib->L2TPMTU = WORD_SWAP(pMib->L2TPMTU);
	pMib->pptpMTU = WORD_SWAP(pMib->pptpMTU);
	pMib->pppIdleTime = WORD_SWAP(pMib->pppIdleTime);
	pMib->pptpIdleTime = WORD_SWAP(pMib->pptpIdleTime);
	pMib->L2TPIdleTime = WORD_SWAP(pMib->L2TPIdleTime);
	pMib->appLayerGateway = DWORD_SWAP(pMib->appLayerGateway);
	pMib->dhisHostID = DWORD_SWAP(pMib->dhisHostID);
	pMib->podPack = WORD_SWAP(pMib->podPack);
	pMib->podBur = WORD_SWAP(pMib->podBur);
	pMib->synPack = WORD_SWAP(pMib->synPack);
	pMib->synBur = WORD_SWAP(pMib->synBur);
	pMib->scanNum = WORD_SWAP(pMib->scanNum);
//////////////////////////////////////////////////////////////////////////////////////////////
	for (i=0; i<pMib->vserNum; i++) {
		pMib->vserArray[i].fromPort = WORD_SWAP(pMib->vserArray[i].fromPort);
		pMib->vserArray[i].toPort = WORD_SWAP(pMib->vserArray[i].toPort);
	}
	for (i=0; i<pMib->portFwNum; i++) {
		pMib->portFwArray[i].fromPort = WORD_SWAP(pMib->portFwArray[i].fromPort);
		pMib->portFwArray[i].toPort = WORD_SWAP(pMib->portFwArray[i].toPort);
	}
	for (i=0; i<pMib->portFilterNum; i++) {	//no use
		pMib->portFilterArray[i].fromPort = WORD_SWAP(pMib->portFilterArray[i].fromPort);
		pMib->portFilterArray[i].toPort = WORD_SWAP(pMib->portFilterArray[i].toPort);
	}
	for (i=0; i<pMib->triggerPortNum; i++) {
		/*kyle
		pMib->triggerPortArray[i].tri_fromPort = WORD_SWAP(pMib->triggerPortArray[i].tri_fromPort);
		pMib->triggerPortArray[i].tri_toPort = WORD_SWAP(pMib->triggerPortArray[i].tri_toPort);
		for (j=0; j<=4; j++) {
			pMib->triggerPortArray[i].inc_fromPort[j][0] = WORD_SWAP(pMib->triggerPortArray[i].inc_fromPort[j][0]);
			pMib->triggerPortArray[i].inc_fromPort[j][1] = WORD_SWAP(pMib->triggerPortArray[i].inc_fromPort[j][1]);
		}
		pMib->triggerPortArray[i].inc_toPort = WORD_SWAP(pMib->triggerPortArray[i].inc_toPort);
		kyle*/

		for (j=0; j<=4; j++) {
			pMib->triggerPortArray[i].tcp_port[j][0] = WORD_SWAP(pMib->triggerPortArray[i].tcp_port[j][0]);
			pMib->triggerPortArray[i].tcp_port[j][1] = WORD_SWAP(pMib->triggerPortArray[i].tcp_port[j][1]);

			pMib->triggerPortArray[i].udp_port[j][0] = WORD_SWAP(pMib->triggerPortArray[i].udp_port[j][0]);
			pMib->triggerPortArray[i].udp_port[j][1] = WORD_SWAP(pMib->triggerPortArray[i].udp_port[j][1]);
		}
	}
	for (i=0; i<pMib->ACPCNum; i++) {
		pMib->ACPCArray[i].serindex = DWORD_SWAP(pMib->ACPCArray[i].serindex);
		for (j=0; j<=4; j++) {
			pMib->ACPCArray[i].Port[j][0] = WORD_SWAP(pMib->ACPCArray[i].Port[j][0]);
			pMib->ACPCArray[i].Port[j][1] = WORD_SWAP(pMib->ACPCArray[i].Port[j][1]);
		}
	}
	for (i=0; i<pMib->sroutNum; i++) {
		pMib->sroutArray[i].hopCount = WORD_SWAP(pMib->sroutArray[i].hopCount);
	}
}

}

///////////////////////////////////////////////////////////////////
//static void reset_user_profile()
//modify by vance 2009.03.11
void reset_user_profile()
{
	struct stat status;
	umClose();

	if ( stat(T("umconfig.txt"), &status) == 0) // file existed
               	unlink(T("umconfig.txt"));

	umOpen();
	umRestore(T("umconfig.txt"));

	set_user_profile();
}


/////////////////////////////////////////////////////////////////////////////
static unsigned short fwChecksumOk(char_t *data, int len)
{
	unsigned short sum=0;
	int i;

	for (i=0; i<len; i+=2) {
		sum += WORD_SWAP( *((unsigned short *)&data[i]) );

	}
	return( (sum==0) ? 1 : 0);
}

/////////////////////////////////////////////////////////////////////////////
static unsigned short  fwChecksum(char_t *data, int len)
{
    unsigned short sum=0;
	int i;

	for (i=0; i<len; i+=2) {
   		sum += WORD_SWAP( *((unsigned short *)&data[i]) );
    }

	return( 0 - sum );
}


///////////////////////////////////////////////////////////////////////////////
void formAccept(webs_t wp, char_t *path, char_t *query)
{
    char_t  *submitUrl;
	//printf("\nkillall processes\n");
	//#ifdef __TARGET_BOARD__		//Erwin
	//	system("killall auth upnpd udhcpc iwcontrol diagd cleanlog.sh udhcpd");
	//#endif

	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page
	system(submitUrl);
	if (submitUrl[0])
		websRedirect(wp, submitUrl);
	else
		websDone(wp, 200);
  	return;
}
///////////////////////////////////////////////////////////////////////////////
void formReboot(webs_t wp)
{
	char tmpBuf[200];291
	char_t  *submitUrl;
	int	pid;
	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	//sprintf(tmpBuf, T("System Restarting! Please wait for a while !"));
	sprintf(tmpBuf, T("<script>document.write(showText(sysrst));</script>"));

	main_menu=atoi((char_t *)websGetVar(wp, T("main_menu_number"), T("0")));
	sub_menu=atoi((char_t *)websGetVar(wp, T("sub_menu_number"), T("0")));
	third_menu=atoi((char_t *)websGetVar(wp, T("third_menu_number"), T("0")));
	
	sprintf(tmpBuf, T("<script>document.write(showText(SystemRestarting));</script>"));  //EDX patrick add

	OK_MSG3(tmpBuf, submitUrl);
	system("/bin/reboot.sh 2 &");

}
void formApply(webs_t wp)
{
    char tmpBuf[200];
    unsigned char buffer[100];
    unsigned char newurl[150];
    char_t  *submitUrl;
	if (lanipChanged == 1) {

        if (AP_ROUTER_MODE == 2){
	    		if ( !apmib_get( MIB_CONVERT_IP_ADDR,  (void *)buffer) )
	    			return -1;
           }
        else{
        		if ( !apmib_get( MIB_IP_ADDR,  (void *)buffer) )
        			return -1;
          }

		sprintf(newurl, "http://%s", fixed_inet_ntoa(buffer));
		//sprintf(tmpBuf, T("System Restarting! Please wait for a while !"));
		sprintf(tmpBuf, T("<script>if(window.sysrst) document.write(showText(sysrst)); else  document.write(\"System Restarting! Please wait for a while !\");</script>"));
		OK_MSG3(tmpBuf, newurl);
		//kill(1,SIGTERM);
	}
//EDX Timmy start
	else if (stadrvenable == 1) {
		submitUrl=T("/wanwisptest.asp");
		sprintf(tmpBuf, T("<script>if(window.sysrst) document.write(showText(sysrst)); else  document.write(\"System Restarting! Please wait for a while !\");</script>"));
		REPLACE_MSG(submitUrl);
		stadrvenable=0;
	}
	else {
		submitUrl = websGetVar(wp, T("submit-url"), T(""));
		sprintf(tmpBuf, T("<script>if(window.sysrst) document.write(showText(SystemRestarting)); else  document.write(\"System Restarting! Please wait for a while !\");</script>"));
		OK_MSG3(tmpBuf, submitUrl);
	}

	int dmz_mode;
	apmib_get(MIB_DMZ_MODE,(void *)&dmz_mode);
	if (dmz_mode == 1){
		system("/bin/reboot.sh 2 &");
	}
	else
	{
			system("/bin/reset.sh gw 2 &");
	}

/*
	system("killall -SIGTERM pppd");
	sprintf(tmpBuf, "%s %s %s", "/bin/init.sh", "gw", "all");
	system(tmpBuf);
*/
	lanipChanged = 0;
	wizardEnabled = 0;
}

//-----------------------------------------------------


//kyle
///////////////////////////////////////////////////////////////////////////////
void formConnect(webs_t wp) // Tommy 2005.11.02
{
    char tmpBuf[200];
    char_t  *submitUrl;
	int type =2;
	int wanType=atoi(websGetVar(wp,"wanMode",""));
	submitUrl = websGetVar(wp, T("submit-url"), T(""));

	if (strcmp(websGetVar(wp,"buttonact",""),"Connect")==0)
	{
		system("echo on > /var/manual.var");
		int wait_time=30;

		if (wanType == 2)
			system("/bin/pppoe.sh connect"); // Lance 2004.2.3
		if (wanType == 3)
			system("/bin/pptp.sh connect");
		if (wanType == 6)
			system("/bin/l2tp.sh connect");

		while (wait_time-- >0) {
		if (isConnectPPP())
			break;
			sleep(1);
		}
		if (isConnectPPP())
			strcpy(tmpBuf, T("Connected to server successfully.\n"));
		else
			strcpy(tmpBuf, T("Connect to server failed!\n"));

		OK_MSG1(tmpBuf, submitUrl);
		return;
	}

	if (strcmp(websGetVar(wp,"buttonact",""),"Disconnect")==0)
	{
		system("echo on > /var/manual.var");

		system("/bin/disconnect.sh all"); // Lance 2004.2.3

		if (wanType == 2)
			strcpy(tmpBuf, T("PPPoE disconnected.\n"));
		if (wanType == 3)
			strcpy(tmpBuf, T("PPTP disconnected.\n"));
		if (wanType == 6)
			strcpy(tmpBuf, T("L2TP disconnected.\n"));

		OK_MSG1(tmpBuf, submitUrl);
		return;
	}

	if (strcmp(websGetVar(wp,"buttonact",""),"Renew")==0)
	{
		//system("/bin/init.sh gw all"); // Lance 2004.2.3

		system("killall rdisc"); //Kyle 2007/12/12
		//vance 2009.01.06
		char buf[100];
		sprintf(buf,"dhcpc.sh %s wait",_WAN_IF_);
		system(buf);
		//system("dhcpc.sh eth2.2 wait");//Kyle 2007/12/12

		strcpy(tmpBuf, T("Dynamic IP renew successfully.\n"));

		OK_MSG1(tmpBuf, submitUrl);
		return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////
void formSysLog(webs_t wp, char_t *path, char_t *query)
{
	char_t *strSave, *strClear, *submitUrl;
	FILE *fp;
	unsigned char tempbuf[100];
	int rdSize;

	// Save system log to file
	strSave = websGetVar(wp, T("send"), T(""));
	if (strSave[0]) {
		websWrite(wp, "HTTP/1.0 200 OK\n");
		websWrite(wp, "Content-Type: application/octet-stream;\n");
		websWrite(wp, "Content-Disposition: attachment;filename=\"syslog.txt\" \n");
		websWrite(wp, "Pragma: no-cache\n");
		websWrite(wp, "Cache-Control: no-cache\n");
		websWrite(wp, "\n");
		if ((fp = fopen("/var/log/syslog","r"))!=NULL) {
			while ((rdSize = fread(tempbuf, 1, 100, fp)) !=0 )
				websWriteDataNonBlock(wp, tempbuf, rdSize);
			fclose(fp);
		}
		websDone(wp, 200);
		return;
	}

	// clear security log file
	strClear = websGetVar(wp, T("reset"), T(""));
	if (strClear[0]) {
		system("/bin/syslog.sh clean");

		submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page
		REPLACE_MSG(submitUrl);
		return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////
void formSecLog(webs_t wp, char_t *path, char_t *query)
{
	char_t *strSave, *strClear, *submitUrl;
	FILE *fp;
	unsigned char tempbuf[100];
	int rdSize;

	// Save security log to file
	strSave = websGetVar(wp, T("send"), T(""));
	if (strSave[0]) {

		websWrite(wp, "HTTP/1.0 200 OK\n");
		websWrite(wp, "Content-Type: application/octet-stream;\n");
		websWrite(wp, "Content-Disposition: attachment;filename=\"security.txt\" \n");
		websWrite(wp, "Pragma: no-cache\n");
		websWrite(wp, "Cache-Control: no-cache\n");
		websWrite(wp, "\n");

		if ((fp = fopen("/var/log/security","r"))!=NULL) {
			while ((rdSize = fread(tempbuf, 1, 100, fp)) !=0 )
				websWriteDataNonBlock(wp, tempbuf, rdSize);
			fclose(fp);
		}
		websDone(wp, 200);
		return;
	}

	// clear security log file
	strClear = websGetVar(wp, T("reset"), T(""));
	if (strClear[0]) {
		system("/bin/seclog.sh clean");

		submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page
		REPLACE_MSG(submitUrl);
		return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////
void formDebug(webs_t wp, char_t *path, char_t *query)
{
	char_t *strPass;
	FILE *fp;
	unsigned char buffer[100];
	int rdSize;
	char tmpBuf[200];


	strPass = websGetVar(wp, T("password"), T(""));
	if (strcmp(strPass,"report.txt")) // Lance 2003.11.05
	{
		strcpy(tmpBuf, T("ERROR: Invalid value of Command !"));
       	goto setErr_debug;
	}

	system("/bin/savelog.sh");

	websWrite(wp, "HTTP/1.0 200 OK\n");
	websWrite(wp, "Content-Type: application/octet-stream;\n");
	websWrite(wp, "Content-Disposition: attachment;filename=\"report.txt\" \n"); // Lance 003.11.05
	websWrite(wp, "Pragma: no-cache\n");
	websWrite(wp, "Cache-Control: no-cache\n");
	websWrite(wp, "\n");

	if ((fp = fopen("/var/run/syslog","r"))!=NULL) {
		while ((rdSize = fread(buffer, 1, 100, fp)) !=0 )
			websWriteDataNonBlock(wp, buffer, rdSize);
		fclose(fp);
	}
	websDone(wp, 200);
	return;

setErr_debug:
	ERR_MSG(wp, tmpBuf);
	return;
}
//////////////////////////////////////////////////////////////////////////////////////////////
void formSaveConfig(webs_t wp, char_t *path, char_t *query)
{
	char_t *strRequest, *submitUrl;
	char *buf, *ptr=NULL;
	PARAM_HEADER_Tp pHeader;
	unsigned char checksum;
	int len, status=0, ver, force, len1;
	char tmpBuf[200];


	int pid;

	CONFIG_DATA_T type=0;

	len1 = sizeof(PARAM_HEADER_T) + sizeof(APMIB_T) + sizeof(checksum) + 100;  // 100 for expansion
	len = csHeader.len;
	len  = WORD_SWAP(len);
	main_menu=atoi((char_t *)websGetVar(wp, T("main_menu_number"), T("0")));
	sub_menu=atoi((char_t *)websGetVar(wp, T("sub_menu_number"), T("0")));
	third_menu=atoi((char_t *)websGetVar(wp, T("third_menu_number"), T("0")));
	len += sizeof(PARAM_HEADER_T) + 100;
	if (len1 > len)
		len = len1;

	buf = malloc(len);
	if ( buf == NULL ) {
		strcpy(tmpBuf, "Allocate buffer failed!");
		goto back;
	}

	strRequest = websGetVar(wp, T("save-cs"), T(""));
	if (strRequest[0])
		type |= CURRENT_SETTING;

	strRequest = websGetVar(wp, T("save"), T(""));
	if (strRequest[0])
		type |= CURRENT_SETTING;

	strRequest = websGetVar(wp, T("save-hs"), T(""));
	if (strRequest[0])
		type |= HW_SETTING;

	strRequest = websGetVar(wp, T("save-ds"), T(""));
	if (strRequest[0])
		type |= DEFAULT_SETTING;

	strRequest = websGetVar(wp, T("save-all"), T(""));
	if (strRequest[0])
		type |= HW_SETTING | DEFAULT_SETTING | CURRENT_SETTING;

	if (type) {
		websWrite(wp, "HTTP/1.0 200 OK\n");

		websWrite(wp, "Content-Type: application/pla;\n");
		websWrite(wp, "Content-Disposition: attachment;filename=\"config.cfg\" \n");
		websWrite(wp, "Pragma: no-cache\n");
		websWrite(wp, "Cache-Control: no-cache\n");
		websWrite(wp, "\n");

		if (type & HW_SETTING) {
			pHeader = (PARAM_HEADER_Tp)buf;
			len = pHeader->len = hsHeader.len;
			memcpy(&buf[sizeof(PARAM_HEADER_T)], pHwSetting, pHeader->len-1);

			pHeader->len  = WORD_SWAP(pHeader->len);
			memcpy(pHeader->signature, hsHeader.signature, SIGNATURE_LEN);
			ptr = (char *)&buf[sizeof(PARAM_HEADER_T)];
			checksum = CHECKSUM(ptr, len-1);
			buf[sizeof(PARAM_HEADER_T)+len-1] = checksum;

			ptr = &buf[sizeof(PARAM_HEADER_T)];
			ENCODE_DATA(ptr,  len);
			websWriteBlock(wp, buf, len+sizeof(PARAM_HEADER_T));
		}

//		if (type & DEFAULT_SETTING) {			//removed by Erwin
		if (type & CURRENT_SETTING) {
			pHeader = (PARAM_HEADER_Tp)buf;
			len = pHeader->len = dsHeader.len;
//			memcpy(&buf[sizeof(PARAM_HEADER_T)], pMibDef, len-1);	//removed by Erwin
			memcpy(&buf[sizeof(PARAM_HEADER_T)], pMib, len-1);

			pHeader->len  = WORD_SWAP(pHeader->len);
			swap_mib_word_value((APMIB_Tp)&buf[sizeof(PARAM_HEADER_T)]);
			memcpy(pHeader->signature, dsHeader.signature, SIGNATURE_LEN);
			ptr = (char *)&buf[sizeof(PARAM_HEADER_T)];
			checksum = CHECKSUM(ptr, len-1);
			buf[sizeof(PARAM_HEADER_T)+len-1] = checksum;

			ptr = &buf[sizeof(PARAM_HEADER_T)];
			ENCODE_DATA(ptr,  len);

			int sendLength=0;
			int fileLength=len+sizeof(PARAM_HEADER_T);
			//2007.06.11 fixed by Kyle
			//websWriteDataNonBlock(wp, buf, len+sizeof(PARAM_HEADER_T))
			websWriteBlock(wp, buf, len+sizeof(PARAM_HEADER_T));

		}

/*		if (type & CURRENT_SETTING) {			//removed by Erwin
			pHeader = (PARAM_HEADER_Tp)buf;
			len = pHeader->len = csHeader.len;
			memcpy(&buf[sizeof(PARAM_HEADER_T)], pMib, len-1);

			pHeader->len  = WORD_SWAP(pHeader->len);
			swap_mib_word_value((APMIB_Tp)&buf[sizeof(PARAM_HEADER_T)]);
			memcpy(pHeader->signature, csHeader.signature, SIGNATURE_LEN);
			ptr = (char *)&buf[sizeof(PARAM_HEADER_T)];
			checksum = CHECKSUM(ptr, len-1);
			buf[sizeof(PARAM_HEADER_T)+len-1] = checksum;

			ptr = &buf[sizeof(PARAM_HEADER_T)];
			ENCODE_DATA(ptr,  len);
			websWriteDataNonBlock(wp, buf, len+sizeof(PARAM_HEADER_T));
		}
*/
		websDone(wp, 200);
		free(buf);
		return;
	}

	strRequest = websGetVar(wp, T("load"), T(""));
	if (strRequest[0]) {
		len = 0;
		status = 1;
		type = 0;
		while (len < wp->lenPostData) {

			pHeader = (PARAM_HEADER_Tp)&wp->postData[len];

			pHeader->len = WORD_SWAP(pHeader->len);
			len += sizeof(PARAM_HEADER_T);

			if ( sscanf(&pHeader->signature[TAG_LEN], "%02d", &ver) != 1)
				ver = -1;

			force = -1;
			if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_TAG, TAG_LEN) )
				force = 1; // update
			else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_FORCE_TAG, TAG_LEN))
				force = 2; // force
			else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN))
				force = 0; // upgrade

			if ( force >= 0 ) {
				if ( !force && (ver < CURRENT_SETTING_VER || // version is less than current
					(pHeader->len < (sizeof(APMIB_T)+1)) ) { // length is less than current
					status = 0;
					break;
				}
				ptr = &wp->postData[len];
//				dump_mem(ptr,100);
				DECODE_DATA(ptr, pHeader->len);
				if ( !CHECKSUM_OK(ptr, pHeader->len)) {
					status = 0;
					break;
				}
				swap_mib_word_value((APMIB_Tp)ptr);
				apmib_updateFlash(CURRENT_SETTING, ptr, pHeader->len-1, force, ver);
				len += pHeader->len;
				type |= CURRENT_SETTING;
				continue;
			}

			if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_TAG, TAG_LEN) )
				force = 1;	// update
			else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_FORCE_TAG, TAG_LEN) )
				force = 2;	// force
			else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) )
				force = 0;	// upgrade

			if ( force >= 0 ) {
				if ( (ver < DEFAULT_SETTING_VER) || // version is less than current
					(pHeader->len < (sizeof(APMIB_T)+1)) ) { // length is less than current
					status = 0;
					break;
				}
				ptr = &wp->postData[len];
				DECODE_DATA(ptr, pHeader->len);
				if ( !CHECKSUM_OK(ptr, pHeader->len)) {
					status = 0;
					break;
				}

				swap_mib_word_value((APMIB_Tp)ptr);
				apmib_updateFlash(DEFAULT_SETTING, ptr, pHeader->len-1, force, ver);
				len += pHeader->len;
				type |= DEFAULT_SETTING;
				continue;
			}

			if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_TAG, TAG_LEN) )
				force = 1;	// update
			else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_FORCE_TAG, TAG_LEN) )
				force = 2;	// force
			else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) )
				force = 0;	// upgrade

			if ( force >= 0 ) {
				if ( (ver < HW_SETTING_VER) || // version is less than current
					(pHeader->len < (sizeof(HW_SETTING_T)+1)) ) { // length is less than current
					status = 0;
					break;
				}
				ptr = &wp->postData[len];
				DECODE_DATA(ptr, pHeader->len);
				if ( !CHECKSUM_OK(ptr, pHeader->len)) {
					status = 0;
					break;
				}
				apmib_updateFlash(HW_SETTING, ptr, pHeader->len-1, force, ver);
				len += pHeader->len;
				type |= HW_SETTING;
				continue;
			}
		}
		if (status == 0 || type == 0) // checksum error
			strcpy(tmpBuf, T("<script>if(typeof(aspformSaveConfigInvCfg) !=\"undefined\"){ document.write(showText(aspformSaveConfigInvCfg));}else{ document.write(\"Invalid configuration file!\");}</script>"));
		else {
			if (type) { // upload success
				if (apmib_reinit() ==0) {
					strcpy(tmpBuf,T("Re-initialize AP MIB failed!\n"));
					goto back;
				}
				reset_user_profile();  // re-initialize user password

			system("/bin/reset.sh gw 2 &");
			}
			strcpy(tmpBuf, T("<script>dw(UpdateSuccessfully)</script>"));


		}

back:

		ERR_MSG(wp, tmpBuf);
		free(buf);
		return;
	}
	char_t *strIsconvertmode;
	strRequest = websGetVar(wp, T("reset"), T(""));
	strIsconvertmode = websGetVar(wp,T("Conver_Mode_Default"),T(""));
	//printf("run rest %s",strRequest);
	if (strRequest[0]) {
	if ( !gstrcmp(strIsconvertmode,T("1")) )
        	system("/bin/scriptlib_util.sh flashDefault 1");
        else
		system("/bin/scriptlib_util.sh flashDefault");

	system("/bin/scriptlib_util.sh reloadFlash");
	system("/bin/setssidmac.sh");
	system("/bin/AutoWPA \`flash get HW_NIC0_ADDR | cut -b14-\` wepmac");

	//printf("run rest s2");
/*
		if ( !apmib_updateDef() ) {
			free(ptr);
			strcpy(tmpBuf, "Write default to current setting failed!\n");
			goto back;
		}
*/

		//restart system init script
	system("/bin/reset.sh gw 2 &");


	//printf("run rest s3");
		if (apmib_reinit() ==0) {
			free(ptr);
			strcpy(tmpBuf, "Re-initialize AP MIB failed!\n");
			goto back;
		}
		reset_user_profile();  // re-initialize user password
		strcpy(tmpBuf, T("Reload settings successfully. Please wait system restart for a while!"));
		//printf("run rest s4");
		submitUrl = websGetVar(wp, T("submit-url"), T(""));
		OK_MSG3(tmpBuf, submitUrl)

/*	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	sprintf(tmpBuf, T("Reboot successfully !<br><br>Please wait a while for rebooting..."));
	system("/bin/reboot.sh");
	}
}
/*void formUploadConfig(webs_t wp, char_t *path, char_t *query)//Edx patrick add
{
	char_t *submitUrl;
	char *ptr=NULL;
	PARAM_HEADER_Tp pHeader;
	int len, status=0, ver, force;
	char tmpBuf[200];

	CONFIG_DATA_T type=0;
	len = 0;
	status = 1;
	type = 0;
	printf("**********************\n");
 printf("*    Upload Config   *\n");
 printf("**********************\n");
//#ifdef _BOA_
  int iestr_offset;
  iestr_offset = parseMultiPart(wp);
  wp->postData = wp->upload_data + iestr_offset;
  wp->lenPostData = wp->upload_len - iestr_offset;
//#endif

	while (len < wp->lenPostData) {
		pHeader = (PARAM_HEADER_Tp)&wp->postData[len];


		len += sizeof(PARAM_HEADER_T);

		if ( sscanf(&pHeader->signature[TAG_LEN], "%02d", &ver) != 1)
			ver = -1;

		force = -1;
		if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_TAG, TAG_LEN) )
			force = 1; // update
		else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_FORCE_TAG, TAG_LEN))
			force = 2; // force
		else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN))
			force = 0; // upgrade

		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}

			apmib_updateFlash(CURRENT_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= CURRENT_SETTING;
			continue;
		}
		if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_TAG, TAG_LEN) ){
			force = 1;	// update
		}	
		else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_FORCE_TAG, TAG_LEN) ){
			force = 2;	// force
		}	
		else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) ){
			force = 0;	// upgrade
		}
		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}


			apmib_updateFlash(DEFAULT_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= DEFAULT_SETTING;
			continue;
		}

		if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_TAG, TAG_LEN) )
			force = 1;	// update
		else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_FORCE_TAG, TAG_LEN) )
			force = 2;	// force
		else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) )
			force = 0;	// upgrade

		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}
			apmib_updateFlash(HW_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= HW_SETTING;
			continue;
		}

	}
	if (status == 0 || type == 0){ // checksum error

		strcpy(tmpBuf, T("<script>dw(Invalidconfig)</script>")); 

 }
	else {
		if (type) { // upload success
			if (apmib_reinit() ==0) {
				strcpy(tmpBuf,T("Re-initialize AP MIB failed!\n"));
				goto back;
			}
			reset_user_profile();  // re-initialize user password

		system("reboot.sh &");
		}
		//	strcpy(tmpBuf, T("Update successfully."));
		strcpy(tmpBuf, T("<script>document.write(showText(Reloadsuccess)+' '+showText(SystemRestarting))</script>"));

	}
back:

	if (status == 0 || type == 0) // checksum error
	{
		ERR_MSG(wp, tmpBuf);
	}
	else{
		DEFAULTandGOTOurl(wp, tmpBuf, "/index.asp");
	}	

	free(ptr);
	return;
}*/
void formwizResetDefault(webs_t wp, char_t *path, char_t *query) //EDX patrick add 20130508
{
	char *ptr=NULL;
	char tmpBuf[200], tmpBuf1[200];
	int router_mode=0, intOne=1, intZero=0;

	printf("**********************\n");
	printf(" WIZ Reset Default   *\n");
	printf("**********************\n");

	
	apmib_set( MIB_WIZ_MODE, (void *)&router_mode);
	apmib_set( MIB_IQSET_DISABLE, (void *)&intZero);
	apmib_set( MIB_IS_RESET_DEFAULT, (void *)&intZero);
	apmib_update(CURRENT_SETTING);

	system("/bin/reboot.sh 2 &");
	if (apmib_reinit() ==0) {
		free(ptr);
		strcpy(tmpBuf, "Re-initialize AP MIB failed!\n");
		strcpy(tmpBuf1, "");
		goto back;
	}
	reset_user_profile();  // re-initialize user password
	//strcpy(tmpBuf, T("<script>document.write(showText(Reloadsuccess)+'&nbsp;&nbsp;'+showText(SystemRestarting))</script>"));
	strcpy(tmpBuf, T("showText(resettodefault)"));
	strcpy(tmpBuf1, T("showText(Resettowizard)"));
		
back:
	OK_MSG_WIZDEFAULT(tmpBuf, tmpBuf1);
  free(ptr);
	return;
}
void formResetDefault(webs_t wp, char_t *path, char_t *query) //EDX patrick add
{
		char *ptr=NULL;
		char tmpBuf[200];
		printf("**********************\n");
		printf("*    Reset Default   *\n");
		printf("**********************\n");

		system("/bin/scriptlib_util.sh flashDefault");

		system("/bin/reboot.sh 2 &");

		if (apmib_reinit() ==0) {
			free(ptr);
			strcpy(tmpBuf, "Re-initialize AP MIB failed!\n");
			goto back;
		}
		reset_user_profile();  // re-initialize user password
		strcpy(tmpBuf, T("<script>document.write(showText(resettodefault))</script>"));
		//printf("run rest s4");

back:
		DEFAULTandGOTOurl(wp, tmpBuf, "/index.asp");
  	free(ptr);
	//	free(buf);
		return;
}
void formSaveConfigSec(webs_t wp, char_t *path, char_t *query)
{
	char_t *submitUrl;
	char *buf, *ptr=NULL;
	PARAM_HEADER_Tp pHeader;
	int len, status=0, ver, force;
	char tmpBuf[200];

	CONFIG_DATA_T type=0;
	len = 0;
	status = 1;
	type = 0;

	int iestr_offset;
	iestr_offset = parseMultiPart(wp);
	wp->postData = wp->upload_data + iestr_offset;
	wp->lenPostData = wp->upload_len - iestr_offset;

	sleep(1);

	while (len < wp->lenPostData) {
		pHeader = (PARAM_HEADER_Tp)&wp->postData[len];

		len += sizeof(PARAM_HEADER_T);

		if ( sscanf(&pHeader->signature[TAG_LEN], "%02d", &ver) != 1)
			ver = -1;

		force = -1;
		if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_TAG, TAG_LEN) )
			force = 1; // update
		else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_FORCE_TAG, TAG_LEN))
			force = 2; // force
		else if ( !memcmp(pHeader->signature, CURRENT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN))
			force = 0; // upgrade

		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}

			apmib_updateFlash(CURRENT_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= CURRENT_SETTING;
			continue;
		}
		if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_TAG, TAG_LEN) )
			force = 1;	// update
		else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_FORCE_TAG, TAG_LEN) )
			force = 2;	// force
		else if ( !memcmp(pHeader->signature, DEFAULT_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) )
			force = 0;	// upgrade

		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}

			apmib_updateFlash(DEFAULT_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= DEFAULT_SETTING;
			continue;
		}

		if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_TAG, TAG_LEN) )
			force = 1;	// update
		else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_FORCE_TAG, TAG_LEN) )
			force = 2;	// force
		else if ( !memcmp(pHeader->signature, HW_SETTING_HEADER_UPGRADE_TAG, TAG_LEN) )
			force = 0;	// upgrade

		if ( force >= 0 ) {
			ptr = &wp->postData[len];
			DECODE_DATA(ptr, pHeader->len);
			if ( !CHECKSUM_OK(ptr, pHeader->len)) {
				status = 0;
				break;
			}
			apmib_updateFlash(HW_SETTING, ptr, pHeader->len-1, force, ver);
			len += pHeader->len;
			type |= HW_SETTING;
			continue;
		}

	}

	sleep(1);

	if (status == 0 || type == 0) // checksum error
	{
		strcpy(tmpBuf, T("<script>dw(Invalidconfig)</script>"));
	}
	else {
		if (type) { // upload success
			if (apmib_reinit() ==0) {
				strcpy(tmpBuf,T("Re-initialize AP MIB failed!\n"));
				goto back;
			}

			sleep(1);
			reset_user_profile();  // re-initialize user password

			system("reboot.sh 15 &");
		}
		strcpy(tmpBuf, T("<script>document.write(showText(Reloadsuccess)+' '+showText(SystemRestarting))</script>"));
	}
back:

	if (status == 0 || type == 0) // checksum error
	{
		system("echo 1 > /tmp/InvalidConfig");
		system("/bin/webs_check_ForuploadConfig.sh &");
	}
	else
	{
		system("rm -rf /tmp/BootComplete");	
	}

	free(buf);

	return;
}

///////////////////////////////////////////////////////////////////////////////
void formUpload(webs_t wp, char_t * path, char_t * query)
{
	system("echo 1 > /tmp/upgradeNow");
	printf("\n##### formUpload ########\n");
    int fh;
    int		 len;
    int          locWrite;
    int          numLeft;
    int          numWrite;
    IMG_HEADER_Tp pHeader;
    char tmpBuf[200], tmpBuf2[100];
    char_t *strRequest, *submitUrl, *strVal, *strTarget;
    int flag, startAddr=-1, startAddrWeb=-1, intVal=0; //defEnable=0;	//removed by Erwin
    int target=0; // 0 upgrade router ; 1 upgrade adsl module
    int pid;
    {
	int iestr_offset;

	iestr_offset = parseMultiPart(wp);
	wp->postData = wp->upload_data + iestr_offset;
	wp->lenPostData = wp->upload_len - iestr_offset;
    }

  strRequest = websGetVar(wp, T("save"), T(""));

if (strRequest[0]) {
	int fh=0;
	char *buf=NULL;

	char *filename=FLASH_DEVICE_NAME;
	int imageLen=-1;
	IMG_HEADER_T header;

	strVal = websGetVar(wp, T("readAddr"), T(""));
	if ( strVal[0] )
		startAddr = strtol( strVal, (char **)NULL, 16);

	strVal = websGetVar(wp, T("size"), T(""));
	if ( strVal[0] )
		imageLen = strtol( strVal, (char **)NULL, 16);

	fh = open(filename, O_RDONLY);
	if ( fh == -1 ) {
      		strcpy(tmpBuf, T("Open file failed!"));
		goto ret_err;
	}
	if (startAddr==-1 || imageLen==-1) {
		// read system image
		lseek(fh, CODE_IMAGE_OFFSET, SEEK_SET); // Lance 2003.06.15
		if ( read(fh, &header, sizeof(header)) != sizeof(header)) {
     			strcpy(tmpBuf, T("Read image header error!"));
			goto ret_err;
		}

		if ( memcmp(header.signature, FW_HEADER, SIGNATURE_LEN) ) {  // Lance 2003.06.15
			printf("upgrade fail: Invalid file format! ( %s should be %s )\n",
			header.signature,
			FW_HEADER);
			strcpy(tmpBuf, T("Invalid file format!"));
			goto ret_err;
  	}

		startAddr = CODE_IMAGE_OFFSET; // Lance 2003.06.15
		imageLen =  sizeof(header) + header.len;
		printf("imageLen:%d\n",imageLen);
	}

	buf = malloc(0x10000);
	if ( buf == NULL) {
    strcpy(tmpBuf, T("Allocate buffer failed!"));
		goto ret_err;
	}

	lseek(fh, startAddr, SEEK_SET);

	websWrite(wp, "HTTP/1.0 200 OK\n");
	websWrite(wp, "Content-Type: application/octet-stream;\n");
	websWrite(wp, "Content-Disposition: attachment;filename=\"apcode.bin\" \n");
	websWrite(wp, "Pragma: no-cache\n");
	websWrite(wp, "Cache-Control: no-cache\n");
	websWrite(wp, "\n");

	while (imageLen > 0) {
		int blocksize=0x10000;
		if (imageLen < blocksize)
			blocksize=imageLen;

		if ( read(fh, buf, blocksize) != blocksize) {
	     		strcpy(tmpBuf, T("Read image error!"));
			goto ret_err;
		}
		websWriteBlock(wp, (char *)buf, blocksize);
		imageLen -= blocksize;
	}



	{
  	unsigned short checksum, checksum_org;
		unsigned long readLen = 0;
		unsigned char readBuf[1024];
		unsigned long i;
		int _flashfd;


//		if((_flashfd = open(FLASH_DEVICE_NAME, O_RDWR))<0)
		if((_flashfd = open(UPG_DEVICE_NAME, O_RDWR))<0)
		{
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		for(i=0; i < 0x40000; i+=0x10000)
		{
			lseek(_flashfd, 0x10000 + i, SEEK_SET);
			read(_flashfd, readBuf, 12);

			if(memcmp(readBuf, "CSYS", 4))
				continue;
			readLen = *((unsigned long *)&readBuf[8]);

			printf("\n\nFound CSYS image in %lx, Len = %lx\n", 0x10000 + i, readLen);
			break;
		}

		if ( i == 0x40000)
		{
			close(_flashfd);
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		checksum = 0;
		for(i=0 ;i < (readLen); i += 2)
		{
			read(_flashfd, readBuf, 2);
			checksum += *(unsigned short*)readBuf;
		}

		checksum_org = *(unsigned short*)readBuf;

		printf("\n checksum_org = %lx, checksum=%lx\n", checksum_org, checksum);
		if(checksum != 0)
		{
         	strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}
	}

	websDone(wp, 200);
	goto ret_ok;

ret_err:
		system("echo 1 > /tmp/Invalidfw");
		//ERR_MSG(wp, tmpBuf);
ret_ok:
		if (fh>0)
			close(fh);
		if (buf)
			free(buf);
		return;
 }

  // assume as firmware upload
strVal = websGetVar(wp, T("writeAddrCode"), T(""));
if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddr = strtol( &strVal[2], (char **)NULL, 16);
    }
    strVal = websGetVar(wp, T("writeAddrWebPages"), T(""));
    if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddrWeb = strtol( &strVal[2], (char **)NULL, 16);
    }
    submitUrl = websGetVar(wp, T("submit-url"), T(""));
    locWrite = 0;
    numLeft = wp->lenPostData;
    pHeader = (IMG_HEADER_Tp) wp->postData;



	if ((!memcmp(&wp->postData[MODTAG_LEN], _WEB_HEADER_, SIGNATURE_LEN)) || (!memcmp(&wp->postData[MODTAG_LEN], "XZKJ", SIGNATURE_LEN))) {
    	flag = 2;
	}
	else {
		printf("upgrade fail: Invalid file format! ( %s should be %s or %s )\n", &wp->postData[MODTAG_LEN], _WEB_HEADER_,FW_HEADER);

		strcpy(tmpBuf, T("Invalid file format!"));
	
    goto ret_upload;
  }


    len = pHeader->len;
    len  = DWORD_SWAP(len);

	printf("f/w length=%x, checksum=%x\n", len,*((unsigned short *)&wp->postData[sizeof(IMG_HEADER_T) + len-2]));

	if ( !fwChecksumOk(&wp->postData[sizeof(IMG_HEADER_T)], len)) {
		sprintf(tmpBuf, T("<b>Image checksum mismatched!, checksum=0x%x</b><br>"),
						fwChecksum(&wp->postData[sizeof(IMG_HEADER_T)], len-2));
		goto ret_upload;
	}

//    fh = open(FLASH_DEVICE_NAME, O_RDWR);

	fh = open(UPG_DEVICE_NAME, O_RDWR);

    if ( fh == -1 ) {
       	strcpy(tmpBuf, T("File open failed!"));
    } else {


	if (flag == 1) {
		if ( startAddr == -1)
			startAddr = CODE_IMAGE_OFFSET;
	}
	else {
		if ( startAddrWeb == -1)
			startAddr = WEB_PAGE_OFFSET;
		else
			startAddr = startAddrWeb;
	}
	printf("mtd=%s,startAddr=%d\n",UPG_DEVICE_NAME,startAddr);

		UPGRADE_PROCESS_MSG(); //EDX
		printf(">>> go to UPGRADE_PROCESS_MSG()\n");
	
	lseek(fh, startAddr, SEEK_SET);	//Modified lance 2005
	close(wp->fd); //EDX shakim, close network socket
	printf(">>> close network socket\n");

			system("echo \"LED BLINK 100\" > /dev/NewFW_LED");

	system("/bin/killapp.sh"); //EDX 
	numWrite = write(fh, &(wp->postData[locWrite]), numLeft);	//lance 2005



	if (numWrite < numLeft) {	//lance 2005
		sprintf(tmpBuf, T("File write failed.<br> locWrite=%d numLeft=%d numWrite=%d Size=%d bytes."), locWrite, numLeft, numWrite, wp->lenPostData);
		printf("locWrite=%d numLeft=%d numWrite=%d Size=%d bytes.",locWrite,numLeft, numWrite, wp->lenPostData);

	}
	locWrite += numWrite;
	numLeft -= numWrite;	//lance 2005

  pid_t pid;
  pid = fork();
  if (pid == 0){
    close(fh);
    exit(0);
  }
	else
		waitpid(pid, NULL, 0);

	if (numLeft == 0)
		sprintf(tmpBuf, T("Update successfully (size = %d bytes)!<br><br>Please wait a while for rebooting..."), wp->lenPostData);
	else
		sprintf(tmpBuf, T("numLeft=%d locWrite=%d Size=%d bytes."), numLeft, locWrite, wp->lenPostData);

}


	sprintf(tmpBuf2, "fsync %s", UPG_DEVICE_NAME);
	system(tmpBuf2);
	printf(">>> %s done!\n");

	system("/bin/reboot.sh 3 &");
	printf(">>> call /bin/reboot.sh\n");
	websDone(wp, 200);
  return;

ret_upload:
	system("echo 1 > /tmp/Invalidfw");
  //ERR_MSG(wp, tmpBuf);
  return;

}

//////////////////////////////////////////////////////////////////////////////
void formBootUpload(webs_t wp, char_t * path, char_t * query)
{
	int numWrite = 0, numLeft = 0, locWrite = 0, fh = 0, len = 0;
	IMG_HEADER_Tp pHeader;
	char_t  *submitUrl;
	char    tmpBuf[64];
    {
        int iestr_offset;

        iestr_offset = parseMultiPart(wp);
        wp->postData = wp->upload_data + iestr_offset;
        wp->lenPostData = wp->upload_len - iestr_offset;
    }

	printf("enter boot upload\n");
	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	numLeft = wp->lenPostData;

	fh = open(BOOT_DEVICE_NAME, O_RDWR);
	if ( fh == -1 ){
		strcpy(tmpBuf, T("Invalid file format!"));
		goto ret_upload;
	}
	lseek(fh, 0, SEEK_SET);
	numWrite = write(fh, &(wp->postData[locWrite]), numLeft);

	if (numWrite < numLeft){
		strcpy(tmpBuf, T("Invalid file size!"));
		close (fh);
		goto ret_upload;
	}

	locWrite += numWrite;
	numLeft -= numWrite;
	close(fh);

	if(numLeft == 0){
                char_t *strnextEnable;
                strnextEnable = websGetVar(wp, T("new_apply"), T(""));
                if (strnextEnable[0])
                {
                        system("/bin/reset.sh gw 2");
                }
                REPLACE_MSG(submitUrl);
		return;
	}

ret_upload:
	ERR_MSG(wp, tmpBuf);
	return;

} // formBootUpload

//////////////////////////////////////////////////////////////////////////////
/*void formPassword(webs_t wp, char_t *path, char_t *query)
{
	char_t *submitUrl, *strOPassword, *strNPassword;
	char tmpBuf[100];

	strOPassword = websGetVar(wp, T("oldpass"), T(""));
	apmib_get(MIB_USER_PASSWORD, (void *)tmpBuf);
	strNPassword = websGetVar(wp, T("newpass"), T(""));

	printf("\noldpass=%s\n",strOPassword);
	printf("\nMIB_USER_PASSWORD=%s\n",tmpBuf);
	printf("\nnewpass=%s\n",strNPassword);

	if ( strOPassword[0] || strNPassword[0])
	{
		if ( ! strcmp(strOPassword,tmpBuf )) {
        	  if ( umUserExists("admin"))
        	       umDeleteUser("admin");

         	  if ( umAddUser("admin", strNPassword, DEFAULT_GROUP, FALSE, FALSE) ) {
           	       strcpy(tmpBuf, T("ERROR: Password setting failed !"));
            	       goto setErr_pass;
	      	  }
		}
		else{
        	     strcpy(tmpBuf, T("ERROR: Password is not matched !"));
        	     goto setErr_pass;
		}
	}
	// Retrieve next page URL
	apmib_update(CURRENT_SETTING);

	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page

	OK_MSG(wp, submitUrl);
	return;

setErr_pass:
	ERR_MSG(wp, tmpBuf);
}*/
/////////////////////////////////////////////////////////////////////////////
void formPasswordSetup(webs_t wp, char_t *path, char_t *query)
{
	char_t *submitUrl, /**strUser,*/ *strPassword, *userid, *nextUserid, *strOPassword, *app_callreboot;
	char tmpBuf[100], strUser[100];
	apmib_get(MIB_USER_NAME, (void *)strUser);

	strPassword = websGetVar(wp, T("newpass"), T(""));
	strOPassword = websGetVar(wp, T("oldpass"), T(""));
	apmib_get(MIB_USER_PASSWORD, (void *)tmpBuf);
//	printf("\nUsername=%-10s oldpass=%-10s MIB_USER_PASSWORD=%-10s newpass=%-10s\n",strUser,strOPassword,tmpBuf,strPassword);
	if (  strcmp(strOPassword,tmpBuf )) {
			strcpy(tmpBuf, T("ERROR: Password is not matched !"));
			goto setErr_pass;
	}
	if ( strUser[0] ) {
		// Check if user name is the same as supervisor name
		if ( !apmib_get(MIB_SUPER_NAME, (void *)tmpBuf)) {
			strcpy(tmpBuf, T("ERROR: Get supervisor name MIB error!"));
			goto setErr_pass;
		}
		if ( !strcmp(strUser, tmpBuf)) {
			strcpy(tmpBuf, T("ERROR: Cannot use the same user name as supervisor."));
			goto setErr_pass;
		}
		// Check if supervisor account exist. if not, create it
		if ( !umGroupExists(DEFAULT_GROUP) )
			if ( umAddGroup(DEFAULT_GROUP, (short)PRIV_ADMIN, AM_BASIC, FALSE, FALSE) ) {
				strcpy(tmpBuf, T("ERROR: Unable to add group."));
				goto setErr_pass;
			}
		if ( !umAccessLimitExists(ACCESS_URL) )
			if ( umAddAccessLimit(ACCESS_URL, AM_FULL, (short)0, DEFAULT_GROUP) ) {
				strcpy(tmpBuf, T("ERROR: Unable to add access limit."));
				goto setErr_pass;
			}
		if ( !umUserExists(superName))
			if ( umAddUser(superName, superPass, DEFAULT_GROUP, FALSE, FALSE) ) {
				strcpy(tmpBuf, T("ERROR: Unable to add supervisor account."));
				goto setErr_pass;
			}

		// Add new one
		if ( umUserExists(strUser))
			umDeleteUser(strUser);

		if ( umAddUser(strUser, strPassword, DEFAULT_GROUP, FALSE, FALSE) ) {
			strcpy(tmpBuf, T("ERROR: Unable to add user account."));
			goto setErr_pass;
		}
	}
	else {
		// Set NULL account, delete supervisor from DB
		umDeleteAccessLimit("/");
		umDeleteUser(superName);
		umDeleteGroup(DEFAULT_GROUP);
	}
	/* Delete current user account */
	userid = umGetFirstUser();
	while (userid) {
		if ( gstrcmp(userid, superName) && gstrcmp(userid, strUser)) {
			nextUserid = umGetNextUser(userid);
			if ( umDeleteUser(userid) ) {
				strcpy(tmpBuf, T("ERROR: Unable to delete user account."));
				goto setErr_pass;
			}
			userid = nextUserid;
			continue;
		}
		userid = umGetNextUser(userid);
	}
	if (umCommit(NULL) != 0) {
		strcpy(tmpBuf, T("ERROR: Unable to save user configuration."));
		goto setErr_pass;
	}
	/* Set user account to MIB */
	if ( !apmib_set(MIB_USER_NAME, (void *)strUser) ) {
		strcpy(tmpBuf, T("ERROR: Set user name to MIB database failed."));
		goto setErr_pass;
	}

	if ( !apmib_set(MIB_USER_PASSWORD, (void *)strPassword) ) {
		strcpy(tmpBuf, T("ERROR: Set user password to MIB database failed."));
		goto setErr_pass;
	}

	char_t *iqsetupclose;
	iqsetupclose = websGetVar(wp, T("iqsetupclose"), T(""));
	if (iqsetupclose[0]){
		int close_iQsetup=1;
		apmib_set(MIB_IQSET_DISABLE, (void *)&close_iQsetup); // close iqsetup
	}

	/* Retrieve next page URL */
	apmib_update(CURRENT_SETTING);

		system("/bin/scriptlib_util.sh flash_to_data_structure_for_openvpn_user");
		system("/bin/scriptlib_util.sh reloadFlash");
		system("killall -SIGUSR1 webs");

	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page

	app_callreboot = websGetVar(wp, T("APP_call_DUTreboot"), T(""));   

	if(app_callreboot[0]){
		system("/bin/reboot.sh 1 &");
	}
	else{
		//REPLACE_MSG(submitUrl); // Tommy Modified on 0602
		REDIRECT_PAGE(wp, submitUrl);
	}
	return;

setErr_pass:
	ERR_MSG(wp, tmpBuf);
}

////////////////////////////////////////////////////////////////////
void set_user_profile()
{
	/* first time load, get mib */
	if ( !apmib_get( MIB_SUPER_NAME, (void *)superName ) ||
		!apmib_get( MIB_SUPER_PASSWORD, (void *)superPass ) ||
			!apmib_get( MIB_USER_NAME, (void *)userName ) ||
				!apmib_get( MIB_USER_PASSWORD, (void *)userPass ) ) {
		error(E_L, E_LOG, T("Get user account MIB failed"));
		return;
	}
	/* Create umconfig.txt if necessary */
	if ( userName[0] ) {
		/* Create supervisor */
		if ( !umGroupExists(DEFAULT_GROUP) )
			if ( umAddGroup(DEFAULT_GROUP, (short)PRIV_ADMIN, AM_BASIC, FALSE, FALSE) ) {
				error(E_L, E_LOG, T("ERROR: Unable to add group."));
				return;
			}
		if ( !umAccessLimitExists(ACCESS_URL) )
			if ( umAddAccessLimit(ACCESS_URL, AM_FULL, (short)0, DEFAULT_GROUP) ) {
				error(E_L, E_LOG, T("ERROR: Unable to add access limit."));
				return;
			}
		if ( !umUserExists(superName))
			if ( umAddUser(superName, superPass, DEFAULT_GROUP, FALSE, FALSE) ) {
				error(E_L, E_LOG, T("ERROR: Unable to add supervisor account."));
				return;
			}

		/* Create user */
		if ( umUserExists(userName))
			umDeleteUser(userName);

		if ( umAddUser(userName, userPass, DEFAULT_GROUP, FALSE, FALSE) ) {
			error(E_L, E_LOG, T("ERROR: Unable to add user account."));
			return;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
void formStats(webs_t wp, char_t *path, char_t *query)
{
	char_t *submitUrl;

	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page

	if (submitUrl[0])
		websRedirect(wp, submitUrl);
}




int  opModeHandler(webs_t wp, char *tmpBuf)
{
	char_t *tmpStr;
	int opmode, wanId;
	int apmode;
	int repeater_enabled;

	tmpStr = websGetVar(wp, T("opMode"), T(""));
	if(tmpStr[0]){
		opmode = tmpStr[0] - '0' ;
		if ( apmib_set(MIB_OP_MODE, (void *)&opmode) == 0) {
			strcpy(tmpBuf, T("Set Opmode error!"));
			goto setErr_opmode;
		}
		if (opmode == 1) {
			apmode=0;
			if ( apmib_set(MIB_AP_MODE, (void *)&apmode) == 0) {
				DEBUGP(tmpBuf, T("Set WLAN Mode error!"));
				goto setErr_opmode;
			}
			repeater_enabled=1;
			if ( apmib_set(MIB_REPEATER_ENABLED, (void *)&repeater_enabled) == 0) {
				DEBUGP(tmpBuf, T("Set Universal Repeater Mode error!"));
				goto setErr_opmode;
			}
		}
		else {
			repeater_enabled=0;
			if ( apmib_set(MIB_REPEATER_ENABLED, (void *)&repeater_enabled) == 0) {
				DEBUGP(tmpBuf, T("Set Universal Repeater Mode error!"));
				goto setErr_opmode;
			}
		}
	}
	tmpStr = websGetVar(wp, T("wispWanId"), T(""));
	if(tmpStr[0]){
		wanId = tmpStr[0] - '0' ;
		if ( apmib_set(MIB_WISP_WAN_ID, (void *)&wanId) == 0) {
			strcpy(tmpBuf, T("Set WISP WAN Id error!"));
			goto setErr_opmode;
		}
	}
	return 0;

setErr_opmode:
	return -1;

}
void formOpMode(webs_t wp, char_t *path, char_t *query)
{
	char_t *submitUrl;
	char tmpBuf[100];

	int pid;
	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page

	if(opModeHandler(wp, tmpBuf) < 0)
		goto setErr;

	apmib_update(CURRENT_SETTING);

	/*pid = fork();
        if (pid)
		waitpid(pid, NULL, 0);
        else if (pid == 0) {
		snprintf(tmpBuf, 100, "%s/%s", _CONFIG_SCRIPT_PATH,  _CONFIG_SCRIPT_PROG);

		execl( tmpBuf, _CONFIG_SCRIPT_PROG, "gw", "all", NULL);
               	exit(1);
       	} */ // Removed by Tommy
                char_t *strnextEnable;
                strnextEnable = websGetVar(wp, T("new_apply"), T(""));
                if (strnextEnable[0])
                {
                        system("/bin/reset.sh gw 2");
                }
                REPLACE_MSG(submitUrl);
	return;

setErr:
	ERR_MSG(wp, tmpBuf);
}
void formrefresh(webs_t wp, char_t *path, char_t *query)
{

 		system("echo 0 > /tmp/auto_refresh_nbtscan"); //EDX, Robert add, "0:Close 1:Run" auto refresh nbtscan.

	char command[100];
	char lanIP[30];
	char_t  *submitUrl;

			struct in_addr	intaddr;
			if ( getInAddr(BRIDGE_IF, IP_ADDR, (void *)&intaddr ) )
				sprintf(lanIP, "%s", inet_ntoa(intaddr) );
			else
				sprintf(lanIP, "%s", "0.0.0.0" );

			int i=strlen(lanIP);
			while(lanIP[i]!='.') i--;
			lanIP[i]='\0';
			sprintf(command,"/bin/nbtscan -m -s %s.0/24 > /var/netName.var",lanIP); system(command);

	submitUrl = websGetVar(wp, T("submit-url"), T(""));   // hidden page
	websRedirect(wp, submitUrl);

}
void formUploadBootCode(webs_t wp, char_t * path, char_t * query)
{
    int fh;
    int		 len;
    int          locWrite;
    int          numLeft;
    int          numWrite;
    IMG_HEADER_Tp pHeader;
    char tmpBuf[200];
    char_t *strRequest, *submitUrl, *strVal, *strTarget;
    int flag, startAddr=-1, startAddrWeb=-1, intVal=0; //defEnable=0;	//removed by Erwin
    int target=0; // 0 upgrade router ; 1 upgrade adsl module
    int upgTarget=0; //0 firmware ,1 bootcode added by vance
    int pid;
        printf("\nkillall processes\n");



main_menu=atoi((char_t *)websGetVar(wp, T("main_menu_number"), T("0")));
sub_menu=atoi((char_t *)websGetVar(wp, T("sub_menu_number"), T("0")));
  strRequest = websGetVar(wp, T("save"), T(""));
if (strRequest[0]) {
	int fh=0;
	char *buf=NULL;

	char *filename=FLASH_DEVICE_NAME;
	int imageLen=-1;
	IMG_HEADER_T header;

	strVal = websGetVar(wp, T("readAddr"), T(""));
	if ( strVal[0] )
		startAddr = strtol( strVal, (char **)NULL, 16);

	strVal = websGetVar(wp, T("size"), T(""));
	if ( strVal[0] )
		imageLen = strtol( strVal, (char **)NULL, 16);

	fh = open(filename, O_RDONLY);
	if ( fh == -1 ) {
      		strcpy(tmpBuf, T("Open file failed!"));
		goto ret_err;
	}
	if (startAddr==-1 || imageLen==-1) {
		// read system image
		lseek(fh, CODE_IMAGE_OFFSET, SEEK_SET); // Lance 2003.06.15
		if ( read(fh, &header, sizeof(header)) != sizeof(header)) {
     			strcpy(tmpBuf, T("Read image header error!"));
			goto ret_err;
		}

		if ( memcmp(header.signature, FW_HEADER, SIGNATURE_LEN) ) {  // Lance 2003.06.15
				printf("upgrade fail: Invalid file format! ( %s should be %s )\n",
								header.signature,
								FW_HEADER);
       			strcpy(tmpBuf, T("Invalid file format!"));
			goto ret_err;
	    	}



		startAddr = CODE_IMAGE_OFFSET; // Lance 2003.06.15
		imageLen =  sizeof(header) + header.len;
	}

	buf = malloc(0x10000);
	if ( buf == NULL) {
       		strcpy(tmpBuf, T("Allocate buffer failed!"));
		goto ret_err;
	}

	lseek(fh, startAddr, SEEK_SET);

	websWrite(wp, "HTTP/1.0 200 OK\n");
	websWrite(wp, "Content-Type: application/octet-stream;\n");
	websWrite(wp, "Content-Disposition: attachment;filename=\"apcode.bin\" \n");
	websWrite(wp, "Pragma: no-cache\n");
	websWrite(wp, "Cache-Control: no-cache\n");
	websWrite(wp, "\n");

	while (imageLen > 0) {
		int blocksize=0x10000;
		if (imageLen < blocksize)
			blocksize=imageLen;

		if ( read(fh, buf, blocksize) != blocksize) {
	     		strcpy(tmpBuf, T("Read image error!"));
			goto ret_err;
		}
		websWriteBlock(wp, (char *)buf, blocksize);
		imageLen -= blocksize;
	}



	{
    	unsigned short checksum, checksum_org;
		unsigned long readLen = 0;
		unsigned char readBuf[1024];
		unsigned long i;
		int _flashfd;


//		if((_flashfd = open(FLASH_DEVICE_NAME, O_RDWR))<0)
		if((_flashfd = open(UPG_DEVICE_NAME, O_RDWR))<0)
		{
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		for(i=0; i < 0x40000; i+=0x10000)
		{
			lseek(_flashfd, 0x10000 + i, SEEK_SET);
			read(_flashfd, readBuf, 12);
			if(memcmp(readBuf, "CSYS", 4))
				continue;
			readLen = *((unsigned long *)&readBuf[8]);

			printf("\n\nFound CSYS image in %lx, Len = %lx\n", 0x10000 + i, readLen);
			break;
		}

		if ( i == 0x40000)
		{
			close(_flashfd);
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		checksum = 0;
		for(i=0 ;i < (readLen); i += 2)
		{
			read(_flashfd, readBuf, 2);
			checksum += *(unsigned short*)readBuf;
		}

		checksum_org = *(unsigned short*)readBuf;

		printf("\n checksum_org = %lx, checksum=%lx\n", checksum_org, checksum);
		if(checksum != 0)
		{
         	strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}
	}

	websDone(wp, 200);
	goto ret_ok;

ret_err:
	ERR_MSG(wp, tmpBuf);
ret_ok:
	if (fh>0)
		close(fh);
	if (buf)
		free(buf);
	return;
   }

    // assume as firmware upload
    strVal = websGetVar(wp, T("writeAddrCode"), T(""));
    if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddr = strtol( &strVal[2], (char **)NULL, 16);
    }
    strVal = websGetVar(wp, T("writeAddrWebPages"), T(""));
    if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddrWeb = strtol( &strVal[2], (char **)NULL, 16);
    }
    submitUrl = websGetVar(wp, T("submit-url"), T(""));
    locWrite = 0;
    numLeft = wp->lenPostData;
    pHeader = (IMG_HEADER_Tp) wp->postData;
	if ( !memcmp(&wp->postData[0], "BOOT", SIGNATURE_LEN) )   // Vance 2009.02.05
		upgTarget=1;
	if ((!memcmp(&wp->postData[MODTAG_LEN], "EWAS", SIGNATURE_LEN))
					|| (!memcmp(&wp->postData[MODTAG_LEN], "WBIP", SIGNATURE_LEN))) {
		if (memcmp(&wp->postData[MODTAG_LEN], _WEB_HEADER_, SIGNATURE_LEN)) {
		}
    	flag = 2;
	}

    len = pHeader->len;
    len  = DWORD_SWAP(len);

	apmib_set( MIB_CLT_CER_FILE, (void *)&intVal );
	apmib_set( MIB_SER_CER_FILE, (void *)&intVal );
	apmib_update(CURRENT_SETTING);
	printf("f/w length=%x, checksum=%x\n", len,*((unsigned short *)&wp->postData[sizeof(IMG_HEADER_T) + len-2]));

	if ( !fwChecksumOk(&wp->postData[sizeof(IMG_HEADER_T)], len)) {
		sprintf(tmpBuf, T("<b>Image checksum mismatched!, checksum=0x%x</b><br>"),
						fwChecksum(&wp->postData[sizeof(IMG_HEADER_T)], len-2));
		goto ret_upload;
	}

	if(upgTarget == 0)
	{
		strVal = websGetVar(wp, T("fwface"), T(""));
		printf("fw control=%s\n",strVal);
		if (!gstrcmp(strVal, T("1")))
			fh = open(UPG2_DEVICE_NAME, O_RDWR);
		else
			fh = open(UPG_DEVICE_NAME, O_RDWR);
	}
    if ( fh == -1 ) {
       	strcpy(tmpBuf, T("File open failed!"));
    } else {


/*	if (defEnable == 1) {	//removed by Erwin
	    lseek(fh, DEFAULT_SETTING_OFFSET, SEEK_SET);
	    write(fh, "\0\0\0\0", 4);
	}
*/
	if (flag == 1) {
		if ( startAddr == -1)
			startAddr = CODE_IMAGE_OFFSET;
	}
	else {
		if ( startAddrWeb == -1)
			startAddr = WEB_PAGE_OFFSET;
		else
			startAddr = startAddrWeb;
	}
		startAddr = 0;
		lseek(fh, startAddr+WEB_PAGE_OFFSET, SEEK_SET);
		numWrite = write(fh, &(wp->postData[locWrite+WEB_PAGE_OFFSET]), numLeft-WEB_PAGE_OFFSET);


		if (numWrite < numLeft-WEB_PAGE_OFFSET) {	//lance 2005

		sprintf(tmpBuf, T("File write failed.<br> locWrite=%d numLeft=%d numWrite=%d Size=%d bytes."), locWrite, numLeft, numWrite, wp->lenPostData);

	}
	locWrite += numWrite;

	 	numLeft -= (numWrite+WEB_PAGE_OFFSET);	//lance 2005
	close(fh);

	if (numLeft == 0)
		sprintf(tmpBuf, T("<script>function getCookie(c_name)\n{\nif (document.cookie.length>0)\n{\nc_start=document.cookie.indexOf(c_name + \"=\")\nif (c_start!=-1)\n{\nc_start=c_start + c_name.length+1;\nc_end=document.cookie.indexOf(\";\",c_start);\nif (c_end==-1) c_end=document.cookie.length;\nreturn unescape(document.cookie.substring(c_start,c_end));\n}\n}\nif (c_name=='language')\n{\nreturn 1;\n}\nelse\n{\nreturn 0;\n}\n}\nvar stype = getCookie('language');\nfunction showText(text)\n{\nreturn text[stype];\n}\nvar aspUpgradeSuccess=new Array(\"Update successfully. <br><br>Please wait a while for rebooting...\",\"ファームウェアの更新に成功しました\")\ndocument.write(showText(aspUpgradeSuccess))</script>"));
	else
		sprintf(tmpBuf, T("numLeft=%d locWrite=%d Size=%d bytes."), numLeft, locWrite, wp->lenPostData);

    }


	//add by Kyle 20080229.

		#define REBOOT       0x040000
		int fd;
		fd = open("/dev/reset_but", O_RDONLY);
		if (fd < 0) {
			printf("reset_but 3333\n");
			perror("/dev/reset_but");
			return -1;
		}

		if (ioctl(fd, REBOOT, NULL) < 0) {
			perror("ioctl reset button");
			close(fd);
			return -1;
		}
		close(fd);
    return;

ret_upload:
    ERR_MSG(wp, tmpBuf);
    return;

}
int ConnectionList(int eid, webs_t wp, int argc, char_t **argv)
{
	int	nBytesSent=0;
	
	return nBytesSent;
}

void formAutoFWupgrade(webs_t wp, char_t *path, char_t *query) {

	char_t *submitUrl;
	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	
	FILE *fp_tmp;
	//char fw_url[128],correct_md5[125],downloadfw_md5[125],fw_version[30],product_name[100];
	char current_version[30], buffer[20], buffer2[500], tmpBuf2[100];
	char_t	*action;
	int isNewAvailable = 0;
	//int isServerAlive = 0;

	action = websGetVar(wp, T("action"), T(""));
	printf("\naction=%s\n",action);
	//get data form firmware_info.txt

	if (!strcmp(action, T("fail"))){
		submitUrl = websGetVar(wp, T("failurl"), T(""));
		REPLACE_MSG(submitUrl);
		return;
	}

	if (!strcmp(action, T("killps"))){
	}

	if (!strcmp(action, T("check_version"))){
		isServerAlive = 0;
		system("rm -f /tmp/fw_info.txt");
		sleep(3);
		if( (fp_tmp = fopen("/tmp/fw_info.txt","r"))!=NULL){
			isServerAlive = 1;

			fgets(product_name,sizeof(product_name),fp_tmp);
			if (product_name[strlen(product_name) - 1] == '\n')
			{	
				if (product_name[strlen(product_name) - 2] == '\r')
					product_name[strlen(product_name) - 2] = '\0';
				else		
					product_name[strlen(product_name) - 1] = '\0';
			}

			fgets(fw_version,sizeof(fw_version),fp_tmp);
			if (fw_version[strlen(fw_version) - 1] == '\n')
			{	
				if (fw_version[strlen(fw_version) - 2] == '\r')
					fw_version[strlen(fw_version) - 2] = '\0';
				else
					fw_version[strlen(fw_version) - 1] = '\0';
			}

			fgets(correct_md5,sizeof(correct_md5),fp_tmp);
			if (correct_md5[strlen(correct_md5) - 1] == '\n')
			{
				if (correct_md5[strlen(correct_md5) - 2] == '\r')
					correct_md5[strlen(correct_md5) - 2] = '\0';
				else
					correct_md5[strlen(correct_md5) - 1] = '\0';
			}

			fgets(zip_url,sizeof(zip_url),fp_tmp);
			if (zip_url[strlen(zip_url) - 1] == '\n')
			{
				if (zip_url[strlen(zip_url) - 2] == '\r')
					zip_url[strlen(zip_url) - 2] = '\0';
				else
					zip_url[strlen(zip_url) - 1] = '\0';
			}

			fgets(fw_url,sizeof(fw_url),fp_tmp);
			if (fw_url[strlen(fw_url) - 1] == '\n')
			{
				if (fw_url[strlen(fw_url) - 2] == '\r')
					fw_url[strlen(fw_url) - 2] = '\0';
				else
					fw_url[strlen(fw_url) - 1] = '\0';
			}

			printf("\n\n[%s]\n[%s]\n[%s]\n[%s]\n[%s]\n\n", 
							product_name, fw_version, correct_md5, zip_url, fw_url); 
			fclose (fp_tmp);
		}
		else
			isServerAlive = 0;
		REPLACE_MSG(submitUrl);
		return;
	}
	if (!strcmp(action, T("download_FW"))){ //download and check md5
		REPLACE_MSG(submitUrl);
		system("echo 1 > /tmp/upgradeNow");
		//download fw
		char commend[100];
		printf(">>> download fw from %s\n",fw_url);
		sprintf( commend, "getfw.sh %s &",fw_url);
		system(commend);
		return;
	}
	
	if (!strcmp(action, T("checkFWfile"))){ //download and check md5
		int downloadOK = 0;
		downloadOK = 1;
		sprintf( buffer, "%s" ,"download_ok");
	}

	if (!strcmp(action, T("install_FW"))){ //install fw and reboot

		int fh, len, locWrite, numLeft, numWrite;
		int flag=0, startAddr=-1, startAddrWeb=-1, intVal=0, size=0;
		int target=0; // 0 upgrade router ; 1 upgrade adsl module
		
		IMG_HEADER_Tp pHeader;
		char tmpBuf[200];
		char_t *strRequest, *submitUrl, *strVal, *strTarget;
		char *fw_buf;
		FILE *xp;

		xp = fopen("/tmp/fw.bin","r");
		if (xp == NULL) 
		{ 
			fw_buf = NULL;
			printf(">>> error to read /tmp/fw.bin\n");
			goto retError;
		}
		
		fseek(xp, 0, SEEK_END);
		size = ftell(xp);
		fseek(xp, 0, SEEK_SET);
		numLeft = size;
		
		fh = open(UPG_DEVICE_NAME, O_RDWR);  
		if ( fh == -1 ) {
			strcpy(tmpBuf, T("File open failed!"));	
			printf(">>> File open failed\n");
			goto retError;
		} 
		else {

			startAddr = 0x0000;
			
			UPGRADE_PROCESS_MSG(); //EDX
			printf(">> go to UPGRADE_PROCESS_MSG()\n");
			
			lseek(fh, startAddr, SEEK_SET);	
			close(wp->fd);	// close network socket, for web page  count down
			printf(">> close network socket\n");

					system("echo \"LED BLINK 100\" > /dev/NewFW_LED");
			system("/bin/killapp.sh");

		}

		int Writed=0;
		unsigned char flashWbuf[4096];
		int read=0;
		numWrite=0;
		while (!feof(xp))
		{
			read=fread(flashWbuf,sizeof(char),sizeof(flashWbuf),xp);
			Writed = write(fh, flashWbuf, read);
			numWrite=numWrite+Writed;
			/*if (numWrite!=0)
			{
				printf("write %d \n",numWrite);
			}*/
		}

		printf(">>> fw write done! \n");

		close(fh);
		fclose(xp);
		printf(">>> fclose done\n");

		sprintf(tmpBuf2, "fsync %s", UPG_DEVICE_NAME);
		system(tmpBuf2);

		printf(">>> fsync done\n");

		//system("/bin/reboot.sh 30 &");//EDX
		system("kill 1");
		printf(">>> system kill 1\n");

		sprintf( buffer, "%s" ,"download_ok");
		//return;
	}

	printf("\n[jquery get data: %s]\n",buffer);

	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;

retError:
	system("echo 1 > /tmp/Invalidfw");
  return;

}

void formSystemLog(webs_t wp, char_t *path, char_t *query)
{
	char_t *strSave;
	FILE *fp;
	unsigned char tempbuf[100];
	int rdSize;

    // Save security log to file
	strSave = websGetVar(wp, T("export"), T(""));
	if (strSave[0]) {
		websWrite(wp, "HTTP/1.0 200 OK\n");
		websWrite(wp, "Content-Type: application/octet-stream;\n");
		websWrite(wp, "Content-Disposition: attachment;filename=\"SystemLog.txt\" \n");
		websWrite(wp, "Pragma: no-cache\n");
		websWrite(wp, "Cache-Control: no-cache\n");
		websWrite(wp, "\n");

		if ((fp = fopen("/var/log/SystemLog","r"))!=NULL) {
			while ((rdSize = fread(tempbuf, 1, 100, fp)) !=0 )
				websWriteDataNonBlock(wp, tempbuf, rdSize);
			fclose(fp);
		}
		websDone(wp, 200);
		return;
	}
}

void formWiFiRomaingFWupgrade(webs_t wp, char_t *path, char_t *query) {

	char_t *submitUrl;
	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	FILE *fp_tmp;
	char buffer[20];
	char_t	*action;

	action = websGetVar(wp, T("action"), T(""));

	if (!strcmp(action, T("check_server_version"))){
		//system("/bin/CSV_SMTS.sh &");
		//system("/bin/check_server_version.sh &");
		//system("/bin/SendMsgToSlave.sh &");
		system("echo 1 > /tmp/CheckFW");
		system("echo 1 > /tmp/OneTime");
		system("echo 1 > /tmp/Rload");
		system("killall -SIGUSR1 airfores");
		system("/bin/check_server_version.sh &");
		REPLACE_MSG(submitUrl);
		return;
	}

	if (!strcmp(action, T("download_server_version"))){
                FILE* fpt;
		system("echo 1 > /tmp/DownloadFW");
		system("echo 2 > /tmp/OneTime");
		system("echo 1 > /tmp/Rload");
		system("rm -rf /tmp/xxx");
		system("/bin/download_server_version.sh &");
		sleep(1);
		system("killall -SIGUSR1 airfores");
		REPLACE_MSG(submitUrl);
		return;
	}

	REPLACE_MSG(submitUrl);
	return;
}

void formGALog(webs_t wp, char_t *path, char_t *query)
{
	char_t *strSave;
	FILE *fp;
	unsigned char tempbuf[500000];
	int rdSize;

    // Save security log to file
	strSave = websGetVar(wp, T("export2"), T(""));
	if (strSave[0]) {
		websWrite(wp, "HTTP/1.0 200 OK\n");
		websWrite(wp, "Content-Type: application/octet-stream;\n");
		websWrite(wp, "Content-Disposition: attachment;filename=\"GA_device_log.txt\" \n");
		websWrite(wp, "Pragma: no-cache\n");
		websWrite(wp, "Cache-Control: no-cache\n");
		websWrite(wp, "\n");

		if ((fp = fopen("/var/log/GA_device_log","r"))!=NULL) {
			while ((rdSize = fread(tempbuf, 1, sizeof(tempbuf), fp)) !=0 )
			{
				printf("rdSize=%d\n",rdSize);
				websWriteDataNonBlock(wp, tempbuf, rdSize);
			}
			fclose(fp);
		}
		websDone(wp, 200);
		return;
	}
}
void formSlaveResult(webs_t wp, char_t *path, char_t *query) {
	char buffer[10] = {0};
	char_t *str;
	FILE *fp;
	FILE *fp2;
	char Command[10], Command2[10];
	int intVal=0, intVal2=0;

	str = websGetVar(wp, T("cmd"), T(""));
	if (!strcmp(str, T("checkSlaveFW"))){
			if((fp=fopen("/tmp/SlaveNum","r"))!=NULL)
			{
				fgets(Command,sizeof(Command),fp);
				intVal=atoi(Command);
				fclose(fp);
			}
			else
			{
				intVal=0;
			}
			system("rm -rf /tmp/Slave_Data");
			sleep(1);
			system("echo `ls /tmp/Slave_*|wc -l |head -n 1` > /tmp/SlaveRealNumber");
			sleep(1);
			if ((fp2 = fopen("/tmp/SlaveRealNumber","r"))!=NULL)
			{
				fgets(Command2,sizeof(Command2),fp2);
				intVal2=atoi(Command2);
				fclose(fp2);
			}
			else
			{
				intVal2=0;
			}
			
			if (intVal != "0")
			{
				if (intVal == intVal2) {
					sprintf(buffer, "%s", "SlaveResultGet");
					sleep(2);
				}
				else if (intVal > intVal2)
				{
					sprintf(buffer, "%s", "LoseSlave");
				}
			}
			else
			{
				sprintf(buffer, "%s", "NoSlave");
			}
	}
	printf("\n[formSlaveResult: jquery data:%s]\n",buffer);
	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;
}
void formSlaveFW(webs_t wp, char_t *path, char_t *query) {
	char buffer[10] = {0}, tmp[5] = {0};
	char_t *str;
	FILE *fp;
	FILE *fp2;
	FILE *fp3;
	char Command[10], Command2[10];
	int intVal=0, intVal2=0, intVal3=0;

	str = websGetVar(wp, T("cmd"), T(""));
	if (!strcmp(str, T("checkSlaveFW"))){
			if((fp=fopen("/tmp/SlaveNum","r"))!=NULL)
			{
				fgets(Command,sizeof(Command),fp);
				intVal=atoi(Command);
				fclose(fp);
			}
			else
			{
				intVal=0;
			}
			//system("rm -rf /tmp/Slave_Data");
			//sleep(1);
			system("echo `ls /tmp/Slave_*|grep -v Slave_Data|wc -l |head -n 1` > /tmp/SlaveRealNumber");
			sleep(1);
			if ((fp2 = fopen("/tmp/SlaveRealNumber","r"))!=NULL)
			{
				fgets(Command2,sizeof(Command2),fp2);
				intVal2=atoi(Command2);
				fclose(fp2);
			}
			else
			{
				intVal2=0;
			}

			UpgradeProcessCount=UpgradeProcessCount+1;
			printf("UpgradeProcessCount=%d\n",UpgradeProcessCount);
			if (intVal != "0")
			{

				if (intVal == intVal2) {
					//check Master_Data Slave_Data Status=value
					system("/bin/Build_Data.sh");
					if ((fp3 = fopen("/tmp/FWstatus","r"))!=NULL)
					{
						fgets(tmp,2,fp3);
						fclose(fp3);
						intVal3 = atoi(tmp);
						if(intVal3 == 1)
						{
							sprintf(buffer, "%s", "SlaveResultGet");
							system("/bin/AllFWupgrade.sh &");
						}
						else
						{
							if (UpgradeProcessCount == 30)
							{
								UpgradeProcessCount=0;
								system("/bin/killUpgradeProcessFile.sh &");
							}
							sprintf(buffer, "%s", "FWstatusFail");
						}
					}
				}
				else if (intVal > intVal2)
				{
					if (UpgradeProcessCount == 30)
					{
						UpgradeProcessCount=0;
						system("/bin/killUpgradeProcessFile.sh &");
					}
					sprintf(buffer, "%s", "LoseSlave");
				}
			}
			else
			{
				if (UpgradeProcessCount == 30)
				{
					UpgradeProcessCount=0;
					system("/bin/killUpgradeProcessFile.sh &");
				}
				sprintf(buffer, "%s", "NoSlave");
			}
	}
	printf("\n[formSlaveFW: jquery data:%s]\n",buffer);
	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;
}

void formTestResult(webs_t wp, char_t *path, char_t *query) {
	char buffer[20];
	char_t *str;
	FILE *fp;
	char Command[200];
	char linkType[20];

	str = websGetVar(wp, T("cmd"), T(""));


	if (!strcmp(str, T("checkWlanTest"))){

		snprintf(Command, sizeof(Command), "/tmp/connectTestdone");
		if ((fp = fopen(Command,"r")) != NULL) {
			sprintf(buffer, "%s", "connectTestdone");
			fclose(fp);
		}
		else{
			if(connectIF == 5) snprintf(Command, sizeof(Command), "/tmp/apclii0IP");
			else snprintf(Command, sizeof(Command), "/tmp/apcli0IP");
			if ((fp = fopen(Command,"r")) != NULL) {
				sprintf(buffer, "%s", "GetIP");
				fclose(fp);
			}
			else{
				if(!SSIDexist) sprintf(buffer, "%s", "SSID_no_exist");
				else sprintf(buffer, "%s", "WaitIP");
			}
		}
	}

	if (!strcmp(str, T("checkWlanIF"))){
		printf("connectIF is %d\n",connectIF);
		sprintf(buffer, "%d", connectIF);

		sprintf(Command, "echo 1 > /tmp/showConnectResult%d", connectIF); // let wiz_ip5g.asp | wiz_ip.asp select autoIP
		system(Command);

		system("rm -f /tmp/connectTestdone 2> /dev/null");
		system("killall udhcpc 2> /dev/null");
		system("rm -f /tmp/SSIDname 2> /dev/null"); //for hidden SSID. EDX patrick
	}
	
	if (!strcmp(str, T("checkWanTest"))){
		if( (fp = fopen("/tmp/DetectInternetDone","r"))!=NULL){
			fclose (fp);
			system("rm -f /tmp/DetectInternetDone 2> /dev/null");
			sprintf(buffer, "%s", "DetectDone");
		}
		else
			sprintf(buffer, "%s", "Detect");
	}

	printf("\n[formTestResult: jquery data:%s]\n",buffer);
	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;
}
void formJQueryData(webs_t wp, char_t *path, char_t *query) {
	char buffer[20];
	char_t	*action;
	FILE *fp;

	action = websGetVar(wp, T("action"), T(""));
	printf("action=%s \n",action);


	if (!strcmp(action, T("checkFWupgrade"))){
		if ((fp = fopen("/tmp/BootComplete","r"))!=NULL) {
			system("rm -f /tmp/BootComplete");
			sprintf( buffer, "%s" ,"upgrade_ok");
			fclose(fp);
		}
	}

	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;

}

void formSaveText(webs_t wp, char_t *path, char_t *query)
{
	struct in_addr inIp;
	char_t *submitUrl;
	int i,j,k, countChar, countLine;
	char *ptr=NULL, *ptr1=NULL, *ptr2=NULL, *token=NULL;
	char tmpBuf[200],tmpBuf1[200],tmpBuf2[200];
	char *startp=NULL, *endp=NULL;

	char SettingArray[23][22]= {
		"2.4G Wi-Fi Name=",
		"2.4G CHANNEL=",
		"2.4G Security Method=",
		"2.4G WPA Type=",
		"2.4G WPA Format=",
		"2.4G WEP Length=",
		"2.4G WEP Format=",
		"2.4G Wi-Fi Key=",
		"5G Wi-Fi Name=",
		"5G CHANNEL=",
		"5G Security Method=",
		"5G WPA Type=",
		"5G WPA Format=",
		"5G WEP Length=",
		"5G WEP Format=",
		"5G Wi-Fi Key=",
		"Mode=",
		"Auto IP=",
		"IP Adress=",
		"Subnet Mask=",
		"Default Gateway=",
		"Login User Name=",
		"Login Password="
	};

	/* check format */
	wp->postData = wp->upload_data;	
	ptr = strstr(wp->postData,"/* Text file header */");                                                   
	ptr1 = strstr(wp->postData,"/* Text file end */");
	
	if((ptr==NULL) || (ptr1==NULL)){
		strcpy(tmpBuf, T("File header/end error!")); 
		goto setErr_up;
	}
	int line=0;
	int intvalue=0;
	
	int int1=1, int2=2, intzero=0, method, pskformat, intSet, len, weplen, wepformat, wepkeyLen, ssidlen;

		ptr = strstr(wp->postData, SettingArray[line]);
	
		if(ptr==NULL){
			strcpy(tmpBuf, T("Name 1 error! ( ex. 2.4G Wi-Fi Name=xxxxxx; )")); 
			goto setErr_up;
		}

		/* start read */
		token = strtok(ptr, "\n");

		/* list all setting */
		while(token != NULL)
		{

			printf("token:[%s]\n", token);

			/* setting done */
			if(strstr(token, "/* Text file end */") != NULL)
				break;

			startp = strstr(token, "=");
			endp = strstr(token, ";");
			/* reset */
			memset(tmpBuf, '\0', sizeof(tmpBuf));

			if((startp!=NULL) && (endp!=NULL)){
				/* get value */
				countChar=endp-startp;
				strncpy(tmpBuf, startp+1, countChar-1);
			}
			else
			{
				sprintf(tmpBuf,"Name %d Format error! ( Setting Name=xxxxxx; )",line+1); 
				goto setErr_up;
			}

			/* set mib, skip null value except login password, wifi key */
			if(tmpBuf[0] || line==22 || line==15 || line==7)
			{	
				/* check format */
				if(strstr(token , SettingArray[line]) == NULL){
					sprintf(tmpBuf,"Name %d Format error! ( %sxxxxxx; )",line+1, SettingArray[line]); 
					goto setErr_up;
				}

				if(line==0){     // SSID
						if (strlen(tmpBuf) > 32) {
							strcpy(tmpBuf, T("2.4G Wi-Fi Name error! (length: 1-32)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_WLAN_SSID, (void *)tmpBuf) == 0) {
							strcpy(tmpBuf, T("MIB_WLAN_SSID error!")); 
							goto setErr_up;
						}
				}
				else if(line==1){ // CHANNEL
						intvalue = strtol( tmpBuf, (char **)NULL, 10);
						if ( apmib_set(MIB_WLAN_CHAN_NUM, (void *)&intvalue) == 0) {
							strcpy(tmpBuf, T("MIB_WLAN_CHAN_NUM error!")); 
							goto setErr_up;
						}
				}
				else if(line==2){ // DISABLE/WEP/WPA
						method = strtol( tmpBuf, (char **)NULL, 10);
						if (method!=0 && method!=1 && method!=2) {
							strcpy(tmpBuf, T("2.4G Security Method error! (0:Disable 1:WEP 2:WPA)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_SECURITY_MODE, (void *)&method) == 0) {
							strcpy(tmpBuf, T("MIB_SECURITY_MODE error!")); 
							goto setErr_up;
						}
						if(method==2 || method==3){
							if ( apmib_set(MIB_WLAN_WPA_AUTH, (void *)&int2) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WPA_AUTH error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==3){  // TKIP/AES/MIXED
						if(method!=2)
						{
							printf("2.4G WPA Mode skip\n");
						}
						else
						{
							intvalue = strtol( tmpBuf, (char **)NULL, 10);
							if (intvalue!=1 && intvalue!=2 && intvalue!=3) {
								strcpy(tmpBuf, T("2.4G WPA Mode error! (1:TKIP 2:AES 3:Mixed )")); 
								goto setErr_up;
							}
							intSet=0;
							if(intvalue==1 || intvalue==3) intSet=1;
							if ( apmib_set(MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WPA_CIPHER_SUITE error!")); 
								goto setErr_up;
							}
							intSet=0;
							if(intvalue==2 || intvalue==3) intSet=2;
							if ( apmib_set(MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WPA2_CIPHER_SUITE error!")); 
								goto setErr_up;
							}
							if (method==0)	intSet=0;
							else if (method==1) intSet=1;
							else if ((method==2) && (intvalue==1)) intSet=2;
							else if ((method==2) && (intvalue==2)) intSet=4;
							else if ((method==2) && (intvalue==3)) intSet=6;
							else goto setErr_up;
							if ( apmib_set(MIB_WLAN_ENCRYPT, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_ENCRYPT error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==4){  // PHASS/HEX
						if(method!=2)
						{
							printf("2.4G WPA Format skip\n");
						}
						else
						{
							pskformat = strtol( tmpBuf, (char **)NULL, 10);
							if (pskformat!=0 && pskformat!=1) {
								strcpy(tmpBuf, T("2.4G WPA Format error! (0:Passphrase, length: 8-63  1:HEX, length: 64)")); 
								goto setErr_up;
							}
							if ( apmib_set(MIB_WLAN_WPA_PSK_FORMAT, (void *)&pskformat) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WPA_PSK_FORMAT error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==5){  //  WEP64/WEP128
						if(method!=1)
						{
							printf("2.4G WEP Length skip\n");
						}
						else
						{
							weplen = strtol( tmpBuf, (char **)NULL, 10);
							if (weplen!=1 && weplen!=2) {
								strcpy(tmpBuf, T("2.4G WEP Length error! (1:64 bit  2:128 bit)")); 
								goto setErr_up;
							}
							if ( apmib_set( MIB_WLAN_WEP, (void *)&weplen) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WEP error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==6){//  ascii 5/10,hex 13/26
						if(method!=1)
						{
							printf("2.4G WEP Format skip\n");
						}
						else
						{
							wepformat = strtol( tmpBuf, (char **)NULL, 10);
							if (wepformat!=0 && wepformat!=1) {
								strcpy(tmpBuf, T("2.4G WEP Format error! (1:ASCII, length: 5/10  2:HEX, length: 13/26)")); 
								goto setErr_up;
							}
							if ( apmib_set( MIB_WLAN_WEP_KEY_TYPE, (void *)&wepformat) == 0) {
								strcpy(tmpBuf, T("MIB_WLAN_WEP_KEY_TYPE error!")); 
								goto setErr_up;
							}
							wepformat+=1;
							if (weplen == WEP64) {
								if (wepformat==1)
									wepkeyLen = WEP64_KEY_LEN;   //WEP64_KEY_LEN=5
								else
									wepkeyLen = WEP64_KEY_LEN*2;
							}
							else if (weplen == WEP128) {
								if (wepformat==1)
									wepkeyLen = WEP128_KEY_LEN;  //WEP128_KEY_LEN=13
								else
									wepkeyLen = WEP128_KEY_LEN*2;
							}
							else
									goto setErr_up;
						}
				}
				else if(line==7){  // wifi KEY
						if(method==0)
						{
							printf("2.4G Wi-Fi Key skip\n");
						}
						else
						{
							if(method==2)  
							{//WPA
								len = strlen(tmpBuf);
								if (pskformat==1) { // hex
									apmib_get(MIB_WLAN_WPA_PSK, (void *)tmpBuf2);
									if (len!=MAX_PSK_LEN || !string_to_hex(tmpBuf, tmpBuf2, MAX_PSK_LEN)) {
										strcpy(tmpBuf, T("2.4G Wi-Fi Key error! (HEX length: 64)")); 
										goto setErr_up;
									}
								}
								else {              // passphras
									if (len < 8 || len > (MAX_PSK_LEN-1) ) {
										strcpy(tmpBuf, T("2.4G Wi-Fi Key error! (Passphrase length: 8-63)")); 
										goto setErr_up;
									}
								}
								if ( !apmib_set(MIB_WLAN_WPA_PSK, (void *)tmpBuf)) {
									strcpy(tmpBuf, T("MIB_WLAN_WPA_PSK error!")); 
									goto setErr_up;
								}
							}
							if(method==1)            
							{//WEP
								if ( apmib_set( MIB_WLAN_WEP_DEFAULT_KEY, (void *)&intzero ) == 0) {
									strcpy(tmpBuf, T("MIB_WLAN_WEP_DEFAULT_KEY error!")); 
									goto setErr_up;
								}
								if (strlen(tmpBuf) != wepkeyLen) {
									strcpy(tmpBuf, T("2.4G Wi-Fi Key error!<br><br>64 bit: ASCII length=5 / HEX length=13<br>128 bit: ASCII length=10 / HEX length=26")); 
									goto setErr_up;
								}
								if (wepformat == 1) // ascii
								{
									if ( !apmib_set(MIB_TMPSTRING, (void *)tmpBuf)) {
										strcpy(tmpBuf, T("MIB_TMPSTRING error!")); 
										goto setErr_up;
									}
									strcpy(tmpBuf1, tmpBuf);
								}
								else { // hex
									if ( !string_to_hex(tmpBuf, tmpBuf1, wepkeyLen)) {
										strcpy(tmpBuf, T("string_to_hex error!")); 
										goto setErr_up;
									}
								}
								if (weplen == WEP64){
									if (apmib_set(MIB_WLAN_WEP64_KEY1, (void *)tmpBuf1)== 0) {
										strcpy(tmpBuf, T("MIB_WLAN_WEP64_KEY1 error!")); 
										goto setErr_up;
									}
								}
								else if (weplen == WEP128){
									if (apmib_set(MIB_WLAN_WEP128_KEY1, (void *)tmpBuf1)== 0) {
										strcpy(tmpBuf, T("MIB_WLAN_WEP128_KEY1 error!")); 
										goto setErr_up;
									}
								}
								else
									goto setErr_up;
							}
						}
				}	
				else if(line==8){
						if (strlen(tmpBuf) > 32){
							strcpy(tmpBuf, T("5G Wi-Fi Name error! (length: 1-32)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_INIC_SSID, (void *)tmpBuf) == 0) {
							strcpy(tmpBuf, T("MIB_INIC_SSID error!")); 
							goto setErr_up;
						}
				}
				else if(line==9){
						intvalue = strtol( tmpBuf, (char **)NULL, 10);
						if ( apmib_set(MIB_INIC_CHAN_NUM, (void *)&intvalue) == 0){
							strcpy(tmpBuf, T("MIB_INIC_CHAN_NUM error!")); 
							goto setErr_up;
						}
				}
				else if(line==10){ // DISABLE/WEP/WPA
						method = strtol( tmpBuf, (char **)NULL, 10);
						if (method!=0 && method!=1 && method!=2) {
							strcpy(tmpBuf, T("5G Security Method error! (0:Disable 1:WEP 2:WPA)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_INIC_SECURITY_MODE, (void *)&method) == 0) {
							strcpy(tmpBuf, T("INIC_SECURITY_MODE error!")); 
							goto setErr_up;
						}
						if(method==2 || method==3){
							if ( apmib_set(MIB_INIC_WPA_AUTH, (void *)&int2) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WPA_AUTH error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==11){  // TKIP/AES/MIXED
						if(method!=2)
						{
							printf("5G WPA Mode skip\n");
						}
						else
						{
							intvalue = strtol( tmpBuf, (char **)NULL, 10);
							if (intvalue!=1 && intvalue!=2 && intvalue!=3) {
								strcpy(tmpBuf, T("5G WPA Mode error! (1:TKIP 2:AES 3:Mixed )")); 
								goto setErr_up;
							}
							intSet=0;
							if(intvalue==1 || intvalue==3) intSet=1;
							if ( apmib_set(MIB_INIC_WPA_CIPHER_SUITE, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WPA_CIPHER_SUITE error!")); 
								goto setErr_up;
							}
							intSet=0;
							if(intvalue==2 || intvalue==3) intSet=2;
							if ( apmib_set(MIB_INIC_WPA2_CIPHER_SUITE, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WPA2_CIPHER_SUITE error!")); 
								goto setErr_up;
							}
							if (method==0)	intSet=0;
							else if (method==1) intSet=1;
							else if ((method==2) && (intvalue==1)) intSet=2;
							else if ((method==2) && (intvalue==2)) intSet=4;
							else if ((method==2) && (intvalue==3)) intSet=6;
							else goto setErr_up;
							if ( apmib_set(MIB_INIC_ENCRYPT, (void *)&intSet) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_ENCRYPT error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==12){  // PHASS/HEX
						if(method!=2)
						{
							printf("5G WPA Format skip\n");
						}
						else
						{
							pskformat = strtol( tmpBuf, (char **)NULL, 10);
							if (pskformat!=0 && pskformat!=1) {
								strcpy(tmpBuf, T("5G WPA Format error! (0:Passphrase, length: 8-63  1:HEX, length: 64)")); 
								goto setErr_up;
							}
							if ( apmib_set(MIB_INIC_WPA_PSK_FORMAT, (void *)&pskformat) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WPA_PSK_FORMAT error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==13){  //  WEP64/WEP128
						if(method!=1)
						{
							printf("5G WEP Length skip\n");
						}
						else
						{
							weplen = strtol( tmpBuf, (char **)NULL, 10);
							if (weplen!=1 && weplen!=2) {
								strcpy(tmpBuf, T("5G WEP Length error! (1:64 bit  2:128 bit)")); 
								goto setErr_up;
							}
							if ( apmib_set( MIB_INIC_WEP, (void *)&weplen) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WEP error!")); 
								goto setErr_up;
							}
						}
				}
				else if(line==14){//  ascii 5/10,hex 13/26
						if(method!=1)
						{
							printf("5G WEP Format skip\n");
						}
						else
						{
							wepformat = strtol( tmpBuf, (char **)NULL, 10);
							if (wepformat!=0 && wepformat!=1) {
								strcpy(tmpBuf, T("5G WEP Format error! (1:ASCII, length: 5/10  2:HEX, length: 13/26)")); 
								goto setErr_up;
							}
							if ( apmib_set( MIB_INIC_WEP_KEY_TYPE, (void *)&wepformat) == 0) {
								strcpy(tmpBuf, T("MIB_INIC_WEP_KEY_TYPE error!")); 
								goto setErr_up;
							}
							wepformat+=1;
							if (weplen == WEP64) {
								if (wepformat==1)
									wepkeyLen = WEP64_KEY_LEN;   //WEP64_KEY_LEN=5
								else
									wepkeyLen = WEP64_KEY_LEN*2;
							}
							else if (weplen == WEP128) {
								if (wepformat==1)
									wepkeyLen = WEP128_KEY_LEN;  //WEP128_KEY_LEN=13
								else
									wepkeyLen = WEP128_KEY_LEN*2;
							}
							else
									goto setErr_up;
						}
				}
				else if(line==15){  // wifi KEY
						if(method==0)
						{
							printf("5G Wi-Fi Key skip\n");
						}
						else
						{
							if(method==2)  
							{//WPA
								len = strlen(tmpBuf);
								if (pskformat==1) { // hex
									apmib_get(MIB_INIC_WPA_PSK, (void *)tmpBuf2);
									if (len!=MAX_PSK_LEN || !string_to_hex(tmpBuf, tmpBuf2, MAX_PSK_LEN)) {
										strcpy(tmpBuf, T("5G Wi-Fi Key error! (HEX length: 64)")); 
										goto setErr_up;
									}
								}
								else {              // passphras
									if (len < 8 || len > (MAX_PSK_LEN-1) ) {
										strcpy(tmpBuf, T("5G Wi-Fi Key error! (Passphrase length: 8-63)")); 
										goto setErr_up;
									}
								}
								if ( !apmib_set(MIB_INIC_WPA_PSK, (void *)tmpBuf)) {
										strcpy(tmpBuf, T("MIB_WLAN_5G_WPA_PSK error!")); 
										goto setErr_up;
									}
							}
							if(method==1)            
							{//WEP
								if ( apmib_set( MIB_INIC_WEP_DEFAULT_KEY, (void *)&intzero ) == 0) {
									strcpy(tmpBuf, T("MIB_INIC_WEP_DEFAULT_KEY error!")); 
									goto setErr_up;
								}
								if (strlen(tmpBuf) != wepkeyLen) {
									strcpy(tmpBuf, T("5G Wi-Fi Key error!<br><br>64 bit: ASCII length=5 / HEX length=13<br>128 bit: ASCII length=10 / HEX length=26")); 
									goto setErr_up;
								}
								if (wepformat == 1) // ascii
								{
									if ( !apmib_set(MIB_TMPSTRING_5G, (void *)tmpBuf)) {
										strcpy(tmpBuf, T("MIB_TMPSTRING_5G error!")); 
										goto setErr_up;
									}
									strcpy(tmpBuf1, tmpBuf);
								}
								else { // hex
									if ( !string_to_hex(tmpBuf, tmpBuf1, wepkeyLen)) {
										strcpy(tmpBuf, T("string_to_hex error!")); 
										goto setErr_up;
									}
								}
								if (weplen == WEP64){
									if (apmib_set(MIB_INIC_WEP64_KEY1, (void *)tmpBuf1)== 0) {
										strcpy(tmpBuf, T("MIB_INIC_WEP64_KEY1 error!")); 
										goto setErr_up;
									}
								}
								else if (weplen == WEP128){
									if (apmib_set(MIB_INIC_WEP128_KEY1, (void *)tmpBuf1)== 0) {
										strcpy(tmpBuf, T("MIB_INIC_WEP128_KEY1 error!")); 
										goto setErr_up;
									}
								}
								else
									goto setErr_up;
							}
						}
				}	
				else if(line==16){
						intvalue = strtol( tmpBuf, (char **)NULL, 10);
						if (intvalue!=0 && intvalue!=1) {
							strcpy(tmpBuf, T("Mode error! (Mode=1)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_WIZ_MODE, (void *)&intvalue) == 0) {
							strcpy(tmpBuf, T("MIB_WIZ_MODE error! ")); 
							goto setErr_up;
						}
				}
				else if(line==17){
						intvalue = strtol( tmpBuf, (char **)NULL, 10);
						if (intvalue!=0 && intvalue!=1) {
							strcpy(tmpBuf, T("Auto IP error! <br><br>0: Static IP <br>1:Obtain an IP address automatically ")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_DHCP_SWITCH, (void *)&intvalue) == 0) {
							strcpy(tmpBuf, T("MIB_DHCP_SWITCH error!")); 
							goto setErr_up;
						}
				}
				else if(line==18){
						if ( !inet_aton(tmpBuf, &inIp) ) {
							strcpy(tmpBuf, T("IP Adress error! (ex. 192.168.2.1)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_IP_ADDR, (void *)&inIp) == 0){
							strcpy(tmpBuf, T("MIB_IP_ADDR error!")); 
							goto setErr_up;
						}
				}
				else if(line==19){
						if ( !inet_aton(tmpBuf, &inIp) ) {
							strcpy(tmpBuf, T("IP Adress error! (ex. 255.255.255.0)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_SUBNET_MASK, (void *)&inIp) == 0) {
							strcpy(tmpBuf, T("MIB_SUBNET_MASK error!")); 
							goto setErr_up;
						}
				}
				else if(line==20){
						if ( !inet_aton(tmpBuf, &inIp) ) {
							strcpy(tmpBuf, T("IP Adress error! (ex. 192.168.2.233)")); 
							goto setErr_up;
						}
						if ( apmib_set(MIB_DEFAULT_GATEWAY, (void *)&inIp) == 0) {
							strcpy(tmpBuf, T("MIB_DEFAULT_GATEWAY error!")); 
							goto setErr_up;
						}
				}
				else if(line==21){
						if ( apmib_set(MIB_USER_NAME, (void *)tmpBuf) == 0) {
							strcpy(tmpBuf, T("MIB_USER_NAME error!")); 
							goto setErr_up;
						}
				}
				else if(line==22){
						if ( apmib_set(MIB_USER_PASSWORD, (void *)tmpBuf) == 0) {
							strcpy(tmpBuf, T("MIB_USER_PASSWORD error!")); 
							goto setErr_up;
						}
				}
				else{
						strcpy(tmpBuf, T("Format error! ( Setting counter > 23 )"));
						goto setErr_up;
				}

				line++;
			}

			token = strtok(NULL, "\n");
		}
		printf("\nline=%d\n",line);
		
		if(line==23){
			if ( apmib_set(MIB_IQSET_DISABLE, (void *)&int1) == 0) {
				strcpy(tmpBuf, T("MIB_IQSET_DISABLE error!")); 
				goto setErr_up;
			}

			if ( apmib_set(MIB_LICENCE, (void *)&int1) == 0) {
				strcpy(tmpBuf, T("MIB_LICENCE error!")); 
				goto setErr_up;
			}

			apmib_update(CURRENT_SETTING);
		}
		else
		{
			strcpy(tmpBuf, T("Format error! ( Setting counter < 23 )")); 
			goto setErr_up;
		}

	strcpy(tmpBuf, T("<script>document.write(showText(Reloadsuccess)+' '+showText(SystemRestarting))</script>"));
	OK_MSG3(tmpBuf, "/index.asp");
	system("/bin/reboot.sh 1 &");
	return ;

setErr_up:
	printf("\n%s\n",tmpBuf);
	ERR_MSG(wp, tmpBuf);
	return ;
}

void formAdvManagement(webs_t wp, char_t *path, char_t *query)
{
	char_t *strVerifyCode, *AdvManagement, *strInto_Effect;
	char checkVerifyCode[10] = "Edimax";
	int intVal;

	strVerifyCode = websGetVar(wp, T("AdvSetup_verifyCode"), T(""));
	if (strVerifyCode[0]){
		if (!strcmp(strVerifyCode, checkVerifyCode)){
			printf(">>Check Verify Code Success!!\n");

			AdvManagement = websGetVar(wp, T("AdvManagement"), T(""));
			if (AdvManagement[0])
			{
				intVal=atoi(AdvManagement);
				printf("AdvManagement=%d\n",intVal);
				apmib_set( MIB_ADVCONTROL_ENABLE, (void *)&intVal);
			}		
		}
		else{
			printf(">>Check Verify Code Fail!!\n");
		}
	}
	else{
			printf(">>Verify Code is Null!!\n");
	}

	/* Retrieve next page URL */
	apmib_update(CURRENT_SETTING);

	/*-- For jquery get data --*/
	char buffer[20];
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);
	/*-------------------------*/

	strInto_Effect = websGetVar(wp, T("AdvSetup_into_effect"), T(""));
	if (strInto_Effect[0]) 
		system("/bin/reboot.sh 1 &");
	return;
}

///////////////////////////////////////////////////////////////////////////////
void formUploadIE(webs_t wp, char_t * path, char_t * query)
{
	printf("\n##### formUploadIE ########\n");
    int fh;
    int		 len;
    int          locWrite;
    int          numLeft;
    int          numWrite;
    IMG_HEADER_Tp pHeader;
    char tmpBuf[200], tmpBuf2[100];
    char_t *strRequest, *submitUrl, *strVal, *strTarget;
    int flag, startAddr=-1, startAddrWeb=-1, intVal=0; //defEnable=0;	//removed by Erwin
    int target=0; // 0 upgrade router ; 1 upgrade adsl module
    int pid;
    {
	int iestr_offset;

	iestr_offset = parseMultiPart(wp);
	wp->postData = wp->upload_data + iestr_offset;
	wp->lenPostData = wp->upload_len - iestr_offset;
    }


	main_menu=atoi((char_t *)websGetVar(wp, T("main_menu_number"), T("0")));
	sub_menu=atoi((char_t *)websGetVar(wp, T("sub_menu_number"), T("0")));
	third_menu=atoi((char_t *)websGetVar(wp, T("third_menu_number"), T("0")));

  strRequest = websGetVar(wp, T("save"), T(""));

if (strRequest[0]) {
	int fh=0;
	char *buf=NULL;

	char *filename=FLASH_DEVICE_NAME;
	int imageLen=-1;
	IMG_HEADER_T header;

	strVal = websGetVar(wp, T("readAddr"), T(""));
	if ( strVal[0] )
		startAddr = strtol( strVal, (char **)NULL, 16);

	strVal = websGetVar(wp, T("size"), T(""));
	if ( strVal[0] )
		imageLen = strtol( strVal, (char **)NULL, 16);

	fh = open(filename, O_RDONLY);
	if ( fh == -1 ) {
      		strcpy(tmpBuf, T("Open file failed!"));
		goto ret_err;
	}
	if (startAddr==-1 || imageLen==-1) {
		// read system image
		lseek(fh, CODE_IMAGE_OFFSET, SEEK_SET); // Lance 2003.06.15
		if ( read(fh, &header, sizeof(header)) != sizeof(header)) {
     			strcpy(tmpBuf, T("Read image header error!"));
			goto ret_err;
		}

		if ( memcmp(header.signature, FW_HEADER, SIGNATURE_LEN) ) {  // Lance 2003.06.15
				printf("upgrade fail: Invalid file format! ( %s should be %s )\n",
								header.signature,
								FW_HEADER);
       			strcpy(tmpBuf, T("Invalid file format!"));
			goto ret_err;
	    	}



		startAddr = CODE_IMAGE_OFFSET; // Lance 2003.06.15
		imageLen =  sizeof(header) + header.len;
		printf("imageLen:%d\n",imageLen);
	}

	buf = malloc(0x10000);
	if ( buf == NULL) {
       		strcpy(tmpBuf, T("Allocate buffer failed!"));
		goto ret_err;
	}

	lseek(fh, startAddr, SEEK_SET);

	websWrite(wp, "HTTP/1.0 200 OK\n");
	websWrite(wp, "Content-Type: application/octet-stream;\n");
	websWrite(wp, "Content-Disposition: attachment;filename=\"apcode.bin\" \n");
	websWrite(wp, "Pragma: no-cache\n");
	websWrite(wp, "Cache-Control: no-cache\n");
	websWrite(wp, "\n");

	while (imageLen > 0) {
		int blocksize=0x10000;
		if (imageLen < blocksize)
			blocksize=imageLen;

		if ( read(fh, buf, blocksize) != blocksize) {
	     		strcpy(tmpBuf, T("Read image error!"));
			goto ret_err;
		}
		websWriteBlock(wp, (char *)buf, blocksize);
		imageLen -= blocksize;
	}



	{
    	unsigned short checksum, checksum_org;
		unsigned long readLen = 0;
		unsigned char readBuf[1024];
		unsigned long i;
		int _flashfd;


//		if((_flashfd = open(FLASH_DEVICE_NAME, O_RDWR))<0)
		if((_flashfd = open(UPG_DEVICE_NAME, O_RDWR))<0)
		{
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		for(i=0; i < 0x40000; i+=0x10000)
		{
			lseek(_flashfd, 0x10000 + i, SEEK_SET);
			read(_flashfd, readBuf, 12);

			if(memcmp(readBuf, "CSYS", 4))
				continue;
			readLen = *((unsigned long *)&readBuf[8]);

			printf("\n\nFound CSYS image in %lx, Len = %lx\n", 0x10000 + i, readLen);
			break;
		}

		if ( i == 0x40000)
		{
			close(_flashfd);
			strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}

		checksum = 0;
		for(i=0 ;i < (readLen); i += 2)
		{
			read(_flashfd, readBuf, 2);
			checksum += *(unsigned short*)readBuf;
		}

		checksum_org = *(unsigned short*)readBuf;

		printf("\n checksum_org = %lx, checksum=%lx\n", checksum_org, checksum);
		if(checksum != 0)
		{
         	strcpy(tmpBuf, T("Upgrade failed!"));
			goto ret_err;
		}
	}

	websDone(wp, 200);
	goto ret_ok;

ret_err:
	ERR_MSG(wp, tmpBuf);
ret_ok:
	if (fh>0)
		close(fh);
	if (buf)
		free(buf);
	return;
   }

    // assume as firmware upload
    strVal = websGetVar(wp, T("writeAddrCode"), T(""));
    if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddr = strtol( &strVal[2], (char **)NULL, 16);
    }
    strVal = websGetVar(wp, T("writeAddrWebPages"), T(""));
    if ( strVal[0] ) {
	if ( !memcmp(strVal, "0x", 2))
		startAddrWeb = strtol( &strVal[2], (char **)NULL, 16);
    }
    submitUrl = websGetVar(wp, T("submit-url"), T(""));
    locWrite = 0;
    numLeft = wp->lenPostData;
    pHeader = (IMG_HEADER_Tp) wp->postData;



	if ((!memcmp(&wp->postData[MODTAG_LEN], _WEB_HEADER_, SIGNATURE_LEN)) || (!memcmp(&wp->postData[MODTAG_LEN], "XZKJ", SIGNATURE_LEN))) {
    	flag = 2;
	}
	else {
		printf("upgrade fail: Invalid file format! ( %s should be %s or %s )\n", &wp->postData[MODTAG_LEN], _WEB_HEADER_,FW_HEADER);

		strcpy(tmpBuf, T("Invalid file format!"));
	
    goto ret_upload;
  }


    len = pHeader->len;
    len  = DWORD_SWAP(len);

	printf("f/w length=%x, checksum=%x\n", len,*((unsigned short *)&wp->postData[sizeof(IMG_HEADER_T) + len-2]));

	if ( !fwChecksumOk(&wp->postData[sizeof(IMG_HEADER_T)], len)) {
		sprintf(tmpBuf, T("<b>Image checksum mismatched!, checksum=0x%x</b><br>"),
						fwChecksum(&wp->postData[sizeof(IMG_HEADER_T)], len-2));
		goto ret_upload;
	}

//    fh = open(FLASH_DEVICE_NAME, O_RDWR);

	fh = open(UPG_DEVICE_NAME, O_RDWR);

    if ( fh == -1 ) {
       	strcpy(tmpBuf, T("File open failed!"));
    } else {


	if (flag == 1) {
		if ( startAddr == -1)
			startAddr = CODE_IMAGE_OFFSET;
	}
	else {
		if ( startAddrWeb == -1)
			startAddr = WEB_PAGE_OFFSET;
		else
			startAddr = startAddrWeb;
	}
	printf("mtd=%s,startAddr=%d\n",UPG_DEVICE_NAME,startAddr);

	UPGRADE_PROCESS_MSG(); //EDX
	lseek(fh, startAddr, SEEK_SET);	//Modified lance 2005
	close(wp->fd); //EDX shakim, close network socket
	system("rm -f /tmp/BootComplete"); //EDX, upgrade page will read this to know Boot Complete or not.
			system("echo \"LED BLINK 100\" > /dev/NewFW_LED");

	system("/bin/killapp.sh"); //EDX 
	numWrite = write(fh, &(wp->postData[locWrite]), numLeft);	//lance 2005



	if (numWrite < numLeft) {	//lance 2005


		sprintf(tmpBuf, T("File write failed.<br> locWrite=%d numLeft=%d numWrite=%d Size=%d bytes."), locWrite, numLeft, numWrite, wp->lenPostData);
		printf("locWrite=%d numLeft=%d numWrite=%d Size=%d bytes.",locWrite,numLeft, numWrite, wp->lenPostData);

	}
	locWrite += numWrite;
	numLeft -= numWrite;	//lance 2005

	//EDX
        pid_t pid;
        pid = fork();
        if (pid == 0)
        {
                close(fh);
                exit(0);
        }
        else
                waitpid(pid, NULL, 0);
	//EDX
	//EDX remove //close(fh);

	if (numLeft == 0)
	
		sprintf(tmpBuf, T("Update successfully (size = %d bytes)!<br><br>Please wait a while for rebooting..."), wp->lenPostData);

	else
		sprintf(tmpBuf, T("numLeft=%d locWrite=%d Size=%d bytes."), numLeft, locWrite, wp->lenPostData);

    }


		//add by Kyle 20080229.


		sprintf(tmpBuf2, "fsync %s", UPG_DEVICE_NAME);
		system(tmpBuf2);


		system("kill 1");


//		system("/bin/reboot.sh 2 1 &"); //EDX shakim
    return;

	main_menu=atoi((char_t *)websGetVar(wp, T("main_menu_number"), T("0")));
	sub_menu=atoi((char_t *)websGetVar(wp, T("sub_menu_number"), T("0")));
	third_menu=atoi((char_t *)websGetVar(wp, T("third_menu_number"), T("0")));

ret_upload:
    ERR_MSG(wp, tmpBuf);
    return;

}

void formAutoFWupgradeIE(webs_t wp, char_t *path, char_t *query) {

	char_t *submitUrl;
	submitUrl = websGetVar(wp, T("submit-url"), T(""));
	
	FILE *fp_tmp;
	//char fw_url[128],correct_md5[125],downloadfw_md5[125],fw_version[30],product_name[100];
	char current_version[30], buffer[20], buffer2[500], tmpBuf2[100];
	char_t	*action;
	int isNewAvailable = 0;
	//int isServerAlive = 0;
	action = websGetVar(wp, T("action"), T(""));
	printf("\naction=%s \n",action);
	//get data form firmware_info.txt

	if (!strcmp(action, T("fail"))){
		submitUrl = websGetVar(wp, T("failurl"), T(""));
		REPLACE_MSG(submitUrl);
		return;
	}

	if (!strcmp(action, T("killps"))){
	}

	if (!strcmp(action, T("check_version"))){
		isServerAlive = 0;
		REPLACE_MSG(submitUrl);
		return;
	}

	if (!strcmp(action, T("download_FW"))){ //download and check md5
		REPLACE_MSG(submitUrl);
		return;
	}
	
	if (!strcmp(action, T("checkFWfile"))){ //download and check md5
		int downloadOK = 0;
		downloadOK = 1;
		sprintf( buffer, "%s" ,"download_ok");
	}

	if (!strcmp(action, T("install_FW"))){ //install fw and reboot
		UPGRADE_PROCESS_MSG();
		return;
	}

	/* For jquery get data*/
	websWrite(wp, T("%s"), T("HTTP/1.0 200 OK\n"));
	websWrite(wp, T("%s"), T("Pragma: no-cache\n"));
	websWrite(wp, T("%s"), T("Cache-control: no-cache\n"));
	websWrite(wp, T("%s"), T("Content-Type: text/html\n"));
	websWrite(wp, T("%s"), T("\n"));
	websWrite(wp, T("%s"), buffer);
	websDone(wp, 200);	
	return;

}

