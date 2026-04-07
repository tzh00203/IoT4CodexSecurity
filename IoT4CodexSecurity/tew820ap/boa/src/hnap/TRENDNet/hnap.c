#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <ctype.h>

#include "boa.h"
#include "globals.h"
#include "apmib.h"
#include "apform.h"
#include "utility.h"
#include "mibtbl.h"
#include "asp_page.h"
#include "channel_list.h"
#include "hnap.h"
#include "hnap_xml.h"

#define BUF_LEN_1024 1024
#define BUF_LEN_256 256
#define BUF_LEN_128	128
#define BUF_LEN_16 16

#define DEF_RadioID_24G "RADIO_2.4GHz"
#define DEF_RadioID_24G_GUEST "RADIO_2.4G_Guest"
#define DEF_RadioID_5G "RADIO_5GHz"             // joe define for dual band 1665
#define DEF_RadioID_5G_GUEST "RADIO_5G_Guest"
#define BAND_802_11_B "802.11b"
#define BAND_802_11_G "802.11g"
#define BAND_802_11_N "802.11n"
#define BAND_802_11_BG "802.11bg"
#define BAND_802_11_GN "802.11gn"
#define BAND_802_11_BGN "802.11bgn"
#define BAND_802_11_A "802.11a"
#define BAND_802_11_AN "802.11an"
#define BAND_802_11_AC "802.11ac"
#define BAND_802_11_NAC "802.11nac"
#define BAND_802_11_ANAC "802.11anac"

#define STR_TRUE		"true"
#define STR_FALSE		"false"

#define RET_ERR 1
#define RET_OK	0
#define MAX_SITE_NUM	32
int hnap_wps_count = 0;
static SS_STATUS_Tp pStatus=NULL;
extern int wps_wizard_call;
extern char *fwVersion;	// defined in version.c
extern char *modelName;	// defined in version.c
extern char *hwVersion; // defined in version.c
extern char *fwdomain; // defined in version.c
extern void send_headers( request *wp, int status, char* title, char* extra_header, char* mime_type );
extern char *to_upper(char *str);
#define send_raw_data(content) 

//joe add for dual band wlan_idx settings.
void set_wlan_idx(char* strRadioID)
{
  	if (strcasecmp(strRadioID, DEF_RadioID_24G) == 0 || strcasecmp(strRadioID, "RADIO_24GHz") == 0 )
		{	
		  vwlan_idx = 0;
		  wlan_idx = 1;
		}
		else if (strcasecmp(strRadioID, DEF_RadioID_24G_GUEST) == 0)
		{
			vwlan_idx = 1;
			wlan_idx = 1;
		}
		else if (strcasecmp(strRadioID, DEF_RadioID_5G) == 0)
		{
			vwlan_idx = 0;
			wlan_idx = 0;
		}
		else if (strcasecmp(strRadioID, DEF_RadioID_5G_GUEST) == 0)
		{
			vwlan_idx = 1;
			wlan_idx = 1;
		}
		else
		{
		  printf("[%s][%d] ERROR band string\n",__FUNCTION__,__LINE__); // joe debug
		}
		
		printf("[%s][%d] wlan_idx = [%d], vwlan_idx = [%d]\n",__FUNCTION__,__LINE__,wlan_idx,vwlan_idx); // joe debug
}

//Tim Wang
//2012/06/14
void do_get_element_value(const char *xraw, char *value, char *tag)
{
        int size;
        char *ps = NULL,buf[128]={};
        
        if (tag == NULL )
          printf("...not found tag...\n");
        else if ((size = do_xml_tag_verify(xraw, tag, &ps)) >= 0) {
        	memcpy(buf, ps, size);
          printf("buf=[%s]\n",buf);      
          memcpy(value, buf, strlen(buf));
          value[size] = '\0';
        }
}

static char *find_next_element(char *source_xml, char *element_name){
        char *index1 = NULL;
        char *ch = NULL;

        index1 = strstr(source_xml, element_name);
        if (index1 != 0){
                while ((*(ch = index1 - 1) != '<') || (*(ch = index1 + strlen(element_name)) != '>'
                                                && *(ch = index1 + strlen(element_name)) != ' ' && *(ch = index1 + strlen(element_name)) != '/')){            
                        index1 += strlen(element_name);
                        index1 = strstr(index1, element_name);

                        if (index1 == NULL)
                                return index1;
        }

                index1 += strlen(element_name) + 1;
                source_xml = index1;
        }else{
                source_xml = NULL;
        }

        return source_xml;
}

int do_GetDeviceSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2*1024];
	char buffer[128];
	int intVal;
	char buf[128];
	unsigned char *strtmp;
	char latest_fwVerion[16]={0};
	FILE *fp;
	//printf("%s\n",__FUNCTION__);
	
	apmib_get( MIB_HOST_NAME, (void *)buffer);
	apmib_get( MIB_GRAPH_AUTH, (void *)&intVal);
	
	send_headers( req,  200, "Ok", (char*) 0, "text/xml" );	
	sprintf(xml_resraw,get_device_settings_result,
	  settings_result_form_head,
	  buffer,
	  modelName,
	  fwVersion);
	req_format_write(req, xml_resraw);
	
	memset(xml_resraw, 0, sizeof(xml_resraw));  
	sprintf(xml_resraw,get_device_settings_result2,
	  settings_result_form_foot);
	req_format_write(req, xml_resraw);
	
	return 0;
}

int do_SetDeviceSettings_to_xml(char *source_xml)
{
	int result=0;
	char strDeviceName[32];
	char strAdminPassword[32];
	char strBuffer[32];
	char ChangePassword[6];
	int  iCAPTCHA=0;
	int not_def = 0;
	
	printf("[%s]\n", __FUNCTION__);	
	
	memset(strDeviceName, 0, sizeof(strDeviceName));
	memset(strAdminPassword, 0, sizeof(strAdminPassword));
	memset(strBuffer, 0, sizeof(strBuffer));
	memset(ChangePassword, 0, sizeof(ChangePassword));
	
	do
	{
		do_get_element_value(source_xml, strDeviceName, "DeviceName");
		if( strlen(strDeviceName) != 0)
			apmib_set(MIB_HOST_NAME, (void *)strDeviceName);
	
		do_get_element_value(source_xml, strAdminPassword, "AdminPassword");
		apmib_set(MIB_USER_PASSWORD, (void *)strAdminPassword);

		apmib_update(CURRENT_SETTING);
		result = 2;
	}while(0);
		
	return result;	
}

int do_SetDeviceSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetDeviceSettings_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "REBOOT");

	sprintf(xml_resraw, set_device_settings_result, 
		settings_result_form_head,
		result,
		settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	if(xml_result == 2)
		system("sysconf init gw all"); //for Setup Wizard
}

int do_IsDeviceReady(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	//printf("%s\n",__FUNCTION__);

	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,is_device_ready_result);

	return 0;
}

int do_GetWLanRadioSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2*1024];
	char buffer1[128];
	char buffer2[128];
	char band_buffer[16];
	char result[32];
	int intVal=0;
	int intNoBroadcastSSID=0;
	int intChannelWidth=0;
	int intChannel=0;
	int iWLanEnable=0;
	struct sockaddr hwaddr;
	unsigned char *pMacAddr;
	char radio_tmp[16];
	
	//printf("%s\n",__FUNCTION__);
	strcpy(result, "OK");
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(buffer1, 0, sizeof(buffer1));
	memset(buffer2, 0, sizeof(buffer2));
	
	//need to set the 1665 to AP mode first
	{
		int OP_MODE = 0;
		int WLAN_MODE = 0;
		int REPEATER_MODE = 0;
		OP_MODE = BRIDGE_MODE;
		WLAN_MODE = AP_MODE;
		REPEATER_MODE = 0;
		apmib_set( MIB_WLAN_MODE, (void *)&WLAN_MODE);
		apmib_set( MIB_OP_MODE, (void *)&OP_MODE);
		apmib_set( MIB_REPEATER_ENABLED1, (void *)&REPEATER_MODE);
		apmib_set( MIB_REPEATER_ENABLED2, (void *)&REPEATER_MODE);
	}
	
	memset(radio_tmp, 0, sizeof(radio_tmp)); //RADIO_24GHz or RADIO_5G only
	do_get_element_value(raw, radio_tmp, "RadioID"); 
	if ((strcasecmp(radio_tmp, DEF_RadioID_24G) == 0) || (strcasecmp(radio_tmp, DEF_RadioID_5G) == 0) ||
		(strcasecmp(radio_tmp, "RADIO_24GHz") == 0))
	{	
		set_wlan_idx(radio_tmp);
			
		apmib_get( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);
		
		apmib_get( MIB_WLAN_BAND, (void *)&intVal);
		intVal--;
		if( intVal == 0 )
			strcpy(band_buffer, BAND_802_11_B);
		else if( intVal == 1 )
			strcpy(band_buffer, BAND_802_11_G);
		else if( intVal == 7 )
			strcpy(band_buffer, BAND_802_11_N);
		else if( intVal == 2 )
			strcpy(band_buffer, BAND_802_11_BG);
		else if( intVal == 9 )
			strcpy(band_buffer, BAND_802_11_GN);
		else if( intVal == 10 )
			strcpy(band_buffer, BAND_802_11_BGN);
		else if( intVal == 3 )
			strcpy(band_buffer, BAND_802_11_A);
		else if( intVal == 11 )
			strcpy(band_buffer, BAND_802_11_AN);
		else if( intVal == 63 )
			strcpy(band_buffer, BAND_802_11_AC);
		else if( intVal == 71 )
			strcpy(band_buffer, BAND_802_11_NAC);
		else if( intVal == 75 )
			strcpy(band_buffer, BAND_802_11_ANAC);
		else if(strcasecmp(radio_tmp, DEF_RadioID_24G) == 0)
			strcpy(band_buffer, BAND_802_11_BGN);
		else if(strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)
			strcpy(band_buffer, BAND_802_11_ANAC);
	
		if(strcasecmp(radio_tmp, DEF_RadioID_24G) == 0)
			strcpy(WLAN_IF, "wlan1");
		else if(strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)
			strcpy(WLAN_IF, "wlan0");
		getInAddr( WLAN_IF, HW_ADDR, (void *)&hwaddr );
		pMacAddr = hwaddr.sa_data;
		sprintf(buffer2,"%02x:%02x:%02x:%02x:%02x:%02x", 
			pMacAddr[0],pMacAddr[1],pMacAddr[2],
			pMacAddr[3],pMacAddr[4],pMacAddr[5]);
		to_upper(buffer2);
		apmib_get( MIB_WLAN_SSID, (void *)buffer1);
		
		apmib_get(MIB_WLAN_HIDDEN_SSID, (void *)&intNoBroadcastSSID);
		
		apmib_get(MIB_WLAN_CHANNEL_BONDING, (void *)&intChannelWidth);
		if(intChannelWidth == 2)
			intChannelWidth = 40;
		else if(intChannelWidth == 1)
			intChannelWidth = 20;
		else
			intChannelWidth =  0;
		
		apmib_get(MIB_WLAN_CHANNEL, (void *)&intChannel);
		//set the wlan_index to original
		vwlan_idx = 0;
	}
	else
	{
		strcpy(result, "ERROR");
	}
	
	sprintf(xml_resraw,get_wlan_radio_settings_result,
		settings_result_form_head,
		result,
		( iWLanEnable ? STR_FALSE :STR_TRUE ),
		band_buffer,
		buffer2,
		buffer1,
		( intNoBroadcastSSID ? STR_FALSE:STR_TRUE ), 
		intChannelWidth,
		intChannel,
		0,
		STR_FALSE,
		settings_result_form_foot);

	send_headers( req, 200, "Ok", (char*) 0, "text/xml" );
	req_format_write(req,xml_resraw);
	
	//printf("xml_resraw=\n%s\n\n",xml_resraw);
	return 0;
}

int do_SetWLanRadioSettings_to_xml(char *source_xml)
{
	int result = 0;
	char strRadioID[16];
	char strEnabled[16];
	char strBuffer[16];
	char strSSID[128];
	
	int iWLanEnable=0;
	int intMode;
	int intNoBroadcastSSID;
	int intChannelWidth;
	int not_def=0;
	
	int wlan_tmp = wlan_idx;
	printf("[%s],wlan_idx = %d\n", __FUNCTION__,wlan_idx);	
	
	memset(strRadioID, 0, sizeof(strRadioID));
	memset(strEnabled, 0, sizeof(strEnabled));
	memset(strBuffer, 0, sizeof(strBuffer));
	memset(strSSID, 0, sizeof(strSSID));
			
	do
	{
		do_get_element_value(source_xml, strRadioID, "RadioID");
		if((strcasecmp( strRadioID, DEF_RadioID_24G ) != 0) && (strcasecmp( strRadioID, DEF_RadioID_24G_GUEST ) != 0) && 
			(strcasecmp( strRadioID, DEF_RadioID_5G ) != 0) && (strcasecmp( strRadioID, DEF_RadioID_5G_GUEST ) != 0))
		{
			result = 2; // ERROR_BAD_RADIOID
			break;
		}		
		
		set_wlan_idx(strRadioID); // set wlan, vwlan idx
		
		do_get_element_value(source_xml, strEnabled, "Enabled");
		if( !strEnabled[0] )
			break;
		if(strcasecmp(strEnabled,STR_TRUE) == 0)
			iWLanEnable = 0;
		else if(strcasecmp(strEnabled,STR_FALSE) == 0) // joe modified. if set wlan_disable, break.
		{
			iWLanEnable = 1;
			apmib_set( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);
			apmib_update(CURRENT_SETTING);
			result = 0;
			printf("[%s][%d]\n",__FUNCTION__,__LINE__); // joe debug
			break;
		}
			
		do_get_element_value(source_xml, strBuffer, "Mode");
		if( !strBuffer[0] )
		{
			result = 3;
			break;
		}
		if(strcasecmp(strBuffer,BAND_802_11_B) == 0)
			intMode = 0;
		else if(strcasecmp(strBuffer,BAND_802_11_G) == 0)
			intMode = 1;
		else if(strcasecmp(strBuffer,BAND_802_11_N) == 0)
			intMode = 7;
		else if(strcasecmp(strBuffer,BAND_802_11_BG) == 0)
			intMode = 2;
		else if(strcasecmp(strBuffer,BAND_802_11_GN) == 0)
			intMode = 9;
		else if(strcasecmp(strBuffer,BAND_802_11_BGN) == 0)
			intMode = 10;
		else if(strcasecmp(strBuffer,BAND_802_11_A) == 0)
			intMode = 3;
		else if(strcasecmp(strBuffer,BAND_802_11_AN) == 0)
			intMode = 11;
		else if(strcasecmp(strBuffer,BAND_802_11_AC) == 0)
			intMode = 63;
		else if(strcasecmp(strBuffer,BAND_802_11_NAC) == 0)
			intMode = 71;
		else if(strcasecmp(strBuffer,BAND_802_11_ANAC) == 0)
			intMode = 75;
		else
		{
			result = 3;
			break;	
		}
		intMode++;	

		do_get_element_value(source_xml, strSSID, "SSID");
		if( !strSSID[0] )
			break;
		encode_hnap_special_char(strSSID);
			
		memset(strBuffer,0x0,sizeof(strBuffer));
		do_get_element_value(source_xml, strBuffer, "SSIDBroadcast");
		if( !strBuffer[0] )
			break;		
		if(strcasecmp(strBuffer,STR_TRUE) == 0)
			intNoBroadcastSSID = 0;
		else if(strcasecmp(strBuffer,BAND_802_11_G) == 0)
			intNoBroadcastSSID = 1;
		else			
			break;		
		
		memset(strBuffer,0x0,sizeof(strBuffer));		
		do_get_element_value(source_xml, strBuffer, "ChannelWidth");
		if( !strBuffer[0] )
			break;
		if(strcasecmp(strBuffer,"40") == 0)
			intChannelWidth = 1;
		else if(strcasecmp(strBuffer,"20") == 0)
			intChannelWidth = 0;
		else
			intChannelWidth = 2;
			
		memset(strBuffer,0x0,sizeof(strBuffer));		
		do_get_element_value(source_xml, strBuffer, "Channel");
		if( !strBuffer[0] )
		{
			result = 4;
			break;	
		}				
		memset(strBuffer,0x0,sizeof(strBuffer));		
		do_get_element_value(source_xml, strBuffer, "SecondaryChannel");
		if( !strBuffer[0] )
		{
			result = 5;
			break;	
		}		
//		memset(strBuffer,0x0,sizeof(strBuffer));		
//		do_get_element_value(source_xml, strBuffer, "QoS");			
//		if( !strBuffer[0] ){
//			break;
//		}
			
		apmib_set( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);		
		apmib_set( MIB_WLAN_SSID, (void *)strSSID);

//		if (strcasecmp( strRadioID, DEF_RadioID_24G ) == 0)
//		{
//			apmib_set( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);		
//			apmib_set( MIB_WLAN_SSID, (void *)strSSID);
//		}
//		else //guestzone
//		{
//			iWLanEnable != iWLanEnable;
//			apmib_set( MIB_GZ_SSID24G_ENABLE, (void *)&iWLanEnable);		
//			apmib_set( MIB_GZ_SSID24G, (void *)strSSID);			
//		}	
				
		apmib_set( MIB_WLAN_BAND, (void *)&intMode);
		apmib_set(MIB_WLAN_HIDDEN_SSID, (void *)&intNoBroadcastSSID);
		apmib_set(MIB_WLAN_CHANNEL_BONDING, (void *)&intChannelWidth);
		not_def = 1;
		apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);
		printf("[%s][%d]\n",__FUNCTION__,__LINE__); // joe debug
		
		result = 0;
		//result = 6;
		//set the wlan_index to original
		vwlan_idx = 0;
		wlan_idx = wlan_tmp;
	}while(0);
	
	return result;
}

int do_SetWLanRadioSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetWLanRadioSettings_to_xml(raw);
	
	apmib_update_web(CURRENT_SETTING);
	
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_RADIOID");
	else if(xml_result == 3)
		strcpy(result, "ERROR_BAD_MODE");
	else if(xml_result == 4)
		strcpy(result, "ERROR_BAD_CHANNEL");		
	else if(xml_result == 5)	
		strcpy(result, "ERROR_BAD_SECONDARY_CHANNEL");
	else if(xml_result == 6)	
		strcpy(result, "REBOOT");
	
	sprintf(xml_resraw, set_wlan_radio_settings_result, 
		settings_result_form_head,
		result,
		settings_result_form_foot);
	send_headers( req, 200, "Ok", (char*) 0, "text/xml" );	
	req_format_write(req, xml_resraw );
  	printf("\nxml_resraw = %s\n", xml_resraw);//joe debug
	if(xml_result == 6)	
		system("sysconf init gw all &");
}

int do_GetWLanRadioSecurity(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{	
	char result[32];
	char pEnabled[8];
	char pType[32];
	char pEncryption[16];	
	
	char xml_resraw[2*1024];
	char strAuthKey[128];
	char rsIP[32];
	char rs2IP[32];
	char rsPasswd[128];
	char rs2Passwd[128];
	char tmpBuffer[128];
	char radio_tmp[16];
	
	int intAuthType=0;
	ENCRYPT_T intENCRYPT= ENCRYPT_WPA2_MIXED;
	int rsPort=0;
	int rs2Port=0;
	
	WEP_T intWepKey;
	int intWepKeyType;
	int intWPAauth=0;
	int intWPAcipher=0;
	int iIntervl=0;
	
	printf("%s\n",__FUNCTION__);
	
	strcpy(result, "ERROR"); 
	memset(pEnabled, 0, sizeof(pEnabled)); 
	memset(pType, 0, sizeof(pType)); 
	memset(pEncryption, 0, sizeof(pEncryption)); 
	
	memset(xml_resraw, 0, sizeof(xml_resraw)); 
	memset(strAuthKey, 0, sizeof(strAuthKey)); 
	memset(rsIP, 0, sizeof(rsIP)); 
	memset(rs2IP, 0, sizeof(rs2IP)); 
	memset(rsPasswd, 0, sizeof(rsPasswd)); 
	memset(rs2Passwd, 0, sizeof(rs2Passwd)); 
	memset(tmpBuffer, 0, sizeof(tmpBuffer)); 	
	
	memset(radio_tmp, 0, sizeof(radio_tmp)); //RADIO_24GHz or RADIO_24G_Guest only
	
	
	do_get_element_value(raw, radio_tmp, "RadioID"); 
	printf("[%s][%d] radio_tmp = %s \n",__FILE__,__LINE__,radio_tmp); // joe debug
	//if (strcasecmp(radio_tmp, DEF_RadioID_24G) == 0)
	if((strcasecmp( radio_tmp, DEF_RadioID_24G ) == 0) || (strcasecmp( radio_tmp, DEF_RadioID_24G_GUEST ) == 0) ||
		(strcasecmp( radio_tmp, DEF_RadioID_5G ) == 0) || (strcasecmp( radio_tmp, DEF_RadioID_5G_GUEST ) == 0))
	{
    	set_wlan_idx(radio_tmp); // joe add for wlan and vwlan idx setting.
		
		apmib_get( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
		apmib_get( MIB_WLAN_AUTH_TYPE, (void *)&intAuthType);
		apmib_get( MIB_WLAN_WEP, (void *)&intWepKey);
		apmib_get( MIB_WLAN_WEP_KEY_TYPE, (void *)&intWepKeyType);		
		apmib_get( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
		
		if( intENCRYPT == ENCRYPT_DISABLED )
		{
			strcpy(pEnabled, STR_FALSE);
			strcpy(pType, "");
			strcpy(pEncryption, "");
		}
		else
			strcpy(pEnabled, STR_TRUE);

		if( intENCRYPT == ENCRYPT_WEP )
		{
			if( intAuthType == 1 )
				strcpy(pType, "WEP-SHARED");
			else
				strcpy(pType, "WEP-BOTH");
			
			if( intWepKey == WEP64)
			{
				strcpy(pEncryption, "WEP-64");
				apmib_get(MIB_WLAN_WEP64_KEY1,  (void *)tmpBuffer);
				if(intWepKeyType == 0)
					returnWepKey(0, 5, tmpBuffer, strAuthKey);
				else
					returnWepKey(1, 5, tmpBuffer, strAuthKey);		
			}
			else if( intWepKey == WEP128)
			{
				strcpy(pEncryption, "WEP-128");
				apmib_get(MIB_WLAN_WEP128_KEY1,  (void *)tmpBuffer);
				if(intWepKeyType == 0)
					returnWepKey(0, 13, tmpBuffer, strAuthKey);
				else
					returnWepKey(1, 13, tmpBuffer, strAuthKey);						
			}
		}
		else if( intENCRYPT == ENCRYPT_WPA )
		{
			apmib_get( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
			if( intWPAauth == WPA_AUTH_PSK )
			{
				strcpy(pType, "WEP-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strAuthKey);	
			}
			else
			{
				strcpy(pType, "WEP-RADIUS");
			}
			
			if( intWPAcipher == 1 )
				strcpy(pEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(pEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(pEncryption, "TKIPORAES");
		}
		else if( intENCRYPT == ENCRYPT_WPA2 )
		{
			apmib_get( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
			if( intWPAauth == WPA_AUTH_PSK )
			{
				strcpy(pType, "WPA2-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strAuthKey);	
			}				
			else
			{
				strcpy(pType, "WPA2-RADIUS");
			}
			
			if( intWPAcipher == 1 )
				strcpy(pEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(pEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(pEncryption, "TKIPORAES");			
		}
		else if( intENCRYPT == ENCRYPT_WPA2_MIXED )
		{
			apmib_get( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
			
			if( intWPAauth == WPA_AUTH_PSK )
			{
				strcpy(pType, "WPAORWPA2-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strAuthKey);
			}
			else
			{
				strcpy(pType, "WPAORWPA2-RADIUS");
			}
			
			if( intWPAcipher == 1 )
				strcpy(pEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(pEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(pEncryption, "TKIPORAES");		
		} 
		
		if( strcasecmp( pType, "WPA-RADIUS" ) == 0 ||
						 strcasecmp( pType, "WPA2-RADIUS" ) == 0 ||
						 strcasecmp( pType, "WPAORWPA2-RADIUS" ) == 0 )
		{

			apmib_get( MIB_WLAN_RS_IP,  (void *)tmpBuffer);
			if (!memcmp(tmpBuffer, "\x0\x0\x0\x0", 4))
					sprintf( rsIP,"0.0.0.0");
			else
					sprintf( rsIP, "%s", inet_ntoa(*((struct in_addr *)tmpBuffer)) );
			apmib_get( MIB_WLAN_RS_PORT, (void *)&rsPort);
			apmib_get( MIB_WLAN_RS_PASSWORD,  (void *)rsPasswd);
			apmib_get( MIB_WLAN_RS2_IP,  (void *)tmpBuffer);
			if (!memcmp(tmpBuffer, "\x0\x0\x0\x0", 4))
					sprintf( rs2IP,"0.0.0.0");
			else
					sprintf( rs2IP, "%s", inet_ntoa(*((struct in_addr *)tmpBuffer)) );
			apmib_get( MIB_WLAN_RS2_PORT, (void *)&rs2Port);
			apmib_get( MIB_WLAN_RS2_PASSWORD,  (void *)rs2Passwd);
			apmib_get( MIB_WLAN_RS_INTERVAL_TIME, (void *)&iIntervl);
		}
		printf("[%s][%d] DEBUG  \n",__FILE__,__LINE__); // joe debug
		strcpy(result, "OK");
		vwlan_idx = 0;
	}
	/*
	else if (strcasecmp(radio_tmp, DEF_RadioID_24G_GUEST) == 0)
	{
		strcpy(pEnabled, STR_TRUE);
		strcpy(pType, "WPAORWPA2-PSK");	
		strcpy(pEncryption, "TKIPORAES");	
		//apmib_get(MIB_GZ_WPA_PSK,  (void *)strAuthKey);		
		strcpy(result, "OK");
	}
	*/
	else
	{
		strcpy(result, "ERROR");
	}
	
	sprintf(xml_resraw,get_wlan_radio_security_result,
		settings_result_form_head,
		result,
		pEnabled,
		pType,
		pEncryption,
		strAuthKey,
		iIntervl,
		rsIP,
		rsPort,
		rsPasswd,
		rs2IP,
		rs2Port,
		rs2Passwd,
		settings_result_form_foot);	
	printf("\nxml_resraw = %s\n", xml_resraw);//joe debug
	send_headers( req, 200, "Ok", (char*) 0, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

int do_SetWLanRadioSecurity_to_xml(char *source_xml)
{
	char strRadioID[16];
	char strEnabled[16];
	char strEncryption[32];
	char strType[32];
	char strAuthKey[128];
	char strAuthKey_WEP[32];
	char rsPasswd[128];
	char rs2Passwd[128];
	char strBuffer[32];
	int not_def=0;
	int result = 0;

	printf("[%s]\n", __FUNCTION__);	

	ENCRYPT_T intENCRYPT;
	int intAuthType;
	WEP_T intWepKey;
	int intWPAcipher;
	int intWPAauth;
	int rsPort=0;
	int rs2Port=0;

	struct in_addr rsIP,rs2IP;

	memset(strRadioID, 0, sizeof(strRadioID));
	memset(strEnabled, 0, sizeof(strEnabled));
	memset(strEncryption, 0, sizeof(strEncryption));
	memset(strType, 0, sizeof(strType));
	memset(strAuthKey, 0, sizeof(strAuthKey));
	memset(strAuthKey_WEP, 0, sizeof(strAuthKey_WEP));
	memset(rsPasswd, 0, sizeof(rsPasswd));
	memset(rs2Passwd, 0, sizeof(rs2Passwd));
	memset(strBuffer, 0, sizeof(strBuffer));


	do
	{
		do_get_element_value(source_xml, strRadioID, "RadioID");
		if((strcasecmp( strRadioID, DEF_RadioID_24G ) != 0) && (strcasecmp( strRadioID, DEF_RadioID_24G_GUEST ) != 0) && 
			(strcasecmp( strRadioID, DEF_RadioID_5G ) != 0) && (strcasecmp( strRadioID, DEF_RadioID_5G_GUEST ) != 0))
		{
			result = 2;
			break;
		}
		
		if ( (strcasecmp( strRadioID, DEF_RadioID_24G ) == 0)  ||  (strcasecmp( strRadioID, DEF_RadioID_5G ) == 0) )
		{
			do_get_element_value(source_xml, strEnabled, "Enabled");
			if( !strEnabled[0] )
				break;
			if( strcasecmp( strEnabled, STR_FALSE ) == 0 )
			{
				intENCRYPT = ENCRYPT_DISABLED;
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_update(CURRENT_SETTING);
				result = 0;
				break;
			}
			else
			{
        		set_wlan_idx(strRadioID);
			}
			
			do_get_element_value(source_xml, strType, "Type");
			do_get_element_value(source_xml, strEncryption, "Encryption");
			do_get_element_value(source_xml, strAuthKey, "Key");
			
			if( strncasecmp( strType, "NONE" , 4 ) == 0 )
			{
				intENCRYPT = 0;
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
			}
			else if( strncasecmp( strType, "WEP" , 3 ) == 0 ) // modified. strcasecmp can not find "WEP", because there is no input of "WEP". In this case, strcasecmp will return 1 or -1, never 0.
			{
				intENCRYPT = ENCRYPT_WEP;
	
				if( strcasecmp( strType, "WEP-SHARED" ) == 0 )
					intAuthType = 1;
				else if( strcasecmp( strType, "WEP-OPEN" ) == 0 )
					intAuthType = 2;
				else if( strcasecmp( strType, "WEP-BOTH" ) == 0 )
					intAuthType = 2;
					
				if( strcasecmp( strEncryption, "WEP-64" ) == 0 )
				{
					if( !strAuthKey[0] || strlen(strAuthKey) > 10 )
					{
						result = 3;
						break;
					}
					intWepKey = WEP64;
					string_to_hex(strAuthKey, strAuthKey_WEP, strlen(strAuthKey));
					apmib_set(MIB_WLAN_WEP64_KEY1, (void *)strAuthKey_WEP);
				}
				else if( strcasecmp( strEncryption, "WEP-128" ) == 0 )
				{
					if( !strAuthKey[0] || strlen(strAuthKey) > 26 )
					{
						result = 3;
						break;
					}
					intWepKey = WEP128;
					string_to_hex(strAuthKey, strAuthKey_WEP, strlen(strAuthKey));
					apmib_set(MIB_WLAN_WEP128_KEY1, (void *)strAuthKey_WEP);
				}
				else
				{
					result = 4;
					break;
				}
					
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_set( MIB_WLAN_AUTH_TYPE, (void *)&intAuthType);
				apmib_set( MIB_WLAN_WEP, (void *)&intWepKey);			
			}
			else if( strcasecmp( strType, "WPA-PSK" ) == 0 ||
							 strcasecmp( strType, "WPA2-PSK" ) == 0 ||
							 strcasecmp( strType, "WPAORWPA2-PSK" ) == 0 )
			{
				if( strcasecmp( strType, "WPA-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA;
				else if( strcasecmp( strType, "WPA2-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2;
				else if( strcasecmp( strType, "WPAORWPA2-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2_MIXED;
				else
				{
					result = 4;
					break;
				}
				
				intWPAauth = WPA_AUTH_PSK;	
				if( !strAuthKey[0] || strlen(strAuthKey) > 63 )
				{
					result = 3;
					break;
				}
				
				if( strcasecmp( strEncryption, "TKIP" ) == 0 )
					intWPAcipher = 1;
				else if( strcasecmp( strEncryption, "AES" ) == 0 )
					intWPAcipher = 2;
				else if( strcasecmp( strEncryption, "TKIPORAES" ) == 0 )
					intWPAcipher = 3;
				else
				{
					result = 4;
					break;
				}
	
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_set( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
				//apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				//if(intENCRYPT == ENCRYPT_WPA)
					apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				//else
					apmib_set( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
				apmib_set( MIB_WLAN_WPA_PSK,  (void *)strAuthKey);
				apmib_set( MIB_WLAN_WSC_PSK,  (void *)strAuthKey);
			}
			else if( strcasecmp( strType, "WPA-RADIUS" ) == 0 ||
							 strcasecmp( strType, "WPA2-RADIUS" ) == 0 ||
							 strcasecmp( strType, "WPAORWPA2-RADIUS" ) == 0 )
			{
				if( strcasecmp( strType, "WPA-RADIUS" ) == 0 )
					intENCRYPT = ENCRYPT_WPA;
				else if( strcasecmp( strType, "WPA2-RADIUS" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2;
				else if( strcasecmp( strType, "WPAORWPA2-RADIUS" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2_MIXED;
				else
				{
					result = 4;
					break;
				}
				
				intWPAauth = WPA_AUTH_AUTO;
					
				if( strcasecmp( strEncryption, "TKIP" ) == 0 )
					intWPAcipher = 1;
				else if( strcasecmp( strEncryption, "AES" ) == 0 )
					intWPAcipher = 2;
				else if( strcasecmp( strEncryption, "TKIPORAES" ) == 0 )
					intWPAcipher = 3;
				else
				{
					result = 4;
					break;
				}
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_set( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
				//apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				//if(intENCRYPT == ENCRYPT_WPA)
					apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				//else
					apmib_set( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
				
				do_get_element_value(source_xml, strBuffer, "RadiusIP1");
				if( !strBuffer[0] )
					sprintf(strBuffer,"0.0.0.0");
				inet_aton(strBuffer, &rsIP);
				memset( strBuffer, 0x0, sizeof(strBuffer));
				do_get_element_value(source_xml, strBuffer, "RadiusPort1");
				if( strBuffer[0] )
					rsPort = atoi(strBuffer);
				do_get_element_value(source_xml, rsPasswd, "RadiusSecret1");
				
				do_get_element_value(source_xml, strBuffer, "RadiusIP2");
				if( !strBuffer[0] )
					sprintf(strBuffer,"0.0.0.0");
				inet_aton(strBuffer, &rs2IP);
				memset( strBuffer, 0x0, sizeof(strBuffer));
				do_get_element_value(source_xml, strBuffer, "RadiusPort2");
				if( strBuffer[0] )
					rs2Port = atoi(strBuffer);
				do_get_element_value(source_xml, rs2Passwd, "RadiusSecret2");
				
				apmib_set( MIB_WLAN_RS_IP,  (void *)&rsIP);
				apmib_set( MIB_WLAN_RS_PORT, (void *)&rsPort);
				apmib_set( MIB_WLAN_RS_PASSWORD,  (void *)rsPasswd);
	
				apmib_set( MIB_WLAN_RS2_IP,  (void *)&rs2IP);
				apmib_set( MIB_WLAN_RS2_PORT, (void *)&rs2Port);
				apmib_set( MIB_WLAN_RS2_PASSWORD,  (void *)rs2Passwd);
			}
			else
			{
				result = 5;
				break;
			}			
		}
		else if ((strcasecmp( strRadioID, DEF_RadioID_24G_GUEST ) == 0) || (strcasecmp( strRadioID, DEF_RadioID_5G_GUEST ) == 0))			
		{
			do_get_element_value(source_xml, strEnabled, "Enabled");
			if( !strEnabled[0] )
				break;
			if( strcasecmp( strEnabled, STR_FALSE ) == 0 )
			{
				result = 5;
				break;
			}
			else
			{
				vwlan_idx = 1;
			}
			
			do_get_element_value(source_xml, strType, "Type");
			do_get_element_value(source_xml, strEncryption, "Encryption");
			do_get_element_value(source_xml, strAuthKey, "Key");
			
			if( strcasecmp( strType, "WPAORWPA2-PSK" ) == 0 )
			{
				if( !strAuthKey[0] || strlen(strAuthKey) > 63 )
				{
					result = 3;
					break;
				}
				
				if( strcasecmp( strEncryption, "TKIPORAES" ) == 0 )
					intWPAcipher = 3;
				else
				{
					result = 4;
					break;
				}
				//apmib_set( MIB_GZ_WPA_PSK,  (void *)strAuthKey);	
				apmib_set( MIB_WLAN_WPA_PSK,  (void *)strAuthKey);
				
				intWPAauth = WPA_AUTH_PSK;
				apmib_set( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
				apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				apmib_set( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
			}
			else
			{
				result = 5;
				break;
			}	
			vwlan_idx = 0;			
		}
		not_def = 1;
		apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);		
		
	}while(0);
	
	return result;
}

int do_SetWLanRadioSecurity(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetWLanRadioSecurity_to_xml(raw);
	
	apmib_update(CURRENT_SETTING);

	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_RADIOID");
	else if(xml_result == 3)
		strcpy(result, "ERROR_ILLEGAL_KEY_VALUE");
	else if(xml_result == 4)
		strcpy(result, "ERROR_ENCRYPTION_NOT_SUPPORTED");		
	else if(xml_result == 5)	
		strcpy(result, "ERROR_TYPE_NOT_SUPPORTED");
	else if(xml_result == 6)	
		strcpy(result, "REBOOT");
	
	sprintf(xml_resraw, set_wLan_radio_security_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req, 200, "Ok", (char*) 0, "text/xml" );	
	req_format_write(req, xml_resraw );
	
	printf("[%s]xml_result = %d, xml_resraw=\n%s\n", __FUNCTION__,xml_result,xml_resraw);
}

int do_GetWLanRadios(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[20480];
	char result[32];
	int wlan_idx_tmp = wlan_idx;
	int RegDomain;
	char Reg_tmpStr[8]={0x0};
	char channel_tmp_2g[128]={0x0};
	char *channel_tmp_2g_tok;
	char ChannelList_2g[300] = {"<int>0</int>"};
	char channel_tmp_5g[128]={0x0};
	char *channel_tmp_5g_tok;
	char ChannelList_5g[300] = {"<int>0</int>"};
	char RadioInfo[10240] = {};
	char RadioInfo2g[7168] = {};
	char RadioInfo5g[7168] = {};
	char RadioInfo2g_tmp[4096] = {};
	char RadioInfo5g_tmp[4096] = {};
	char WideChannels_2GHz_tmp[4096] = {};
	
	strcpy(result, "OK");
	memset(xml_resraw, 0, sizeof(xml_resraw));			
	sprintf(xml_resraw, get_wlan_radios_result_head,
	 settings_result_form_head,
	 result);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	// get 2.4GHz info
	wlan_idx = 1;
    apmib_get( MIB_HW_REG_DOMAIN ,(void *)&RegDomain);	 
    sprintf(Reg_tmpStr,"%d",RegDomain);
    channel_list("",Reg_tmpStr,&channel_tmp_2g,"2.4G");//get 2.4GHz channel list 
   	
	channel_tmp_2g_tok=strtok(channel_tmp_2g, ",");
	sprintf(ChannelList_2g, "%s<int>%s</int>", ChannelList_2g, channel_tmp_2g_tok);
	while((channel_tmp_2g_tok=strtok(NULL, ","))) {
		sprintf(ChannelList_2g, "%s<int>%s</int>", ChannelList_2g, channel_tmp_2g_tok);
	}
	if ( RegDomain == 3 )
		sprintf(WideChannels_2GHz_tmp, WideChannels_2GHz, "<WideChannel><Channel>11</Channel><SecondaryChannels><int>9</int><int>13</int></SecondaryChannels></WideChannel>");
	else
		sprintf(WideChannels_2GHz_tmp, WideChannels_2GHz, "");

	req_format_write(req,"<RadioInfo>");
	sprintf(RadioInfo2g_tmp, RadioInfo_2GHz, ChannelList_2g);
	req_format_write(req,RadioInfo2g_tmp);
	req_format_write(req,WideChannels_2GHz_tmp);
	req_format_write(req,SecurityInfo);
	req_format_write(req,"</RadioInfo>");
	
	//get 5GHz info
	wlan_idx = 0;
	memset(Reg_tmpStr, 0, sizeof(Reg_tmpStr));
	apmib_get( MIB_HW_REG_DOMAIN ,(void *)&RegDomain);	 
    sprintf(Reg_tmpStr,"%d",RegDomain);
	channel_list(Reg_tmpStr,"",&channel_tmp_5g,"5G_disable"); //5G_disable = DFS enable, 5G_enable = DFS disable
   
	channel_tmp_5g_tok=strtok(channel_tmp_5g, ",");
	sprintf(ChannelList_5g, "%s<int>%s</int>", ChannelList_5g, channel_tmp_5g_tok);
	while((channel_tmp_5g_tok=strtok(NULL, ","))) {
		sprintf(ChannelList_5g, "%s<int>%s</int>", ChannelList_5g, channel_tmp_5g_tok);
	} 
	
	req_format_write(req,"<RadioInfo>");
	sprintf(RadioInfo5g_tmp, RadioInfo_5GHz, ChannelList_5g);
	req_format_write(req,RadioInfo5g_tmp);
	req_format_write(req,WideChannels_5GHz);
	req_format_write(req,SecurityInfo);
	req_format_write(req,"</RadioInfo>");
	//get 5GHz info end
	
	req_format_write(req,get_wlan_radios_result_foot);	
	wlan_idx = wlan_idx_tmp;
}

int do_SetTriggerWirelessSiteSurvey(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char radio_tmp[16];
	int WaitTime = 15;
	
	strcpy(result, "OK");

	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(radio_tmp, 0, sizeof(radio_tmp));
	
	hnap_sitesurvey_flag = 0;
	do_get_element_value(raw, radio_tmp, "RadioID");

	sprintf(xml_resraw, set_trigger_wireless_site_survey_result,
	 settings_result_form_head,
	 result,
	 WaitTime,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	if ((strcasecmp(radio_tmp, "RADIO_24GHz") == 0) || (strcasecmp(radio_tmp, DEF_RadioID_24G) == 0) || (strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)){
		
		hnap_sitesurvey_flag = 1;
//		if(strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)
//	 		strcpy(WLAN_IF, "wlan0");
//	 	else
//	 		strcpy(WLAN_IF, "wlan1");
	}
	else
		strcpy(result, "ERROR_BAD_BandID");

}

int do_GetSiteSurvey(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char xml_resraw2[2048];
	char result[32];
	char radio_tmp[13];
	char APStatInfo[4096];
	char APStatInfo_tmp[2048];
	char SecurityType[20];
	char Encryptions[40];
	char ssidbuf[32*5+1];
	char ssidbuf_tmp[32*5+1];
	char tmpBuf[256];
	unsigned char res;
	int i=0;
	int wait_time;
	int status;
	int wpa_exist = 0;
	BssDscr *pBss;
	bss_info bss;
	
	strcpy(result, "OK");
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(radio_tmp, 0, sizeof(radio_tmp));
	memset(APStatInfo, 0, sizeof(APStatInfo));
	memset(APStatInfo_tmp, 0, sizeof(APStatInfo_tmp));
	memset(SecurityType, 0, sizeof(SecurityType));
	memset(Encryptions, 0, sizeof(Encryptions));
	memset(ssidbuf, 0, sizeof(ssidbuf));
	memset(ssidbuf_tmp, 0, sizeof(ssidbuf_tmp));
	memset(tmpBuf, 0, sizeof(tmpBuf));
	
	
	sprintf(xml_resraw, get_site_survey_result_head,
	 settings_result_form_head,
	 result);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	do_get_element_value(raw, radio_tmp, "RadioID");
	if ((strcasecmp(radio_tmp, "RADIO_24GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_2.4GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_5GHz") == 0)){	
		pStatus=NULL;
		//pStatus->number = 0;
		
		if(strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)
	 		strcpy(WLAN_IF, "wlan0");
	 	else
	 		strcpy(WLAN_IF, "wlan1"); 

		if (pStatus==NULL) {
			pStatus = calloc(1, sizeof(SS_STATUS_T));
			if ( pStatus == NULL ) {
				printf("Allocate buffer failed!\n");
				strcpy(result, "ERROR");
			}
		}
		
		if ( getWlSiteSurveyResult(WLAN_IF, pStatus) < 0 ) {
			printf("Read site-survey status failed!");
			strcpy(result, "ERROR");
			free(pStatus);
			pStatus = NULL;
		}
		
		if ( getWlBssInfo(WLAN_IF, &bss) < 0) {
			printf("Get bssinfo failed!");
			strcpy(result, "ERROR");
		}
		
		for(i = 0; i < pStatus->number && pStatus->number != 0xff; i++){
			pBss = &pStatus->bssdb[i];
			sprintf(tmpBuf,"%02X:%02X:%02X:%02X:%02X:%02X", pBss->bdBssId[0], pBss->bdBssId[1], pBss->bdBssId[2], 
			 pBss->bdBssId[3],pBss->bdBssId[4],pBss->bdBssId[5]);
			//memcpy(ssidbuf_tmp, pBss->bdSsIdBuf, pBss->bdSsId.Length);
			memset(ssidbuf, 0, sizeof(ssidbuf));
			memset(ssidbuf_tmp, 0, sizeof(ssidbuf_tmp));
			strncpy(ssidbuf_tmp, pBss->bdSsIdBuf, pBss->bdSsId.Length);
			encode_hnap_special_char(ssidbuf_tmp);
			strcpy(ssidbuf, ssidbuf_tmp);
			//ssidbuf[pBss->bdSsId.Length] = '\0';
			
			if((pBss->bdCap & cPrivacy) == 0){
				strcpy(SecurityType, "None");
				strcpy(Encryptions, "");
			}
			else{
				if(pBss->bdTstamp[0] == 0){
					strcpy(SecurityType, "WEP");
					strcpy(Encryptions, "WEP-64</string><string>WEP-128");
				}
				else{
					if (pBss->bdTstamp[0] & 0x0000ffff) {
						wpa_exist = 1;
						strcpy(SecurityType, "WPA");
						if (((pBss->bdTstamp[0] & 0x0000f000) >> 12) == 0x4)
							strcat(SecurityType, "-PSK");
						else if(((pBss->bdTstamp[0] & 0x0000f000) >> 12) == 0x2){
							strcat(SecurityType, "-1X");
						}
	
						if (((pBss->bdTstamp[0] & 0x00000f00) >> 8) == 0x5){
							strcpy(Encryptions, "TKIPORAES");
						}
						else if (((pBss->bdTstamp[0] & 0x00000f00) >> 8) == 0x4)
							strcpy(Encryptions, "AES");
						else if (((pBss->bdTstamp[0] & 0x00000f00) >> 8) == 0x1){
							strcpy(Encryptions, "TKIP");
						}
					}
					if(pBss->bdTstamp[0] & 0xffff0000) {
						if (wpa_exist)
							strcat(SecurityType, "/WPA2");
						else
							strcpy(SecurityType, "WPA2");
						if (((pBss->bdTstamp[0] & 0xf0000000) >> 28) == 0x4)
							strcat(SecurityType, "-PSK");
						else if (((pBss->bdTstamp[0] & 0xf0000000) >> 28) == 0x2){
							strcat(SecurityType, "-1X");
						}
	
						if (((pBss->bdTstamp[0] & 0x0f000000) >> 24) == 0x5){
							strcpy(Encryptions, "TKIPORAES");
						}
						else if (((pBss->bdTstamp[0] & 0x0f000000) >> 24) == 0x4)
							strcpy(Encryptions, "AES");
						else if (((pBss->bdTstamp[0] & 0x0f000000) >> 24) == 0x1){
							strcpy(Encryptions, "TKIP");
						}
					}
				}
			}

			pBss->rssi*=1.56;
			if(pBss->rssi > 100) 
				pBss->rssi = 100;

			
			sprintf(APStatInfo_tmp, ap_stat_info_str, ssidbuf, tmpBuf, SecurityType, Encryptions, pBss->ChannelNumber, pBss->rssi);
			strcat(APStatInfo, APStatInfo_tmp);
		
			if(i % 2 == 0){
				req_format_write(req, APStatInfo);
				memset(APStatInfo, 0, sizeof(APStatInfo));
				memset(APStatInfo_tmp, 0, sizeof(APStatInfo_tmp));
			}
		}
	}
	else
		strcpy(result, "ERROR_BAD_BandID");

	sprintf(xml_resraw, get_site_survey_result_foot, settings_result_form_foot);
	req_format_write(req, xml_resraw);
	return 0;
}

int do_GetWanSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	unsigned char buffer[512];
	unsigned char buffer1[16];
	unsigned char tmp_mac[6];
	char Type[8];
	char PrimaryDNS[32];
	char SecondaryDNS[32];
	char IPAddress[32];
	char SubnetMask[32];
	char Gateway[32];
	char wan_mac[32];
	int dhcp;
	int dhcp_mtu;
	int IPAddress_flag;
	struct in_addr intaddr;
	FILE *fp;
	
	printf("%s\n",__FUNCTION__);
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(Type, 0, sizeof(Type));
	memset(PrimaryDNS, 0, sizeof(PrimaryDNS));
	memset(SecondaryDNS, 0, sizeof(SecondaryDNS));
	memset(IPAddress, 0, sizeof(IPAddress));
	memset(SubnetMask, 0, sizeof(SubnetMask));
	memset(Gateway, 0, sizeof(Gateway));
	memset(wan_mac, 0, sizeof(wan_mac));
	memset(tmp_mac, 0, sizeof(tmp_mac));
	
	strcpy(result, "OK");

	apmib_get( MIB_DHCP ,(void *)&dhcp);
	if(dhcp == 1){
		strcpy(Type, "DHCP");
		strcpy(PrimaryDNS, "");
		strcpy(SecondaryDNS, "");
	}
	else{
		strcpy(Type, "Static");
		apmib_get( MIB_DNS1, (void *)buffer);
		if (!memcmp(buffer, "\x0\x0\x0\x0", 4))
			strcpy(PrimaryDNS, "0.0.0.0");
		else 
			strcpy(PrimaryDNS, inet_ntoa(*((struct in_addr *)buffer)));
		apmib_get( MIB_DNS2, (void *)buffer);
		if (!memcmp(buffer, "\x0\x0\x0\x0", 4))
			strcpy(SecondaryDNS, "0.0.0.0");
		else
			strcpy(SecondaryDNS, inet_ntoa(*((struct in_addr *)buffer)));
	}
		
	//ip
	if ((fp = fopen("/tmp/got_dhcpcip", "r")) == NULL){
		if(dhcp==1)
			strcpy(IPAddress, "0.0.0.0");
	}	
	else{	
		fgets(buffer1,sizeof(buffer1),fp);
		fclose(fp);
		buffer1[strlen(buffer1)-1]='\0';
	}
	if ((fp = fopen("/tmp/zeroip", "r"))==NULL)
		strcpy(IPAddress, "0.0.0.0");
	else{
		fgets(buffer,sizeof(buffer),fp);
		fclose(fp);
		buffer[strlen(buffer)-1]='\0';
	}	
	if(dhcp==1){
		if(!strcmp(buffer1,"1")){ //got ip
			if (getInAddr(BRIDGE_IF, IP_ADDR, (void *)&intaddr))
				strcpy(IPAddress, inet_ntoa(intaddr));
		}
		else{//not yet
			if(buffer[0])
				strcpy(IPAddress, buffer);
			else
				strcpy(IPAddress, "0.0.0.0");
		}
	}
	else{
		if (getInAddr(BRIDGE_IF, IP_ADDR, (void *)&intaddr))
			strcpy(IPAddress, inet_ntoa(intaddr));
		else
			strcpy(IPAddress, "0.0.0.0");
	}
	
	//mask
	if (getInAddr(BRIDGE_IF, SUBNET_MASK, (void *)&intaddr))
		strcpy(SubnetMask, inet_ntoa(intaddr));
	else
		strcpy(SubnetMask, "0.0.0.0");
	
	//gateway
	if(dhcp != 1){
		if(apmib_get( MIB_DEFAULT_GATEWAY,  (void *)buffer)){
			if(memcmp(buffer, "\x0\x0\x0\x0", 4))
				strcpy(Gateway, inet_ntoa(*((struct in_addr *)buffer)));
		}
	}
	if(dhcp == 2){
		apmib_get(MIB_DEFAULT_GATEWAY,  (void *)buffer);
		if (!memcmp(buffer, "\x0\x0\x0\x0", 4))
			strcpy(Gateway,  "0.0.0.0");
   		else
			strcpy(Gateway, inet_ntoa(*((struct in_addr *)buffer)));
	}
	if (getDefaultRoute(BRIDGE_IF, &intaddr) )
		strcpy(Gateway, inet_ntoa(intaddr));
	else
		strcpy(Gateway,  "0.0.0.0");
	
	apmib_get( MIB_HW_NIC1_ADDR, (void *)tmp_mac);
	sprintf(wan_mac,"%02X:%02X:%02X:%02X:%02X:%02X", tmp_mac[0],tmp_mac[1],tmp_mac[2],tmp_mac[3],tmp_mac[4],tmp_mac[5]);
	apmib_get(MIB_DHCP_MTU_SIZE ,(void *)&dhcp_mtu);
	
	sprintf(xml_resraw, get_wan_settings_result,
	 settings_result_form_head,
	 result,
	 Type,
	 IPAddress,
	 SubnetMask,
	 Gateway,
	 PrimaryDNS,
	 SecondaryDNS,
	 wan_mac,
	 dhcp_mtu,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	printf("%s\n",xml_resraw);
	return 0;
}

int do_SetWanSettings_to_xml(char *source_xml)
{
	int result=0;
	struct in_addr inIp, inMask, inGateway, dns1, dns2;
	char strType[8];
	char strIPAddress[32];
	char strSubnetMask[32];
	char strGateway[32];
	char strPrimaryDNS[32];
	char strSecondaryDNS[32];
	unsigned char strMacAddress[32];
	unsigned char tmpMacAddress[32];
	unsigned char wan_mac[32];
	int dhcp;
	int i, j;
	
	memset(strType, 0, sizeof(strType));
	memset(strIPAddress, 0, sizeof(strIPAddress));
	memset(strSubnetMask, 0, sizeof(strSubnetMask));
	memset(strGateway, 0, sizeof(strGateway));
	memset(strPrimaryDNS, 0, sizeof(strPrimaryDNS));
	memset(strSecondaryDNS, 0, sizeof(strSecondaryDNS));
	memset(strMacAddress, 0, sizeof(strMacAddress));
	memset(tmpMacAddress, 0, sizeof(tmpMacAddress));
	memset(wan_mac, 0, sizeof(wan_mac));
	
	do_get_element_value(source_xml, strType, "Type");
	if(!strcmp(strType, "DHCP"))
		dhcp = 1;
	else if(!strcmp(strType, "Static"))
		dhcp = 0;
	else{ 
		dhcp = 1;
		result = 3;
	}
	apmib_set(MIB_DHCP, (void *)&dhcp);
	
	if(dhcp == 0){
		do_get_element_value(source_xml, strIPAddress, "IPAddress");
		if (!inet_aton(strIPAddress, &inIp))
			result = 1;
		if (!apmib_set(MIB_IP_ADDR, (void *)&inIp))
			result = 1;
		do_get_element_value(source_xml, strSubnetMask, "SubnetMask");
		if (!inet_aton(strSubnetMask, &inMask))
			result = 1;
		if (!apmib_set(MIB_SUBNET_MASK, (void *)&inMask))
			result = 1;
		do_get_element_value(source_xml, strGateway, "Gateway");
		if (!inet_aton(strGateway, &inGateway))
			result = 1;
		if (!apmib_set(MIB_DEFAULT_GATEWAY, (void *)&inGateway))
			result = 1;
		do_get_element_value(source_xml, strPrimaryDNS, "Primary");
		if (!inet_aton(strPrimaryDNS, &dns1))
			result = 1;
		if (!apmib_set(MIB_DNS1, (void *)&dns1))
			result = 1;
		do_get_element_value(source_xml, strSecondaryDNS, "Secondary");
		if (!inet_aton(strSecondaryDNS, &dns2))
			result = 1;
		if (!apmib_set(MIB_DNS2, (void *)&dns2))
			result = 1;
	}
	
	do_get_element_value(source_xml, strMacAddress, "MacAddress");
	if (!strncmp(strMacAddress,"00:00:00:00:00:00", 17))
		result = 1;
	if (strcmp(strMacAddress,"")){
		for (i=0,j=0;i<17;i++){//remove ':' out of MacAddress
			if ( i==2 || i==5 || i==8 || i==11 || i==14 )
				continue;
			tmpMacAddress[j] = strMacAddress[i];
			j++;
		}
		string_to_hex(tmpMacAddress, wan_mac, 12);
		apmib_set(MIB_HW_NIC1_ADDR, (void *)wan_mac);
	}
	
	apmib_update(CURRENT_SETTING);
	
	return result;	
}

int do_SetWanSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	printf("%s\n",__FUNCTION__);
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetWanSettings_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "REBOOT");
	else if(xml_result == 3)
		strcpy(result, "ERROR_BAD_WANTYPE");
	else if(xml_result == 4)
		strcpy(result, "ERROR_AUTO_MTU_NOT_SUPPORTED");

	
	sprintf(xml_resraw, set_wan_settings_result, 
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}
//It only supported wlan0 currently.
int do_SetTriggerWPS(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char strTrigger[2];
	char strCliPin[16];
	char tmpbuf[256];
	char devicePin[16];
	char pincodestr_b[20];	
	int intVal, intMode;
	int idx, idx2;
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(strTrigger, 0, sizeof(strTrigger));	
	memset(tmpbuf, 0, sizeof(tmpbuf));	
	memset(devicePin, 0, sizeof(devicePin));
		
	strcpy(result, "OK");
	wlan_idx = 0;
	
	do_get_element_value(raw, strTrigger, "triggerWPS");
	do_get_element_value(raw, strCliPin, "clientPIN");
	
	if(!strcmp(strTrigger, "1")){
		apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
		if (intVal) {
			intVal = 0;
			apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
            updateVapWscDisable(wlan_idx, intVal);
			apmib_update_web(CURRENT_SETTING);	// update to flash	
			system("echo 1 > /var/wps_start_pbc");
			run_init_script("bridge");			
		}
		else {		
	       	sprintf(tmpbuf, "%s -sig_pbc wlan0", _WSC_DAEMON_PROG);
			system(tmpbuf);
		}
	}
	else if(!strcmp(strTrigger, "2")){	
		apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
		if (intVal) {
			intVal = 0;
			apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
			apmib_update_web(CURRENT_SETTING);	
			sprintf(tmpbuf, "echo %s > /var/wps_peer_pin", strCliPin);
			system(tmpbuf);
			run_init_script("bridge");		
		}
		else {			
			memset(pincodestr_b,'\0',20);				
			idx2=0;
			for(idx=0 ; idx <strlen(strCliPin) ; idx++){
				if(strCliPin[idx] >= '0' && strCliPin[idx]<= '9'){
					pincodestr_b[idx2]=strCliPin[idx];	
					idx2++;
				}
			}
			
			sprintf(tmpbuf, "iwpriv wlan0 set_mib pin=%s", pincodestr_b);			
			system(tmpbuf);
		}
	}
	else if(!strcmp(strTrigger, "3")){	
//		wlan_idx = 0;
//		intVal = 0;
//		intMode = CLIENT_MODE;
//		ENCRYPT_T encrypt;
//		encrypt = ENCRYPT_WPA2;
//		
//		apmib_set(MIB_WLAN_MODE, (void *)&intMode);
//		apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
//		apmib_set(MIB_WLAN_WLAN_DISABLED, (void *)&intVal);
//		if ( apmib_set(MIB_WLAN_ENCRYPT, (void *)&encrypt) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA2_UNICIPHER failed!"));
//		}
//		
//		intVal = WPA_CIPHER_AES ;
//		
//		if ( apmib_set(MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intVal) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA2_UNICIPHER failed!"));
//		}
//		
//		if ( apmib_set(MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intVal) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA_CIPHER_SUITE failed!"));
//		}
//		
//		intVal = 34;
//		apmib_set(MIB_WLAN_WSC_AUTH, (void *)&intVal); // 34, wpa/wpa2 auto
//		
//		intVal = 8;
//		apmib_set(MIB_WLAN_WSC_ENC, (void *)&intVal); // aes
		
		wps_wizard_call = 1;
		
		system("echo 1 > /var/wps_start_pbc");
		system("sysconf init ap wlan_app");
		
		sprintf(tmpbuf, "%s -sig_pbc wlan0", _WSC_DAEMON_PROG);
		system(tmpbuf);
	}
	else if(!strcmp(strTrigger, "4")){	
//		wlan_idx = 0;
//		intVal = 0;
//		intMode = CLIENT_MODE;
//		ENCRYPT_T encrypt;
//		encrypt = ENCRYPT_WPA2;
//		
//		apmib_set(MIB_WLAN_MODE, (void *)&intMode);
//		apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
//		apmib_set(MIB_WLAN_WLAN_DISABLED, (void *)&intVal);
//		if ( apmib_set(MIB_WLAN_ENCRYPT, (void *)&encrypt) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA2_UNICIPHER failed!"));
//		}
//		
//		intVal = WPA_CIPHER_AES ;
//		
//		if ( apmib_set(MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intVal) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA2_UNICIPHER failed!"));
//		}
//		
//		if ( apmib_set(MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intVal) == 0) {
//			strcpy(tmpbuf, ("Set MIB_WLAN_WPA_CIPHER_SUITE failed!"));
//		}
//		
//		intVal = 34;
//		apmib_set(MIB_WLAN_WSC_AUTH, (void *)&intVal); // 34, wpa/wpa2 auto
//		
//		intVal = 8;
//		apmib_set(MIB_WLAN_WSC_ENC, (void *)&intVal); // aes
//		
//		apmib_update(CURRENT_SETTING);
//		apmib_reinit();
		
		apmib_get(MIB_WLAN_WSC_PIN_GEN, (void *)devicePin);	
		apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
		if (intVal){
			intVal = 0;			
			apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
			updateVapWscDisable(wlan_idx, intVal);
			apmib_update_web(CURRENT_SETTING);
			system("echo 1 > /var/wps_start_pin");
			run_init_script("bridge");					
		}
		else{						
    		sprintf(tmpbuf, "%s -sig_start wlan0", _WSC_DAEMON_PROG);
			system(tmpbuf);
		}
	}
	
	sprintf(xml_resraw, set_trigger_wps_result,
	 settings_result_form_head,
	 result,
	 devicePin,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

int do_GetCurrentWPSStatus(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char WPSStatus[8];
	int wps_status;
	FILE *fp;
	int i;
	struct stat status;
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(WPSStatus, 0, sizeof(WPSStatus));	
	
	strcpy(result, "OK");
	
	if ((fp = fopen("/tmp/wscd_status", "r")) == NULL)     	   		
   		printf("Cannot Read File!\n");
   	else
   		fscanf(fp, "%d", &wps_status);
   		
	fclose(fp);
	
	if(wps_status == 0)
		strcpy(WPSStatus, "started");
	else if(wps_status == 3 ){
		hnap_wps_count = 0; //reset wps_count.
		strcpy(WPSStatus, "success");
	}
	else{ // wps processing
		hnap_wps_count++;
		if(hnap_wps_count >= 120){
			printf("wps 120 sec time out\n"); // joe debug
			strcpy(WPSStatus, "failed");
			system("echo 1 > /tmp/wscd_cancel");
			hnap_wps_count=0;
	
			if ( stat("/var/wps_start_pbc", &status) >= 0) // file exist
				unlink("/var/wps_start_pbc");
			if ( stat("/var/wps_start_pin", &status) >= 0) // file exist
				unlink("/var/wps_start_pin");	
		}
	}
	
	printf("WPSStatus = %s\n",WPSStatus);	
	sprintf(xml_resraw, get_current_wps_status_result,
	 settings_result_form_head,
	 result,
	 WPSStatus,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	return 0;
}

int do_GetAPClientSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char radio_tmp[16];
	char tmpbuf[128];
	char strEnabled[8];
	char strSSID[128];
	unsigned char strMACaddr[32];
	char strType[32];
	char strEncryption[16];
	char strKey[128];
	char *ptr_ch_width;
	char val_ch_width;
	int iWLanEnable;
	int intChannelWidth;
	int intAuthType;
	int intWPAcipher;
	int intWPAauth;
	int intWepKeyType;
	int i;
	struct sockaddr hwaddr;
	unsigned char *pMacAddr;
	ENCRYPT_T intENCRYPT;
	WEP_T intWepKey;
	FILE *fp;
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(tmpbuf, 0, sizeof(tmpbuf));
	memset(strEnabled, 0, sizeof(strEnabled));
	memset(strSSID, 0, sizeof(strSSID));	
	memset(strMACaddr, 0, sizeof(strMACaddr));	
	memset(strType, 0, sizeof(strType));
	memset(strEncryption, 0, sizeof(strEncryption));	
	memset(strKey, 0, sizeof(strKey));		
	
	strcpy(result, "OK");
	
	do_get_element_value(raw, radio_tmp, "RadioID");
	if((strcasecmp(radio_tmp, "RADIO_2.4GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_24GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_5GHz") == 0)){
		apmib_get( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);
		apmib_get( MIB_WLAN_SSID,  (void *)strSSID);
		
		if(strcasecmp(radio_tmp, DEF_RadioID_5G) == 0)
			strcpy(WLAN_IF, "wlan0");
		else 
			strcpy(WLAN_IF, "wlan1");
			
		getInAddr( WLAN_IF, HW_ADDR, (void *)&hwaddr );
		pMacAddr = hwaddr.sa_data;
		sprintf(strMACaddr,"%02x:%02x:%02x:%02x:%02x:%02x", pMacAddr[0], pMacAddr[1], pMacAddr[2],
		 pMacAddr[3], pMacAddr[4], pMacAddr[5]);
		to_upper(strMACaddr);
		
		sprintf(tmpbuf, "/proc/wlan%d/mib_11n", wlan_idx);
		if ((fp = fopen(tmpbuf,"r"))==NULL){
			strcpy(result, "ERROR");
		}
		
		i=0;
		while ((strstr(tmpbuf, "use40M"))==NULL){
			fgets(tmpbuf, sizeof(tmpbuf), fp);
			if (i==10) //avoid to trap endless loop
				break;
			i++;
		}
		fclose(fp);

		ptr_ch_width = strstr(tmpbuf, "use40M") + 8; 
		val_ch_width = *ptr_ch_width;
		if(val_ch_width == '0')
			intChannelWidth = 20;
		else if(val_ch_width == '1')
			intChannelWidth = 0;
		else if(val_ch_width == '2')
			intChannelWidth = 1;
		else
			intChannelWidth = 0;
		
		apmib_get( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
		apmib_get( MIB_WLAN_AUTH_TYPE, (void *)&intAuthType);
		apmib_get( MIB_WLAN_WEP, (void *)&intWepKey);	
		apmib_get( MIB_WLAN_WEP_KEY_TYPE, (void *)&intWepKeyType);	
		apmib_get( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
		
		if( intENCRYPT == ENCRYPT_DISABLED ){
			strcpy(strType, "NONE");
		}
		else if( intENCRYPT == ENCRYPT_WEP ){
			if( intAuthType == 1 )
				strcpy(strType, "WEP-SHARED");
			else
				strcpy(strType, "WEP-BOTH");
			
			if( intWepKey == WEP64){
				strcpy(strEncryption, "WEP-64");
				apmib_get(MIB_WLAN_WEP64_KEY1,  (void *)tmpbuf);
				if(intWepKeyType == 0)
					returnWepKey(0, 5, tmpbuf, strKey);
				else
					returnWepKey(1, 5, tmpbuf, strKey);		
			}
			else if( intWepKey == WEP128){
				strcpy(strEncryption, "WEP-128");
				apmib_get(MIB_WLAN_WEP128_KEY1,  (void *)tmpbuf);
				if(intWepKeyType == 0)
					returnWepKey(0, 13, tmpbuf, strKey);
				else
					returnWepKey(1, 13, tmpbuf, strKey);						
			}
		}
		else if( intENCRYPT == ENCRYPT_WPA ){
			apmib_get( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
			if( intWPAauth == WPA_AUTH_PSK ){
				strcpy(strType, "WPA-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strKey);	
			}
			else{
				strcpy(strType, "WPA-RADIUS");
			}
			
			if( intWPAcipher == 1 )
				strcpy(strEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(strEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(strEncryption, "TKIPORAES");	
		}
		else if( intENCRYPT == ENCRYPT_WPA2 ){
			apmib_get( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
			if( intWPAauth == WPA_AUTH_PSK ){
				strcpy(strType, "WPA2-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strKey);	
			}				
			else{
				strcpy(strType, "WPA2-RADIUS");
			}	
			
			if( intWPAcipher == 1 )
				strcpy(strEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(strEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(strEncryption, "TKIPORAES");		
		}
		else if( intENCRYPT == ENCRYPT_WPA2_MIXED ){
			apmib_get( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
			
			if( intWPAauth == WPA_AUTH_PSK ){
				strcpy(strType, "WPAORWPA2-PSK");
				apmib_get(MIB_WLAN_WPA_PSK,  (void *)strKey);
			}
			else{
				strcpy(strType, "WPAORWPA2-RADIUS");
			}
			
			if( intWPAcipher == 1 )
				strcpy(strEncryption, "TKIP");
			else if( intWPAcipher == 2 )
				strcpy(strEncryption, "AES");
			else if( intWPAcipher == 3 )
				strcpy(strEncryption, "TKIPORAES");	
		} 
	}
	else{
		strcpy(result, "ERROR_BAD_RadioID");
	}

	sprintf(xml_resraw, get_apclient_settings_result,
	 settings_result_form_head,
	 result,
	 (iWLanEnable ? STR_FALSE : STR_TRUE),
	 strSSID,
	 strMACaddr,
	 intChannelWidth,
	 strType,
	 strEncryption,
	 strKey,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	return 0;
}

int do_SetAPClientSettings_to_xml(char *source_xml)
{
	int result=0;
	char radio_tmp[16];
	char strEnabled[8];
	char strSSID[128];
	unsigned char strMacAddr[32];
	unsigned char tmpMacAddr[32];
	unsigned char mac[32];
	char strChannelWidth[4];
	char strType[32];
	char strEncryption[16];
	char strKey_WEP[32];
	char strKey[128];
	int wlan_disabled;
	int channel_width;
	int intAuthType;
	int intWPAcipher;
	int intWPAauth;
	int i, j;
	WEP_T intWepKey;
	ENCRYPT_T intENCRYPT;
	
	
	memset(radio_tmp, 0, sizeof(radio_tmp));
	memset(strEnabled, 0, sizeof(strEnabled));
	memset(strSSID, 0, sizeof(strSSID));
	memset(strMacAddr, 0, sizeof(strMacAddr));
	memset(strChannelWidth, 0, sizeof(strChannelWidth));
	memset(strType, 0, sizeof(strType));
	memset(strEncryption, 0, sizeof(strEncryption));
	memset(strKey, 0, sizeof(strKey));
	memset(strKey_WEP, 0, sizeof(strKey_WEP));
	
	do_get_element_value(source_xml, radio_tmp, "RadioID");
	do_get_element_value(source_xml, strEnabled, "Enabled");
	do_get_element_value(source_xml, strSSID, "SSID");
	do_get_element_value(source_xml, strMacAddr, "MacAddress");
	do_get_element_value(source_xml, strChannelWidth, "ChannelWidth");
	do_get_element_value(source_xml, strType, "SecurityType");
	do_get_element_value(source_xml, strEncryption, "string");
	do_get_element_value(source_xml, strKey, "Key");
	
	do{	
		if((strcasecmp(radio_tmp, "RADIO_2.4GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_24GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_5GHz") == 0)){
			set_wlan_idx(radio_tmp);
			
			if(strcasecmp(strEnabled, "true") != 0)
				wlan_disabled = 1;
			else
				wlan_disabled = 0;
			apmib_set( MIB_WLAN_WLAN_DISABLED, (void *)&wlan_disabled);
			
			apmib_set( MIB_WLAN_SSID, (void *)strSSID);
		
			if (strcmp(strMacAddr, "")){
				for (i=0,j=0;i<17;i++){//remove ':' out of MacAddress
					if ( i==2 || i==5 || i==8 || i==11 || i==14 )
						continue;
					tmpMacAddr[j] = strMacAddr[i];
					j++;
				}
				string_to_hex(tmpMacAddr, mac, 12);
				apmib_set(MIB_HW_NIC0_ADDR, (void *)mac);
			}
			
			if (strcmp(strChannelWidth, "20"))
				channel_width = 0;
			else if (strcmp(strChannelWidth, "0"))
				channel_width = 1;
			else if (strcmp(strChannelWidth, "1"))
				channel_width = 2;
			else{
				result = 3;
				break;
			}
			apmib_set(MIB_WLAN_CHANNEL_BONDING, (void *)&channel_width);
			
			if( strncasecmp( strType, "NONE" , 4 ) == 0 )
			{
				intENCRYPT = 0;
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
			}
			else if( strncasecmp( strType, "WEP" , 3 ) == 0 ) {
				intENCRYPT = ENCRYPT_WEP;
	
				if( strcasecmp( strType, "WEP-SHARED" ) == 0 )
					intAuthType = 1;
				else if( strcasecmp( strType, "WEP-OPEN" ) == 0 )
					intAuthType = 2;
				else if( strcasecmp( strType, "WEP-BOTH" ) == 0 )
					intAuthType = 2;
					
				if( strcasecmp( strEncryption, "WEP-64" ) == 0 ){
					if( !strKey[0] || strlen(strKey) > 10 ){
						result = 6;
						break;
					}
					intWepKey = WEP64;
					string_to_hex(strKey, strKey_WEP, strlen(strKey));
					apmib_set(MIB_WLAN_WEP64_KEY1, (void *)strKey_WEP);
				}
				else if( strcasecmp( strEncryption, "WEP-128" ) == 0 ){
					if( !strKey[0] || strlen(strKey) > 26 ){
						result = 6;
						break;
					}
					intWepKey = WEP128;
					string_to_hex(strKey, strKey_WEP, strlen(strKey));
					apmib_set(MIB_WLAN_WEP128_KEY1, (void *)strKey_WEP);
				}
				else{
					result = 5;
					break;
				}
					
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_set( MIB_WLAN_AUTH_TYPE, (void *)&intAuthType);
				apmib_set( MIB_WLAN_WEP, (void *)&intWepKey);			
			}
			else if( strcasecmp( strType, "WPA-PSK" ) == 0 || strcasecmp( strType, "WPA2-PSK" ) == 0 || strcasecmp( strType, "WPAORWPA2-PSK" ) == 0 ){
				if( strcasecmp( strType, "WPA-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA;
				else if( strcasecmp( strType, "WPA2-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2;
				else if( strcasecmp( strType, "WPAORWPA2-PSK" ) == 0 )
					intENCRYPT = ENCRYPT_WPA2_MIXED;
				else{
					result = 4;
					break;
				}
				
				intWPAauth = WPA_AUTH_PSK;	
				if( !strKey[0] || strlen(strKey) > 63 ){
					result = 6;
					break;
				}
				
				if( strcasecmp( strEncryption, "TKIP" ) == 0 )
					intWPAcipher = 1;
				else if( strcasecmp( strEncryption, "AES" ) == 0 )
					intWPAcipher = 2;
				else if( strcasecmp( strEncryption, "TKIPORAES" ) == 0 )
					intWPAcipher = 3;
				else{
					result = 5;
					break;
				}
	
				apmib_set( MIB_WLAN_ENCRYPT, (void *)&intENCRYPT);
				apmib_set( MIB_WLAN_WPA_AUTH, (void *)&intWPAauth);
				apmib_set( MIB_WLAN_WPA_CIPHER_SUITE, (void *)&intWPAcipher);
				apmib_set( MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&intWPAcipher);
				apmib_set( MIB_WLAN_WPA_PSK,  (void *)strKey);
				apmib_set( MIB_WLAN_WSC_PSK,  (void *)strKey);
			}
			else{
				result = 4;
				break;
			}		
		}
		else
			result = 2;
	}while(0);
	
	return result;	
}

int do_SetAPClientSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetAPClientSettings_to_xml(raw);
	
	apmib_update(CURRENT_SETTING);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_RADIOID");
	else if(xml_result == 3)
		strcpy(result, "ERROR_BAD_CHANNEL_WIDTH");
	else if(xml_result == 4)
		strcpy(result, "ERROR_BAD_SECURITYTYPE");
	else if(xml_result == 5)
		strcpy(result, "ERROR_ENCRYPTION_NOT_SUPPORTED");
	else if(xml_result == 6)
		strcpy(result, "ERROR_ILLEAGL_KEY_VALUE");
		
	sprintf(xml_resraw, set_apclient_settings_result, 
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

int do_GetOperationMode(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[4096];
	char xml_resraw2[2048];
	char RadioID_24G_str[512];
	char RadioID_5G_str[512];
	char RadioID_24G_GUEST_str[512];
	char RadioID_5G_GUEST_str[512];
	char result[32];
	char OpMode[32];
	int OP_MODE = 0;
	int WLAN_MODE = 0;
	int REPEATER_MODE = 0;
	int REPEATER_MODE2 = 0;
	
	strcpy(result, "OK");
	memset(OpMode, 0, sizeof(OpMode));
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(xml_resraw2, 0, sizeof(xml_resraw2));
	
	apmib_get( MIB_WLAN_MODE, (void *)&WLAN_MODE);
	apmib_get( MIB_OP_MODE, (void *)&OP_MODE);
	apmib_get( MIB_REPEATER_ENABLED1, (void *)&REPEATER_MODE);
	apmib_get( MIB_REPEATER_ENABLED2, (void *)&REPEATER_MODE2);
	
	if( WLAN_MODE == 0 )
		strcpy(OpMode, "WirelessAp");
	else if( WLAN_MODE == 1 )
		strcpy(OpMode, "WirelessAdHoc");
			
	if( OP_MODE == BRIDGE_MODE && (REPEATER_MODE == 1 || REPEATER_MODE2 == 1) )
		strcpy(OpMode, "WirelessRepeater");
		
#ifndef FORM_TEW820 
	sprintf(RadioID_24G_str, get_operation_mode_str, DEF_RadioID_24G, OpMode, "<string>WirelessRepeater</string>");
	sprintf(RadioID_24G_GUEST_str, get_operation_mode_str, DEF_RadioID_24G_GUEST, OpMode, ,"<string>WirelessRepeater</string>");
#endif
	sprintf(RadioID_5G_str, get_operation_mode_str, DEF_RadioID_5G, OpMode, "");
	sprintf(RadioID_5G_GUEST_str, get_operation_mode_str, DEF_RadioID_5G_GUEST, OpMode, "");
	
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	sprintf(xml_resraw, get_operation_mode_result_head,
	 settings_result_form_head,
	 result);
	req_format_write(req,xml_resraw);
#ifndef FORM_TEW820
	req_format_write(req,RadioID_24G_str);
	req_format_write(req,RadioID_24G_GUEST_str);
#endif
	req_format_write(req,RadioID_5G_str);
	req_format_write(req,RadioID_5G_GUEST_str);
	sprintf(xml_resraw, get_operation_mode_result_foot,
	 settings_result_form_foot);
	req_format_write(req,xml_resraw);
	
	return 0;
}

int do_SetOperationMode_to_xml(char *source_xml)
{
	int result=0;
	char radio_tmp[16];
	char strOPMode[32];
	int WLAN_MODE;
	
	memset(radio_tmp, 0, sizeof(radio_tmp));
	memset(strOPMode, 0, sizeof(strOPMode));
	
	do_get_element_value(source_xml, radio_tmp, "RadioID");
	do_get_element_value(source_xml, strOPMode, "CurrentOPMode");
	if((strcasecmp(radio_tmp, "RADIO_2.4GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_24GHz") == 0) || (strcasecmp(radio_tmp, "RADIO_5GHz") == 0)){
		set_wlan_idx(radio_tmp);
		
		if(strcasecmp(strOPMode, "WirelessAp") == 0){
			WLAN_MODE = 0;
			apmib_set( MIB_WLAN_MODE, (void *)&WLAN_MODE);
		}
		else if(strcasecmp(strOPMode, "WirelessAdHoc") == 0){
			WLAN_MODE = 1;
			apmib_set( MIB_WLAN_MODE, (void *)&WLAN_MODE);
		}
#ifndef FORM_TEW820
		else if(strcasecmp(strOPMode, "WirelessRepeater") == 0){
			//Set wlan mode to Repeater mode.
		}
#endif
		else
			result = 3;
	}
	else
		result = 2;

	return result;	
}

int do_SetOperationMode(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetOperationMode_to_xml(raw);
	
	apmib_update(CURRENT_SETTING);
	
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_RADIOID");
	else if(xml_result == 3)
		strcpy(result, "ERROR_BAD_CurrentOPMode");
		
	sprintf(xml_resraw, set_operation_mode_result, 
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

}

int do_Reboot(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	//printf("%s\n",__FUNCTION__);
	
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,reboot_result);
	
	system("reboot");
	
	return 0;
}

int do_unknown_call(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	printf("%s\n",__FUNCTION__);
	return 0;
}

void parse_multi_xml(char *source_xml, char *xml, char *next_xml, char *tag){
	char *start_ptr = NULL;
	char *end_ptr = NULL;
	char start[35];
	char end[35];
	int len = 0;
	int next_len = 0;

	memset(start, 0, sizeof(start));
	memset(end, 0, sizeof(end));
	
	sprintf(start, "<%s xmlns", tag);
	sprintf(end, "</%s>", tag);

	if((start_ptr = strstr(source_xml, start)) != NULL){
		len = 0;
		end_ptr = strstr(start_ptr, end);
		
		if (start_ptr >= end_ptr){
			len = start_ptr - end_ptr;
		}
		else{
			len = end_ptr - start_ptr;
			next_len = end_ptr - source_xml;
		}
		
		strncpy(xml, start_ptr, len);
		source_xml += next_len;
		strcpy(next_xml, source_xml);
	}
}

int do_xml_tag_verify(
	const char *xraw,
	const char *tag,
	char **tstart)
{
	/* ret is contents size */
	int ret = -1;
	/* XXX: assuming tag size 128 bytes */
	char *pb, *pbe, *pe, bxt[128], bxt2[128], ext[128];

	sprintf(bxt, "<%s>", tag);
	sprintf(bxt2, "<%s ", tag);
	sprintf(ext, "</%s>", tag);
	
	if ((pb = strstr(xraw, bxt)) == NULL && (pb = strstr(xraw, bxt2)) == NULL)
		goto out;
	if ((pbe = strstr(pb, "/>")) == NULL && (pbe = strstr(pb, ">")) == NULL)
		goto out;
	if (*pbe == '>' && (pe = strstr(pbe + 1, ext)) == NULL)
		goto out;
	if (*pbe == '/'){	/* End of tag is <..../> */
		ret = 0;
	}
	else {
		*tstart = pbe + 1;
		ret = pe - *tstart;
	}

out:
	return ret;
}

int do_xml_mapping(const char *xraw, hnap_entry_s *p , request *req)
{
	int size, ret = -1;
	char *ps = NULL, buf[2048];		/* XXX: assuming tag contents 2KB */
	HNAP_SESSION_T session_type = SIMPLE;
/*====================================================================*/
//Carl : change <XXX/> tag to <XXX></XXX>
	char copy_xraw[2048], old_string[32][64]={0}, new_string[32][128]={0}, replace_string[128];
	int i, j=0, start=0, end=0;
	
	memset(copy_xraw, 0, sizeof(copy_xraw));
	strcpy(copy_xraw,xraw);
	
	for (i=0; i < strlen(copy_xraw); i++){
    	if(copy_xraw[i] == '<')
           start = i;
        if(copy_xraw[i] == '/' && copy_xraw[i+1] == '>')
           end = i+1;
        
        if(start != 0 && end != 0 && (start < end)){
      		strncpy(old_string[j], &copy_xraw[start], (end - start + 1));
      		strncpy(new_string[j], &copy_xraw[start+1], (end - start - 2));
      		j++;
       		start = end;
      	}	
    }
    
	for(j=0;j<16;j++){
		if(strlen(new_string[j]) > 0 && strlen(new_string[j]) < 32){
			//printf("new_string[%d]=%s\n", j, new_string[j]);
			memset(replace_string, 0, sizeof(replace_string));
			sprintf(replace_string, "<%s></%s>", new_string[j], new_string[j]);
			replace(copy_xraw, old_string[j], replace_string); 
		}		
	}	
	
	//printf("%s\n", copy_xraw);
/*====================================================================*/
	for (; p && p->fn; p++) {
		bzero(buf, sizeof(buf));

		if (p->tag == NULL && ret == -1) {	/* unknown call */
			ret = p->fn(p->key, buf, session_type , req);
			break;
		} else if ((size = do_xml_tag_verify(copy_xraw, p->tag, &ps)) >= 0) {
			strncpy(buf, ps, size);
			printf(" p->tag = %s \n",p->tag); // joe debug
			ret = p->fn(p->key, buf, session_type , req);
			break;
		}
	}
  	printf("%s\n",__FUNCTION__); // joe debug
	return ret;
}

#define POST_BUF_SIZE	10000
#define  MIN(a, b)               (((a)<(b))?(a):(b))
int do_xml_parser(char *xraw, request *req)
{
	hnap_entry_s ls[] = {
		{ "GetDeviceSettings",				NULL,	do_GetDeviceSettings },
		{ "SetDeviceSettings",				NULL,	do_SetDeviceSettings },
		{ "IsDeviceReady",					NULL,	do_IsDeviceReady },
		{ "GetWLanRadioSettings",			NULL,	do_GetWLanRadioSettings },
		{ "SetWLanRadioSettings",			NULL,	do_SetWLanRadioSettings },
		{ "GetWLanRadioSecurity",			NULL,	do_GetWLanRadioSecurity },
		{ "SetWLanRadioSecurity",			NULL,	do_SetWLanRadioSecurity },	
		{ "GetWLanRadios",					NULL,	do_GetWLanRadios },	
		{ "SetTriggerWirelessSiteSurvey",	NULL,	do_SetTriggerWirelessSiteSurvey },
		{ "GetSiteSurvey",					NULL,	do_GetSiteSurvey },
		{ "GetWanSettings",					NULL,	do_GetWanSettings },
		{ "SetWanSettings",					NULL,	do_SetWanSettings },
		{ "SetTriggerWPS",					NULL,	do_SetTriggerWPS },	
		{ "GetCurrentWPSStatus",			NULL,	do_GetCurrentWPSStatus },
		{ "GetAPClientSettings",			NULL,	do_GetAPClientSettings },
		{ "SetAPClientSettings",			NULL,	do_SetAPClientSettings },
		{ "GetOperationMode",				NULL,	do_GetOperationMode },
		{ "SetOperationMode",				NULL,	do_SetOperationMode },
		{ "Reboot",							NULL,	do_Reboot },
		{ NULL, 							NULL, 	do_unknown_call },
		{ NULL, 							NULL, 	NULL}
	};

	printf("%s\n",__FUNCTION__);
	//printf("xraw = %s\n", xraw);
	
	if(strlen(xraw) > POST_BUF_SIZE)
		return 0;
	else
		return do_xml_mapping(xraw, ls , req);
}

void do_hnap_post(char *url, FILE *stream, int len, char *boundary, request *req)
{	
	char post_buf[POST_BUF_SIZE];
	printf("%s(%d):%s\n",__FILE__,__LINE__,__FUNCTION__);
	/* Get query */
	if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream))
		return;
	len -= strlen(post_buf);		
	if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream))
		return;
	len -= strlen(post_buf);
	printf("%s(%d):post_buf=%s\n",__FILE__,__LINE__,post_buf);
	/* Slurp anything remaining in the request */
	while (len--)
		(void) fgetc(stream);
	do_xml_parser(post_buf,req);
}

void do_hnap(char *url, FILE *stream)
{
	printf("%s(%d):%s\n",__FILE__,__LINE__,__FUNCTION__);
}

void replace(char *ori_string, char *target_string, char *replace_string)
{
	char buffer[4096];
	char *ch;
	
	if(!(ch = strstr(ori_string, target_string)))
		return;
	
	strncpy(buffer, ori_string, ch-ori_string);
	
	buffer[ch-ori_string] = 0;
	
	sprintf(buffer+(ch - ori_string), "%s%s", replace_string, ch + strlen(target_string));
	
	ori_string[0] = 0;
	strcpy(ori_string, buffer);
	
	return replace(ori_string, target_string, replace_string);
}

int hnap_sitesurvey()
{
	char command[64];
	unsigned char res;
	int status;
	int wait_time;
	
	sprintf(command,"ifconfig wlan0 down");
	system(command);
	sprintf(command,"ifconfig wlan0 up");
	system(command);

	wait_time = 0;
	while (1) {
		switch(getWlSiteSurveyRequest("wlan0", &status)) 
		{ 
			case -2: 
				printf("-2\n"); 
				printf("Auto scan running!!please wait...\n"); 
				break; 
			case -1: 
				printf("-1\n"); 
				printf("Site-survey request failed!\n"); 
				break; 
			default:
				break; 
		} 
	
		if (status != 0) {	// not ready
			if (wait_time++ > 5) {
				printf("scan request timeout!");
			}
			sleep(1);
		}
		else
			break;
	}
	
	// wait until scan completely
	wait_time = 0;
	while (1) {
		res = 1;	// only request request status
		if ( getWlSiteSurveyResult("wlan0", (SS_STATUS_Tp)&res) < 0 ) {
			printf("Read site-survey status failed!");
			free(pStatus);
			pStatus = NULL;
		}
		if (res == 0xff) {   // in progress
			/*prolong wait time due to scan both 2.4G and 5G */
			if (wait_time++ > 30) {
				printf("scan timeout!");
				free(pStatus);
				pStatus = NULL;
			}
			sleep(1);
		}
		else
			break;
	}

	return 0;
}

#define HNAP_SPECIAL_CHAR_LEN 6
char hnap_xml_char[] = {'&', '<', '>', '"', '\'', '%'};

void encode_hnap_special_char(char *str){
	char *replace_str;
	int i, j, len, find, count;

	len = strlen(str);
	replace_str = malloc(sizeof(char) * (len * 6));

	memset(replace_str, 0, sizeof(char) * (len * 6));
	count = 0;

	for (i = 0; i < len; i++){
		find = 0;

		for (j = 0; j < HNAP_SPECIAL_CHAR_LEN; j++){
			if (str[i] == hnap_xml_char[j]){	// find special characters
				find = 1;
				break;
			}
		}

		if (find){
			if (str[i] == '%'){	// we need to add one more %, otherwise xml format will be error
				replace_str[count++] = str[i];
				replace_str[count++] = '%';
			}else{
				replace_str[count++] = '&';
				switch(j){
					case 0:	// &
						replace_str[count++] = 'a';
						replace_str[count++] = 'm';
						replace_str[count++] = 'p';
						replace_str[count++] = ';';
						break;
					case 1:	// <
						replace_str[count++] = 'l';
						replace_str[count++] = 't';
						replace_str[count++] = ';';
						break;
					case 2:	// >
						replace_str[count++] = 'g';
						replace_str[count++] = 't';
						replace_str[count++] = ';';
						break;
					case 3:	// "
						replace_str[count++] = 'q';
						replace_str[count++] = 'u';
						replace_str[count++] = 'o';
						replace_str[count++] = 't';
						replace_str[count++] = ';';
						break;
					case 4:	// '
						replace_str[count++] = 'a';
						replace_str[count++] = 'p';
						replace_str[count++] = 'o';
						replace_str[count++] = 's';
						replace_str[count++] = ';';
						break;
				}
			}
		}else{
			replace_str[count++] = str[i];
		}
	}

	replace_str[count] = '\0';

	memset(str, 0, sizeof(str));
	memcpy(str, replace_str, count);

	free(replace_str);
}