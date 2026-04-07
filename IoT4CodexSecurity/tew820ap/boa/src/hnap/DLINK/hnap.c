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
static SS_STATUS_Tp pStatus=NULL;

extern int BULK_FirmwareUpgrade(char *upload_data, int upload_len, int is_root, char *buffer);
extern char langCategory[];
extern char *fwVersion;	// defined in version.c
extern char *modelName;	// defined in version.c
extern char *hwVersion; // defined in version.c
extern char *fwdomain; // defined in version.c
extern void send_headers( request *wp, int status, char* title, char* extra_header, char* mime_type );
extern char *to_upper(char *str);
#define send_raw_data(content) 

char fwQuery_url[256]={0};
char fwDownload_url[256]={0};
void set_fwQuery_url(){
	
	char query_hwVersion[2]={0};
	unsigned char tmp_mac[6];
	char mac_addr[32]={0};
	char *Major;
	char *Minor;
	char fwVersion_tmp[8];
	
	apmib_get( MIB_HW_NIC0_ADDR, (void *)tmp_mac);
	sprintf(mac_addr,"%02x%02x%02x%02x%02x%02x", tmp_mac[0],tmp_mac[1],tmp_mac[2],tmp_mac[3],tmp_mac[4],tmp_mac[5]);

	strcpy(fwVersion_tmp, fwVersion);
	Major=strtok(fwVersion_tmp, ".");
	Minor=strtok(NULL, ".");
	strcat(Major,Minor);
	
	strcat(strncpy(query_hwVersion,hwVersion,1),"x");
	sprintf(fwQuery_url,"wrpd.dlink.com,/router/firmware/query.aspx?model=%s_%s_%s_FW_0%s_%s", modelName, 
	  query_hwVersion, (strcmp(fwdomain,"WW"))?fwdomain:"Default", Major, mac_addr);
	
	//printf("fwQuery_url = %s\n",fwQuery_url);
}

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
	
	set_fwQuery_url();
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "online_firm -i br0 -u %s -v %s", fwQuery_url, fwVersion);
	//printf("command = %s\n",buf);
	system(buf);
	sleep(3);
	
	if ((fp = fopen("/tmp/fw_check_result.txt", "r")) == NULL)           	    		
	   	printf("Read File Error!\n");
   	else{
   		memset(buf, 0, sizeof(buf));
   		strtmp = NULL;
   		fgets(buf, sizeof(buf), fp);
   		
   		if ((strncmp(buf, "ERROR", 5))==0 || ((strncmp(buf, "LATEST", 6))==0))
			strcpy(latest_fwVerion, "");
		else{	
			//latest_ver
	   		strtmp = gettoken(buf, 0, '+');      	   			
	   		strcpy(latest_fwVerion, strtmp[0]=='0'?++strtmp:strtmp );
	   	}
	}
	system("rm /tmp/fw_check_result.txt");
	
	send_headers( req,  200, "Ok", (char*) 0, "text/xml" );	
	sprintf(xml_resraw,get_device_settings_result,
	  settings_result_form_head,
	  buffer,
	  modelName,
	  fwVersion,
	  fwdomain,
	  latest_fwVerion,
	  hwVersion,
	  (intVal ? STR_TRUE:STR_FALSE),
	  STR_FALSE);
	req_format_write(req, xml_resraw);
	
	memset(xml_resraw, 0, sizeof(xml_resraw));  
	sprintf(xml_resraw,get_device_settings_result2,
	  modelName,
	  settings_result_form_foot);
	req_format_write(req, xml_resraw);
	
	//send_headers( req,  200, "Ok", (char*) 0, "text/xml" );	
	//req_format_write(req, xml_resraw );

	return 0;
}

//int do_SetDeviceSettings_to_xml(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
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
		/*
		if( !strDeviceName[0] || strlen(strDeviceName) > 15 )
			break;
			*/
		
		do_get_element_value(source_xml, strAdminPassword, "AdminPassword");
		do_get_element_value(source_xml, ChangePassword, "ChangePassword");
		if(strcasecmp( ChangePassword, STR_TRUE ) == 0){
			//if( strlen(strAdminPassword) != 0)
			apmib_set(MIB_USER_PASSWORD, (void *)strAdminPassword);
		}
		
		do_get_element_value(source_xml, strBuffer, "CAPTCHA");
		if( strlen(strBuffer) != 0)
		{
			if( strcasecmp( strBuffer, STR_TRUE ) == 0 )
				iCAPTCHA = 1;
			else
				iCAPTCHA = 0;	
			apmib_set(MIB_GRAPH_AUTH, (void *)&iCAPTCHA);
		}
		
		not_def = 1;
		apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);
		
		apmib_update(CURRENT_SETTING);
		result = 0;
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
}

//Tim Wang
//2012/06/29
typedef struct time_zone_s {
	const char *GMT;
	const char *time_key;
} time_zone;

//Tim Wang
//2012/06/29
char* do_TimeZoneList(char* timezone, char* timezone_string)
{
	time_zone ntp_zone_array[]={
		{"GMT-12:00","12\ 1"},
		{"GMT-11:00","11\ 1"},
		{"GMT-10:00","10\ 1"},
		{"GMT-09:00","9\ 1"},
		{"GMT-08:00","8\ 1"},
		{"GMT-07:00","7\ 1"},
		{"GMT-07:00","7\ 2"},
		{"GMT-07:00","7\ 3"},
		{"GMT-06:00","6\ 1"},
		{"GMT-06:00","6\ 2"},
		{"GMT-06:00","6\ 3"},
		{"GMT-06:00","6\ 4"},
		{"GMT-05:00","5\ 1"},
		{"GMT-05:00","5\ 2"},
		{"GMT-05:00","5\ 3"},
		{"GMT-04:00","4\ 1"},
		{"GMT-04:00","4\ 2"},
		{"GMT-04:00","4\ 3"},
		{"GMT-03:30","3\ 1"},
		{"GMT-03:00","3\ 2"},
		{"GMT-03:00","3\ 3"},
		{"GMT-03:00","3\ 4"},
		{"GMT-02:00","2\ 1"},
		{"GMT-01:00","1\ 1"},
		{"GMT-01:00","1\ 2"},
		{"GMT","0\ 1"},
		{"GMT","0\ 2"},
		{"GMT+01:00","-1\ 1"},
		{"GMT+01:00","-1\ 2"},
		{"GMT+01:00","-1\ 3"},
		{"GMT+01:00","-1\ 4"},
		{"GMT+01:00","-1\ 5"},
		{"GMT+02:00","-2\ 1"},
		{"GMT+02:00","-2\ 2"},
		{"GMT+02:00","-2\ 3"},
		{"GMT+02:00","-2\ 4"},
		{"GMT+02:00","-2\ 5"},
		{"GMT+02:00","-2\ 6"},
		{"GMT+02:00","-2\ 7"},
		{"GMT+03:00","-3\ 1"},
		{"GMT+03:00","-3\ 2"},
		{"GMT+03:00","-3\ 3"},
		{"GMT+03:30","-3\ 4"},
		{"GMT+04:00","-4\ 1"},
		{"GMT+04:00","-4\ 2"},
		{"GMT+04:00","-4\ 3"},
		{"GMT+04:00","-4\ 4"},
		{"GMT+04:30","-4\ 5"},
		{"GMT+05:00","-5\ 1"},
		{"GMT+05:30","-5\ 2"},
		{"GMT+06:00","-6\ 1"},            
		{"GMT+06:00","-6\ 2"},
		{"GMT+06:00","-6\ 3"},
		{"GMT+06:00","-6\ 4"},
		{"GMT+06:30","-6\ 5"},
		{"GMT+07:00","-7\ 1"},
		{"GMT+08:00","-8\ 1"},
		{"GMT+08:00","-8\ 2"},
		{"GMT+08:00","-8\ 3"},
		{"GMT+08:00","-8\ 4"},
		{"GMT+08:00","-8\ 5"},
		{"GMT+08:00","-8\ 6"},
		{"GMT+09:00","-9\ 1"},
		{"GMT+09:00","-9\ 2"},
		{"GMT+09:30","-9\ 3"},
		{"GMT+09:30","-9\ 4"},
		{"GMT+10:00","-10\ 1"},
		{"GMT+10:00","-10\ 2"},
		{"GMT+10:00","-10\ 3"},
		{"GMT+10:00","-10\ 4"},
		{"GMT+10:00","-10\ 5"},
		{"GMT+11:00","-11\ 1"},
		{"GMT+11:00","-11\ 2"},
		{"GMT+12:00","-12\ 1"},
		{"GMT+12:00","-12\ 2"},
		{"GMT+13:00","-13\ 1"},
		{NULL,NULL}
	};

	time_zone* p = ntp_zone_array;

	if(strncmp(timezone, "GMT", 3) == 0){
		for(; p->GMT != NULL; p++)
			if(strcmp(p->GMT,timezone) == 0){
				strcpy(timezone_string,p->time_key);
			}
		return 1;
	}else{
		for(; p->time_key != NULL; p++)
			if(strcmp(p->time_key,timezone) == 0){
				strcpy(timezone_string,p->GMT);
			}
		return 1;
	}
}

//Tim Wang
//2012/06/19
int do_GetDeviceSettings2(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	unsigned char SerialNumber[32];
	unsigned char tmp_mac[6];
	unsigned char tmp_TimeZone[16];
	char TimeZone[16];
	char AutoAdjustDST[8];
	int DayLightSave;
	char Locale[16];
	char *SupportedLocales = NULL;
	char SSL[8];
	
	strcpy(result, "OK");
	strcpy(Locale, "en-us");
	strcpy(SSL, STR_FALSE);

	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(SerialNumber, 0, sizeof(SerialNumber));
	memset(tmp_mac, 0, sizeof(tmp_mac));
	memset(tmp_TimeZone, 0, sizeof(tmp_TimeZone));
	memset(TimeZone, 0, sizeof(TimeZone));

	apmib_get(MIB_HW_NIC0_ADDR, (void *)tmp_mac);
	apmib_get(MIB_NTP_TIMEZONE, (void *)tmp_TimeZone);

	do_TimeZoneList(tmp_TimeZone,TimeZone);
	
	sprintf(SerialNumber,"%02x:%02x:%02x:%02x:%02x:%02x", tmp_mac[0],tmp_mac[1],tmp_mac[2],tmp_mac[3],tmp_mac[4],tmp_mac[5]);

	apmib_get( MIB_DAYLIGHT_SAVE, (void *)&DayLightSave);
	if(DayLightSave == 1)
		strcpy(AutoAdjustDST, STR_TRUE);
	else
		strcpy(AutoAdjustDST, STR_FALSE);
	
	if (strcmp(langCategory, "DEFAULT") == 0)
		strcpy(Locale, "en-US");
	else{
		if(CheckLangId(LANG_PATH_TW))
			strcpy(Locale, "zh-TW");
		if(CheckLangId(LANG_PATH_CN))
			strcpy(Locale, "zh-CN");
		if(CheckLangId(LANG_PATH_EN))
			strcpy(Locale, "en-US");
		if(CheckLangId(LANG_PATH_FR))
			strcpy(Locale, "fr-FR");
		if(CheckLangId(LANG_PATH_GE))
			strcpy(Locale, "de-DE");
		if(CheckLangId(LANG_PATH_IT))
			strcpy(Locale, "fr-IT");
		if(CheckLangId(LANG_PATH_JP))
			strcpy(Locale, "ja");
		if(CheckLangId(LANG_PATH_SP))
			strcpy(Locale, "es-ES");
		if(CheckLangId(LANG_PATH_KR))
			strcpy(Locale, "ko-KR");
	}
		
	strcpy(SSL, "false");
	
	sprintf(xml_resraw, get_device_settings2_result,
		settings_result_form_head, 
		result, 
		SerialNumber, 
		TimeZone, 
		AutoAdjustDST, 
		Locale, 
		SupportedLocales, 
		SSL,
		settings_result_form_foot);
		send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

//Tim Wang
//2012/06/20
int do_SetDeviceSettings2(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetDeviceSettings2_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "REBOOT");
	else if(xml_result == 3)
		strcpy(result, "ERROR_USERNAME_NOT_SUPPORTED");
	else if(xml_result == 4)
		strcpy(result, "ERROR_TIMEZONE_NOT_SUPPORTED");
	
	sprintf(xml_resraw, set_device_settings2_result, 
		settings_result_form_head,
		result,
		settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/19
int do_SetDeviceSettings2_to_xml(char *source_xml)
{
	char TimeZone[9];
	char TimeZone2[16];
	char AutoAdjustDST[8];
	char Locale[32];
	char Username[16];
	char SSL[8];
	int True = 1;
	int False = 0;
	int not_def=0;

	printf("[%s]\n", __FUNCTION__);	
	
	do_get_element_value(source_xml, TimeZone, "TimeZone");
	do_get_element_value(source_xml, Locale, "Locale");
	do_get_element_value(source_xml, Username, "Username");
	do_get_element_value(source_xml, SSL, "SSL");
	do_get_element_value(source_xml, AutoAdjustDST, "AutoAdjustDST");

	do_TimeZoneList(TimeZone,TimeZone2);
	apmib_set(MIB_NTP_TIMEZONE, (void *)TimeZone2);
	
	if(strncasecmp(AutoAdjustDST, STR_TRUE, 4) == 0)
		apmib_set( MIB_DAYLIGHT_SAVE, (void *)&True);
	else
		apmib_set( MIB_DAYLIGHT_SAVE, (void *)&False);

	not_def = 1;
	apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);	
	apmib_update(CURRENT_SETTING);
	return 0;
}

int do_IsDeviceReady(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	//printf("%s\n",__FUNCTION__);

	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,is_device_ready_result);

	return 0;
}

int do_GetFactoryDefaults(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int iDefault = 0;

	//printf("%s\n",__FUNCTION__);
	strcpy(result, "OK");
	apmib_get( MIB_RESERVED_WORD_4, (void *)&iDefault);

	sprintf(xml_resraw,get_Factory_Defaults_result,
		settings_result_form_head,
		result,
		( iDefault ? STR_FALSE : STR_TRUE ),
		settings_result_form_foot ); 
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	return 0;
}

int do_SetFactoryDefault(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	//printf("%s\n",__FUNCTION__);

	system("flash default-sw");
	sleep(3);
	
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,is_Restore_Factory_Defaults_result);
	system("reboot");
	
	//return 0;
}

int do_Reboot(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	//printf("%s\n",__FUNCTION__);
	
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,reboot_result);
	
	system("reboot");
	
	return 0;
}

int do_GetFirmwareSettings(const char *key, char *raw, HNAP_SESSION_T session_type, request *req)
{
	char xml_resraw[2*1024];
	char buffer[128];
	
	printf("%s\n",__FUNCTION__);

	sprintf(buffer,"%sT00:00:00",fwDate);
	buffer[4]=buffer[7]=':';
	sprintf(xml_resraw,get_firmware_settings_result,
		settings_result_form_head,
		modelName,
		hwVersion,
		fwVersion,
		buffer,
		settings_result_form_foot );
	
	//send_headers( req,  200, "Ok", (char*) 0, "text/xml" );
	//req_format_write(req,xml_resraw);
	
	send_headers( req, 200, "Ok", (char*) 0, "text/xml" ); 
  req_format_write(req, xml_resraw);
	
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

//int do_SetWLanRadioSettings_to_xml(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
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
		/*
		if (strcasecmp( strRadioID, DEF_RadioID_24G ) == 0)
		{
			apmib_set( MIB_WLAN_WLAN_DISABLED, (void *)&iWLanEnable);		
			apmib_set( MIB_WLAN_SSID, (void *)strSSID);
		}
		else //guestzone
		{
			iWLanEnable != iWLanEnable;
			apmib_set( MIB_GZ_SSID24G_ENABLE, (void *)&iWLanEnable);		
			apmib_set( MIB_GZ_SSID24G, (void *)strSSID);			
		}	
		*/
				
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

//int do_SetWLanRadioSecurity_to_xml(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
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

//Tim Wang
//2012/06/18
int do_GetWiFiOpMode(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int OP_MODE = 0;
	int WLAN_MODE = 0;
	int REPEATER_MODE = 0;
	char WiFiOpMode[16];
	
	strcpy(result, "OK");
	memset(WiFiOpMode, 0, sizeof(WiFiOpMode));
	
	apmib_get( MIB_WLAN_MODE, (void *)&WLAN_MODE);
	apmib_get( MIB_OP_MODE, (void *)&OP_MODE);
	apmib_get( MIB_REPEATER_ENABLED1, (void *)&REPEATER_MODE);
	
	if( WLAN_MODE == 0 )
		strcpy(WiFiOpMode, "AP");
	else if( WLAN_MODE == 1 )
		strcpy(WiFiOpMode, "AP Client");
		
	if( OP_MODE == BRIDGE_MODE && REPEATER_MODE == 1 ){
			strcpy(WiFiOpMode, "Repeater");
	}
	if( OP_MODE == WISP_MODE )
			strcpy(WiFiOpMode, "WiFi HotSpot");
	if( OP_MODE == GATEWAY_MODE && WLAN_MODE == AP_MODE )
		strcpy(WiFiOpMode, "Router");

	printf("WiFiOpMode=%s\n",WiFiOpMode);
	printf("OP_MODE=%d;WLAN_MODE=%d;REPEATER_MODE=%d\n",OP_MODE,WLAN_MODE,REPEATER_MODE);
	
	sprintf(xml_resraw, get_wifi_op_mode_result,
	 settings_result_form_head,
	 result,
	 WiFiOpMode,
	 settings_result_form_foot);
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	return 0;
}

//Tim Wang
//2012/06/20
int do_SetWiFiOpMode(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetWiFiOpMode_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_WiFiOpMode");
	else if(xml_result == 3)	
		strcpy(result, "REBOOT");
	
	sprintf(xml_resraw, set_wifi_op_mode_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/18
int do_SetWiFiOpMode_to_xml(char *source_xml)
{
	int OP_MODE = 0;
	int WLAN_MODE = 0;
	int REPEATER_MODE = 0;
	char WiFiOpMode[16];
	int not_def=0;
	
	printf("[%s]\n", __FUNCTION__);	
	
	memset(WiFiOpMode, 0, sizeof(WiFiOpMode));
	
	do_get_element_value(source_xml, WiFiOpMode, "WiFiOpMode");
	
	if(!strcmp(WiFiOpMode,"AP"))
	{
		OP_MODE = BRIDGE_MODE;
		WLAN_MODE = AP_MODE;
		REPEATER_MODE = 0;
	}
	else if(!strcmp(WiFiOpMode,"AP Client"))
	{
		OP_MODE = BRIDGE_MODE;
		WLAN_MODE = CLIENT_MODE;
		REPEATER_MODE = 0;
	}
	else if(!strcmp(WiFiOpMode,"Repeater"))
	{
		OP_MODE = BRIDGE_MODE;
		WLAN_MODE = CLIENT_MODE;
		REPEATER_MODE = 1;
	}
	else if(!strcmp(WiFiOpMode,"WiFiHotSpot"))
	{
		OP_MODE = WISP_MODE;
		WLAN_MODE = CLIENT_MODE;
		REPEATER_MODE = 0;
	}
	else if(!strcmp(WiFiOpMode,"Router"))
	{
		OP_MODE = GATEWAY_MODE;
		WLAN_MODE = AP_MODE;
		REPEATER_MODE = 0;		
	}
	else	
		return 2;			

	apmib_set( MIB_WLAN_MODE, (void *)&WLAN_MODE);
	apmib_set( MIB_OP_MODE, (void *)&OP_MODE);
	apmib_set( MIB_REPEATER_ENABLED1, (void *)&REPEATER_MODE);
	not_def = 1;
	apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);	
	apmib_update(CURRENT_SETTING);

	return 0;
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
		
	if( OP_MODE == BRIDGE_MODE && (REPEATER_MODE == 1 || REPEATER_MODE2 == 1) )
		strcpy(OpMode, "WirelessRepeater");

	sprintf(RadioID_24G_str, get_operation_mode_str, DEF_RadioID_24G, OpMode);
	sprintf(RadioID_24G_GUEST_str, get_operation_mode_str, DEF_RadioID_24G_GUEST, OpMode);
	sprintf(RadioID_5G_str, get_operation_mode_str, DEF_RadioID_5G, OpMode);
	sprintf(RadioID_5G_GUEST_str, get_operation_mode_str, DEF_RadioID_5G_GUEST, OpMode);
	
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	sprintf(xml_resraw, get_operation_mode_result_head,
	 settings_result_form_head,
	 result);
	req_format_write(req,xml_resraw);
	req_format_write(req,RadioID_24G_str);
	req_format_write(req,RadioID_24G_GUEST_str);
	req_format_write(req,RadioID_5G_str);
	req_format_write(req,RadioID_5G_GUEST_str);
	sprintf(xml_resraw, get_operation_mode_result_foot,
	 settings_result_form_foot);
	req_format_write(req,xml_resraw);
	
	return 0;
}

//Tim Wang
//2012/06/14
int do_GetUserLimit(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char radioID[32] = "RADIO_2.4GHz";;
	char MSSIDIndex[8] = "SSID0";
	int Number = 0;

	memset(radioID, 0, sizeof(radioID));
	memset(MSSIDIndex, 0, sizeof(MSSIDIndex));
	wlan_idx = 1;
	apmib_get(MIB_WLAN_RESERVED_WORD_1, (void *)&Number);	 //MIB_WLAN_STA_NUM

	sprintf(xml_resraw, get_user_limit_result,
	 settings_result_form_head,
	 result, 
	 radioID, 
	 MSSIDIndex, 
	 Number,
	 settings_result_form_foot);
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

//Tim Wang
//2012/06/19
int do_SetUserLimit(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetUserLimit_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_RadioID");
	else if(xml_result == 3)	
		strcpy(result, "ERROR_BAD_MSSIDIndex");
	else if(xml_result == 4)
		strcpy(result, "REBOOT");
	
	sprintf(xml_resraw, set_user_limit_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req, 200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/14
int do_SetUserLimit_to_xml(char *source_xml)
{
	char radioID[16];
	char MSSIDIndex[16];
	char Number[16];
	char temp[16];
	int mssid_index;
	int iNumber = 0;
	int not_def=0;

	do_get_element_value(source_xml, radioID, "RadioID");
	do_get_element_value(source_xml, MSSIDIndex, "MSSIDIndex");
	do_get_element_value(source_xml, Number, "Number");
	
	if ((strcasecmp(radioID, DEF_RadioID_24G) == 0) || (strcasecmp(radioID, DEF_RadioID_5G) == 0)){	
		set_wlan_idx(radioID);
		
		if(MSSIDIndex[0])
			vwlan_idx = atoi(strcpy(temp, MSSIDIndex + 4));
		else
			return 3;

		iNumber = atoi(Number);

		if(iNumber >= 0 && iNumber <= 32)
			apmib_set(MIB_WLAN_RESERVED_WORD_1, (void *)&iNumber);  //MIB_WLAN_STA_NUM
		else
			return 1;
	}
	else
		return 2;

	not_def = 1;
	apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);
	apmib_update(CURRENT_SETTING);
	vwlan_idx = 0;
	return 0;
}

//extern unsigned char *gettoken(const unsigned char *str,unsigned int index,unsigned char symbol);
int do_GetFirmwareStatus(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char *IsTheLatest = STR_TRUE;
	char buf[128];
	FILE *fp;
	unsigned char *strtmp;
	char latest_fwVerion[16]={0};
	char latest_fwDate[16]={0};
	char hostName[32]={0};
	
	strcpy(result, "OK");
	
	set_fwQuery_url();
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "online_firm -i br0 -u %s -v %s", fwQuery_url, fwVersion);
	system(buf);
	//printf("command = %s\n",buf);
	sleep(3);
	
	if ((fp = fopen("/tmp/fw_check_result.txt", "r")) == NULL){            	    		
	   	printf("Read File Error!\n");
	   	strcpy(result, "ERROR");
	   	strcpy(latest_fwDate, "");
	   	strcpy(latest_fwDate, "");
	   	strcpy(fwDownload_url, ""); 
	}
   	else{
   		memset(buf, 0, sizeof(buf));
   		strtmp = NULL;
   		fgets(buf, sizeof(buf), fp);
   		
   		if ((strncmp(buf, "ERROR", 5))==0 || ((strncmp(buf, "LATEST", 6))==0)){
			//strcpy(result, "ERROR");
		}
		else{	   		
	   		//latest_ver
	   		strtmp = gettoken(buf, 0, '+');      	   			
	   		strcpy(latest_fwVerion, strtmp[0]=='0'?++strtmp:strtmp );
	   			
	   		//latest_date
		   	strtmp = gettoken(buf, 1, '+');         	   	
		   	strcpy(latest_fwDate, strtmp);
		   	
	   		//latest_firm_url
	   		strtmp = gettoken(buf, 3, '+');         	   			
	   		strcpy(fwDownload_url, strtmp);  
	   		strcpy(result, "OK");
   		}
   		fclose(fp);
   	}
   	system("rm /tmp/fw_check_result.txt");
   	
   	memset(hostName, 0, sizeof(hostName));
   	apmib_get( MIB_HOST_NAME, (void *)hostName);
   	
	sprintf(xml_resraw, get_firmware_status_result,
	 settings_result_form_head,
	 result,
	 fwVersion,
	 latest_fwVerion, 
	 latest_fwDate,
	 fwDownload_url, 
	 hostName,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

int do_StartFirmwareDownload(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char buf[256];
	char wget_image[256];
	char cat_down_percent[512];
	int rule_line=0;
	FILE *fp;
	
	//try to get new firmware here
	system("rm /tmp/download_fwImage");
	system("rm /tmp/download_percent");
	hnap_fw_download_flag = 0;	
	set_fwQuery_url();
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "online_firm -i br0 -u %s -v %s", fwQuery_url, fwVersion);

	system(buf);
	sleep(3);
	strcpy(result, "OK");
	
	if ((fp = fopen("/tmp/fw_check_result.txt", "r")) != NULL){  
		memset(buf, 0, sizeof(buf));
   		fgets(buf, sizeof(buf), fp);    
   		      	    		
		if ((strncmp(buf, "ERROR", 5))!=0 && ((strncmp(buf, "LATEST", 6))!=0))
			hnap_fw_download_flag = 1;
	}
	
	sprintf(xml_resraw, start_firmware_download_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}
	
int do_PollingFirmwareDownload(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int downloadpercentage=0;
	char string_percent[4]={0};
	char *catch_percent;
	//try to get the status(percentage) of the download process here
	
	FILE *down_file_percent= fopen("/tmp/wget_message","r");
	
	if(down_file_percent){
		while(fgets(string_percent, sizeof(string_percent), down_file_percent) != NULL){
			downloadpercentage=atoi(string_percent);
			strcpy(result, "OK");
		}
		fclose(down_file_percent);
	}else{
		downloadpercentage = 100;
		strcpy(result, "OK");
	}

	sprintf(xml_resraw, polling_firmware_download_result,
	 settings_result_form_head,
	 result,
	 downloadpercentage,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

int do_GetFirmwareValidation(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	int result=0;
	struct stat status;
	
	hnap_upgrade_flag = 0;
	
	if (stat("/tmp/download_fwImage", &status) == 0) { // file existed
        hnap_upgrade_flag = 1;
		result = 1;	
	}
	
	sprintf(xml_resraw, get_firmware_validation_result,
	 settings_result_form_head,
	 (result ? "OK":"ERROR"),
	 (result ? STR_TRUE:STR_FALSE),
	 (result ? 180:0),
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	return 0;	
}

int do_GetGroupSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char group_name[32], unique_name[32];
	
	memset(xml_resraw, 0, sizeof(xml_resraw));	
	
	if((!apmib_get( MIB_GROUP_NAME, (void *)group_name)) || (!apmib_get( MIB_UNIQUE_NAME, (void *)unique_name)))
		strcpy(result, "ERROR");
	else
		strcpy(result, "OK");
	
	sprintf(xml_resraw, get_group_settings_result,
	 settings_result_form_head,
	 result,
	 group_name,
	 unique_name,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	return 0;
}

int do_SetGroupSettings_to_xml(char *source_xml)
{
	int result=0;
	char strGroupName[33], strUniqueName[33];

	memset(strGroupName, 0, sizeof(strGroupName));
	memset(strUniqueName, 0, sizeof(strUniqueName));
	
	do_get_element_value(source_xml, strGroupName, "GroupName");
	if(strlen(strGroupName) <= 33){
		if(apmib_set(MIB_GROUP_NAME, (void *)strGroupName) == 0)
			result=1;
	}
	do_get_element_value(source_xml, strUniqueName, "UniqueName");
	if(strlen(strUniqueName) <= 33){
		if(apmib_set(MIB_UNIQUE_NAME, (void *)strUniqueName) == 0)
			result=1;
	}
	
	return result;	
}

int do_SetGroupSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char group_name[32], unique_name[32];
	memset(xml_resraw, 0, sizeof(xml_resraw));	
	int xml_result = do_SetGroupSettings_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	
	sprintf(xml_resraw, set_group_settings_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/15
int do_GetAccessControlSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char Policy[32];
	char AddressType[32];
	char IPAddress[32];
	char Method[32];
	int i = 0;
		
	char Schedule[300];
	char tmpSche[60];
	int schedultNum = 0;
	SCHEDULE_T sched;
	
	char MacAddress[360];
	char tmpAddress[18];
	int macFilterNum = 0;	
	MACFILTER_T entry;
	
	strcpy(result, "OK");
	strcpy(Policy, "none");
	strcpy(AddressType, "MacAddress");
	strcpy(IPAddress, "x.x.x.x");
	strcpy(Method, STR_TRUE);
	apmib_get(MIB_WLAN_SCHEDULE_TBL_NUM, (void *)&schedultNum);
	apmib_get(MIB_MACFILTER_TBL_NUM, (void *)&macFilterNum);

	for( i = 1; i <= schedultNum; i++){
		memset(&sched, '\0', sizeof(sched));
		*((char *)&sched) = (char)i;
		apmib_get(MIB_WLAN_SCHEDULE_TBL, (void *)&sched);
		sprintf(tmpSche,"%d,%d,%d,%d,%s & ", sched.eco, sched.fTime/60, sched.tTime/60, sched.day, sched.text);
		strcat(Schedule, tmpSche);
	}
	
	for( i = 1; i <= macFilterNum; i++){
		memset(&entry, '\0', sizeof(entry));
		*((char *)&entry) = (char)i;
		apmib_get(MIB_MACFILTER_TBL, (void *)&entry);
		sprintf(tmpAddress,"%02X:%02X:%02X:%02X:%02X:%02X & ",entry.macAddr[0],entry.macAddr[1],entry.macAddr[2],entry.macAddr[3],entry.macAddr[4],entry.macAddr[5]);
		strcat(MacAddress, tmpAddress);
	}
	
	sprintf(xml_resraw, get_access_control_settings_result,
	 settings_result_form_head,
	 result, 
	 Policy, 
	 Schedule, 
	 AddressType, 
	 MacAddress, 
	 IPAddress, 
	 Method,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;
}

//Tim Wang
//2012/06/20
int do_SetAccessControlSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetAccessControlSettings_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	else if(xml_result == 2)
		strcpy(result, "ERROR_BAD_SCHEDULE");
	else if(xml_result == 3)	
		strcpy(result, "ERROR_BAD_ADDRESSTYPE");
	else if(xml_result == 4)	
		strcpy(result, "ERROR_BAD_METHOD");
			
	sprintf(xml_resraw, set_access_control_settings_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/15
int do_SetAccessControlSettings_to_xml(char *source_xml)
{
	char Policy[16];
	char *seperator = "&";

	char Schedule[300];
	//char *tmpSche;
	int schedultNum = 0;
	//SCHEDULE_T sched;

	char MacAddress[360];
	char *tmpAddress;
	char tmpAddress2[13];
	int macFilterNum = 0;	
	MACFILTER_T entry;
	
	char AddressType[16];
	char IPAddress[16];
	char Method[16];
	int i, j, k = 0;
	int not_def = 0;
	
	apmib_get(MIB_WLAN_SCHEDULE_TBL_NUM, (void *)&schedultNum);
	apmib_get(MIB_MACFILTER_TBL_NUM, (void *)&macFilterNum);

	do_get_element_value(source_xml, Policy, "Policy");
	do_get_element_value(source_xml, Schedule, "Schedule");
	do_get_element_value(source_xml, AddressType, "AddressType");
	do_get_element_value(source_xml, MacAddress, "MacAddress");
	do_get_element_value(source_xml, IPAddress, "IPAddress");
	do_get_element_value(source_xml, Method, "Method");

	tmpAddress = strtok(MacAddress,seperator);
	for( i = 1; tmpAddress != NULL; i++){
		
		if (!strncmp(tmpAddress,"00:00:00:00:00:00", 17))
			return 1;

		for (k=0,j=0;k<17;k++){//remove ':' out of MacAddress
			if ( k==2 ||k==5 ||k==8 ||k==11 ||k==14 )
				continue;
			tmpAddress2[j] = tmpAddress[k];
			j++;
		}
		string_to_hex(tmpAddress2, entry.macAddr, 12);//have to remove ':' first
		apmib_set(MIB_MACFILTER_DEL, (void *)&entry);//to avoid set duplicate Mac Address
		apmib_set(MIB_MACFILTER_ADD, (void *)&entry);
		tmpAddress = strtok (NULL, seperator);
	}	


//	not define key in flesh yet....
//	if(!strcmp(AddressType, "IPAddress"))
//		apmib_set(, (void *)&);
//	else if(!strcmp(AddressType, "MacAddress"))
//		apmib_set(, (void *)&);
//	else if(!strcmp(AddressType, "OtherMachine"))
//		apmib_set(, (void *)&);		
//	else
//		return 3;

//	not define key in flesh yet....
//	if(!strcmp(Method, "LogWebAccessOnly"))
//		apmib_set(, (void *)&);
//	else if(!strcmp(Method, "BlockAllAccess"))
//		apmib_set(, (void *)&);
//	else if(!strcmp(Method, "BlockSomeAccess"))
//		apmib_set(, (void *)&);		
//	else
//		return 4;
	not_def = 1;
	apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);
	apmib_update(CURRENT_SETTING);
	return 0;
}

#ifdef MYDLINK_ENABLE
//Tim Wang
//2012/06/15
int do_GetMyDLinkSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int Enabled;
	char Email[BUF_LEN_128];
	char Password[BUF_LEN_128];
	char LastName[BUF_LEN_128];
	char FirstName[BUF_LEN_128];
	char dev_Password[BUF_LEN_128];
	int AccountStatus = 0;

	printf("do_GetMyDLinkSettings\n");
	
	strcpy(result, "OK");
	memset(Email, 0, sizeof(Email));
	memset(Password, 0, sizeof(Password));
	memset(LastName, 0, sizeof(LastName));
	memset(FirstName, 0, sizeof(FirstName));
	memset(dev_Password, 0, sizeof(dev_Password));

	apmib_get( MIB_MYDLINK_ENABLED, (void *)&Enabled);
	
	apmib_get( MIB_MYDLINK_EMAILACCOUNT,  (void *)Email);
	apmib_get( MIB_MYDLINK_ACCOUNTPASSWORD,  (void *)Password);
	apmib_get( MIB_MYDLINK_FIRSTNAME,  (void *)FirstName);
	apmib_get( MIB_MYDLINK_LASTNAME,  (void *)LastName);
	apmib_get( MIB_USER_PASSWORD,  (void *)dev_Password);
	
	apmib_get(MIB_MYDLINK_REG_STAT, (void *)&AccountStatus);
		
	sprintf(xml_resraw, get_mydlink_settings_result,
	 settings_result_form_head,
	 result, 
	 (Enabled ? STR_TRUE:STR_FALSE), 
	 	Email, 
	 	Password, 
	 	LastName, 
	 	FirstName, 
	 	dev_Password, 
	 	(AccountStatus ? STR_TRUE:STR_FALSE),
	 	settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	return 0;

}

//Tim Wang
//2012/06/20
int do_SetMyDLinkSettings(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetMyDLinkSettings_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "OK");
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	
	sprintf(xml_resraw, set_mydlink_settings_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

//Tim Wang
//2012/06/15
int do_SetMyDLinkSettings_to_xml(char *source_xml)
{
	int result=0;
	int mydlink_enabled=0;
	int account_status=0;
	char Enabled[8];
	char AccountStatus[8];
	char Email[BUF_LEN_128];
	char Password[BUF_LEN_128];
	char LastName[BUF_LEN_128];
	char FirstName[BUF_LEN_128];
	char DeviceUserName[BUF_LEN_128];
	char DevicePassword[BUF_LEN_128];
	char ret_content[256];
	
	printf("[%s]\n", __FUNCTION__);	
	
	memset(Enabled, 0, sizeof(Enabled));
	memset(AccountStatus, 0, sizeof(AccountStatus));
	memset(Email, 0, sizeof(Email));
	memset(Password, 0, sizeof(Password));
	memset(LastName, 0, sizeof(LastName));
	memset(FirstName, 0, sizeof(FirstName));
	memset(DevicePassword, 0, sizeof(DevicePassword));
	
	do_get_element_value(source_xml, Enabled, "Enabled");
	do_get_element_value(source_xml, Email, "Email");
	do_get_element_value(source_xml, Password, "Password");
	do_get_element_value(source_xml, FirstName, "FirstName");
	do_get_element_value(source_xml, LastName, "LastName");
	do_get_element_value(source_xml, DeviceUserName, "DeviceUserName");
	do_get_element_value(source_xml, DevicePassword, "DevicePassword");
	do_get_element_value(source_xml, AccountStatus, "AccountStatus");
	
	result = 0;
	
	if(strcmp(Enabled, "false") == 0)
		mydlink_enabled = 0;
	else
		mydlink_enabled = 1;
		
	apmib_set( MIB_MYDLINK_ENABLED, (void *)&mydlink_enabled);
	
	if(strlen(Password) != 0)
		apmib_set( MIB_MYDLINK_ACCOUNTPASSWORD,  (void *)Password);
	
	if(strlen(DevicePassword) != 0)
		apmib_set( MIB_USER_PASSWORD,  (void *)DevicePassword);
	
	apmib_set( MIB_MYDLINK_EMAILACCOUNT,  (void *)Email);
	apmib_set( MIB_MYDLINK_FIRSTNAME,  (void *)FirstName);
	apmib_set( MIB_MYDLINK_LASTNAME,  (void *)LastName);
	
	//account_status = 1;
	//apmib_set( MIB_MYDLINK_REG_STAT, (void *)&account_status);
	
	apmib_update(CURRENT_SETTING);
	
	if(strcmp(AccountStatus, "false") == 0){
		printf("DO mydlink signup\n");
		
		if (mydlink_signup(ret_content) == 1){
			printf("mydlink signup failed\n");
			result = 1;
		}
		if (mydlink_signin(ret_content) == 1){
			printf("mydlink signin failed\n");
			result = 1;
		}
		if (mydlink_addev(ret_content) == 1){
			printf("mydlink addev failed\n");
			result = 1;
		}
	}
	else{
		printf("DO mydlink signin\n");
		
		if (mydlink_signin(ret_content) == 1){
			printf("mydlink signin failed\n");
			result = 1;
		}
		if (mydlink_addev(ret_content) == 1){
			printf("mydlink addev failed\n");
			result = 1;
		}
	}
		
	return result;	
}

int do_SetMyDLinkunregistration(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char unregistration[8]={0};
	char result[8]={0};
	char xml_resraw[2048];
	int i;
	int register_st;
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	
	printf("[%s]\n", __FUNCTION__);	
	
	do_get_element_value(raw, unregistration, "Unregistration");

	for (i = 0;i < sizeof(unregistration);i++)
		unregistration [i] = tolower(unregistration[i]);
	
	printf("unregistration=%s\n",unregistration);

	if (strcmp(unregistration, "true") == 0){
		register_st = 0;
		strcpy(result,"OK");
	}
	else if(strcmp(unregistration, "false") == 0){
		register_st = 1;
		strcpy(result,"OK");
	}
	else
		strcpy(result,"ERROR");
		
	apmib_set(MIB_MYDLINK_REG_STAT, (void *)&register_st);
	apmib_update(CURRENT_SETTING);	
	
	sprintf(xml_resraw, set_unregistration_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}
#endif

int do_GetInterfaceStatistics(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char intface[16];
	struct user_net_device_stats stats;
	
	printf("[%s]\n", __FUNCTION__);
	
	strcpy(result, "OK");
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(intface, 0, sizeof(intface));
	
	// LAN, WAN, WLAN2.4G, WLAN5G
	do_get_element_value(raw, intface, "Interface");

	memset(&stats, 0x0, sizeof(stats));
	
	if (strncmp(intface, "LAN", 3) == 0)
		getStats(BRIDGE_IF, &stats);
	else if (strncmp(intface, "WAN", 3) == 0)
		strcpy(result, "ERROR");
	else if (strncmp(intface, "WLAN2.4G", 8) == 0)
		getStats(WLAN_IF, &stats);
	else if (strncmp(intface, "WLAN5G", 6) == 0)
		strcpy(result, "ERROR");
	else	
		strcpy(result, "ERROR");
	
	
	sprintf(xml_resraw, get_interfacestatistics_result,
	 result, 
	 intface, 
	 /*
	 stats.rx_packets,
	 stats.tx_bytes,
	 stats.rx_dropped,
	 stats.rx_bytes,
	 stats.tx_dropped,
	 stats.tx_packets,
	 stats.tx_errors,
*/
	 stats.tx_bytes, 
	 stats.rx_bytes,
	 stats.tx_packets, 
	 stats.rx_packets, 
	 stats.tx_dropped, 
	 stats.rx_dropped, 
	 stats.tx_errors);
	/*
	stats.rx_bytes, stats.rx_packets, 
	stats.rx_dropped, stats.tx_packets, 
	stats.tx_dropped, stats.tx_bytes, 
	stats.tx_errors);	
	*/
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	return 0;
}

int do_GetClientInfo(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int i, interface;
	WLAN_STA_INFO_Tp pInfo;
	char *buff;
	char mac[24];
	char buff_tmp[256];
	char wifi_type[16];
	
	printf("[%s]\n", __FUNCTION__);
	
	strcpy(result, "OK");	
	memset(xml_resraw, 0, sizeof(xml_resraw));	
	memset(buff_tmp, 0, sizeof(buff_tmp));	

	sprintf(xml_resraw, get_getclientinfo_result_head,
	 settings_result_form_head,
	 result);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	buff = calloc(1, sizeof(WLAN_STA_INFO_T) * (MAX_STA_NUM+1));
	if ( buff == 0 ) {
		printf("Allocate buffer failed!\n");
		strcpy(result, "ERROR");
	}
	for(interface=0; interface<2;interface++){
		sprintf(WLAN_IF, "wlan%d", interface);
 		
		if ( getWlStaInfo(WLAN_IF,  (WLAN_STA_INFO_Tp)buff ) < 0 ) {
			printf("Read wlan sta info failed!\n");
			strcpy(result, "ERROR");
		}
		
		if(interface == 0)
			strcpy(wifi_type, "5G");
		else
			strcpy(wifi_type, "2.4G");
		
		for (i=1; i<=MAX_STA_NUM; i++) {
			pInfo = (WLAN_STA_INFO_Tp)&buff[i*sizeof(WLAN_STA_INFO_T)];
			if (pInfo->aid && (pInfo->flag & STA_INFO_FLAG_ASOC)) {
				sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", pInfo->addr[0],pInfo->addr[1],pInfo->addr[2],pInfo->addr[3],pInfo->addr[4],pInfo->addr[5]);
				sprintf(buff_tmp, get_getclientinfo_struct_str, to_upper(mac), wifi_type);
				req_format_write(req,buff_tmp);
				memset(buff_tmp, 0, sizeof(buff_tmp));	
			}
		}
	}
	free(buff);
	req_format_write(req,get_getclientinfo_result_foot);
	
	return 0;
}

int do_GetDirectServer(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char error[8];
	char serverip[16];
	char port[8];
	FILE *fp;
	
	printf("[%s]\n", __FUNCTION__);
	
	memset(xml_resraw, 0, sizeof(xml_resraw));	
	memset(error, 0, sizeof(error));	
	memset(serverip, 0, sizeof(serverip));	
	memset(port, 0, sizeof(port));	
	
	fp=fopen("/tmp/upnpc_result","r");
	if (fp==NULL)
		strcpy(result, "fail");	 
	else
	{
		fgets(result, sizeof(result), fp); 
		fgets(serverip, sizeof(serverip), fp);
		fgets(port, sizeof(port), fp);
		fclose(fp);
		
		if (strlen(serverip) < 4 )
		{
			strcpy(result, "fail");	
		}
		else
		{
			result[strlen(result)-1]=0x0;
			serverip[strlen(serverip)-1]=0x0;
		}
		port[strlen(port)-1]=0x0;
	}
	
	sprintf(xml_resraw, get_directserver_result,
	 settings_result_form_head,
	 result, 
	 error, 
	 serverip, 
	 port,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
}

int do_GetMACFilters2(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int i, item;
	char mac[32];
	char buff_tmp[256];
	int filterMode = 0;
	int macFilterNum = 0;
	MACFILTER_T entry;
	
	printf("[%s]\n", __FUNCTION__);
	
	strcpy(result, "OK");	
	memset(xml_resraw, 0, sizeof(xml_resraw));	
	
	apmib_get(MIB_MACFILTER_ENABLED,  (void *)&filterMode); //0:disable; 1:allow; 2:deny
	apmib_get(MIB_MACFILTER_TBL_NUM, (void *)&macFilterNum);
	
	sprintf(xml_resraw, get_macfilters2_result_head,
	 settings_result_form_head,
	 "OK", 
	 (filterMode==0)?"false":"true",
	 (filterMode==1)?"true":"false");
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
			
	for( i = 1; i <= macFilterNum; i++)
	{
		item = i;
		*((char *)&entry) = (char)item;
		if(!apmib_get(MIB_MACFILTER_TBL, (void *)&entry))
				continue;
		else
		{
			if(strcmp(entry.comment,"1") == 0) 
			{
				memset(buff_tmp, 0, sizeof(buff_tmp));	
				sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", 
				entry.macAddr[0], entry.macAddr[1], entry.macAddr[2],
				entry.macAddr[3], entry.macAddr[4], entry.macAddr[5]);
				sprintf(buff_tmp, mac_info, to_upper(mac));
				req_format_write(req,buff_tmp);	
			}
		}	
	}
	req_format_write(req,get_macfilters2_result_foot);

  return 0;
}

int do_SetMACFilters2_to_xml(char *source_xml)
{
	int filterMode; //0:disable; 1:allow; 2:deny
	char Enabled_str[BUF_LEN_16];
	char IsAllowList[BUF_LEN_16];
	int not_def = 0;
	MACFILTER_T macEntry;
	int i,j;
	char macStr_tmp[32];
	char macStr[32];
	int result = 0;
	int macFilterNum = 0;
	
	printf("[%s]Line:%d\n", __FUNCTION__, __LINE__);
	
	memset(Enabled_str, 0, sizeof(Enabled_str));
	memset(IsAllowList, 0, sizeof(IsAllowList));
	
	do_get_element_value(source_xml, Enabled_str, "Enabled");
	do_get_element_value(source_xml, IsAllowList, "IsAllowList");
	
	if (strcmp(Enabled_str, "false") == 0)
		filterMode = 0;
	else
	{
		if (strcmp(IsAllowList, "true") == 0)
			filterMode = 1;
		else
			filterMode = 2;
	}
	apmib_set(MIB_MACFILTER_ENABLED, (void *)&filterMode);
	
	apmib_get(MIB_MACFILTER_TBL_NUM, (void *)&macFilterNum);
	
	memset(&macEntry, '\0', sizeof(macEntry));
	apmib_set(MIB_MACFILTER_DELALL ,(void *)&macEntry);
	
	for(macFilterNum; macFilterNum<20; macFilterNum++)
	{
		if ((source_xml = find_next_element(source_xml, "MACInfo")) != NULL) 
		{	
			memset(&macEntry, '\0', sizeof(macEntry));
			memset(macStr, '\0', sizeof(macStr));		
			do_get_element_value(source_xml, macStr_tmp, "MacAddress");	
			for(i=0,j=0;i<17;i++)
			{
				if( i==2 ||i==5 ||i==8 ||i==11 ||i==14 )
					continue;
				macStr[j] = macStr_tmp[i];
				j++;
			}
			string_to_hex(macStr, macEntry.macAddr, 12);
			macEntry.comment[0] = '1';
			//or MIB_WLAN_AC_ADDR_DEL?
		//	apmib_set(MIB_MACFILTER_DEL, (void *)&macEntry);			
			if ( apmib_set(MIB_MACFILTER_ADD, (void *)&macEntry) == 0) 
				result = 1;
		}
		else
			break;
	}

	not_def = 1;
	apmib_set( MIB_RESERVED_WORD_4, (void *)&not_def);
	apmib_update(CURRENT_SETTING);
	return result;
}

int do_SetMACFilters2(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	int xml_result = do_SetMACFilters2_to_xml(raw);
		
	if(xml_result == 0)
		strcpy(result, "REBOOT");//or OK
	else if(xml_result == 1)
		strcpy(result, "ERROR");
	
	sprintf(xml_resraw, set_macfilters2_result, 
		settings_result_form_head,
		result,
		settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	
	if(xml_result == 0)	
		system("sysconf init gw all &");
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

int do_SetMultipleActions(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char tag[32];
	char xml[2048];
	char remain_raw[2048];
	char next_raw[2048];
	int i = 0;
	int next_len = 0;
	int SetDeviceSettings_flag = 0;
	int SetWLanRadioSettings_flag = 0;
	int SetWLanRadioSecurity_flag = 0;
	
	strcpy(result, "OK");

	memset(xml, 0, sizeof(xml));
	memset(remain_raw, 0, sizeof(remain_raw));
	memset(next_raw, 0, sizeof(next_raw));
	memset(xml_resraw, 0, sizeof(xml_resraw));
	
	parse_multi_xml(raw, xml, remain_raw, "SetDeviceSettings");
	SetDeviceSettings_flag = do_SetDeviceSettings_to_xml(xml);
	
	parse_multi_xml(raw, xml, remain_raw, "SetWLanRadioSettings");
	SetWLanRadioSettings_flag = do_SetWLanRadioSettings_to_xml(xml);
	strcpy(next_raw, remain_raw);
	parse_multi_xml(next_raw, xml, remain_raw, "SetWLanRadioSettings");
	SetWLanRadioSettings_flag = do_SetWLanRadioSettings_to_xml(xml);
	
	parse_multi_xml(raw, xml, remain_raw, "SetWLanRadioSecurity");
	SetWLanRadioSecurity_flag = do_SetWLanRadioSecurity_to_xml(xml);
	strcpy(next_raw, remain_raw);
	parse_multi_xml(next_raw, xml, remain_raw, "SetWLanRadioSecurity");
	SetWLanRadioSecurity_flag = do_SetWLanRadioSecurity_to_xml(xml);
	
	apmib_update_web(CURRENT_SETTING);
	
	sprintf(xml_resraw, set_multipleactions_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);

	system("reboot");
}

int do_GetCurrentInternetStatus(const char *key, char *raw, HNAP_SESSION_T session_type , request *req)
{
	char xml_resraw[2048];
	char result[32];
	char InternetStatus[6];
	
	memset(xml_resraw, 0, sizeof(xml_resraw));
	memset(InternetStatus, 0, sizeof(InternetStatus));	
	
	do_get_element_value(raw, InternetStatus, "InternetStatus");
	
	if(strcasecmp(InternetStatus, "true") == 0)
		strcpy(result, "OK_DETECTING_1");
	else if(strcasecmp(InternetStatus, "false") == 0){
		strcpy(result, "OK_NOTCONNECTED");
	}
			
	sprintf(xml_resraw, get_current_internet_status_result,
	 settings_result_form_head,
	 result,
	 settings_result_form_foot);
	send_headers( req,  200, "Ok", NULL, "text/xml" );
	req_format_write(req,xml_resraw);
	return 0;
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
		{ "SetMultipleActions",				NULL,	do_SetMultipleActions },
		{ "GetDeviceSettings",				NULL,	do_GetDeviceSettings },
		{ "SetDeviceSettings",				NULL,	do_SetDeviceSettings },
		{ "GetDeviceSettings2",				NULL,	do_GetDeviceSettings2 },
		{ "SetDeviceSettings2",				NULL,	do_SetDeviceSettings2 },
		{ "IsDeviceReady",					NULL,	do_IsDeviceReady },
		{ "Reboot",							NULL,	do_Reboot },
		{ "GetFactoryDefaults",				NULL,	do_GetFactoryDefaults },
		{ "SetFactoryDefault",				NULL,	do_SetFactoryDefault },
		{ "GetFirmwareSettings",			NULL,	do_GetFirmwareSettings },
		{ "GetWLanRadioSettings",			NULL,	do_GetWLanRadioSettings },
		{ "SetWLanRadioSettings",			NULL,	do_SetWLanRadioSettings },
		{ "GetWLanRadioSecurity",			NULL,	do_GetWLanRadioSecurity },
		{ "SetWLanRadioSecurity",			NULL,	do_SetWLanRadioSecurity },
		{ "GetWiFiOpMode",					NULL,	do_GetWiFiOpMode },
		{ "SetWiFiOpMode",					NULL,	do_SetWiFiOpMode },
		{ "GetUserLimit",					NULL,	do_GetUserLimit },
		{ "SetUserLimit",					NULL, 	do_SetUserLimit },
		{ "GetFirmwareStatus",				NULL,	do_GetFirmwareStatus },
		{ "StartFirmwareDownload",			NULL,	do_StartFirmwareDownload },
		{ "PollingFirmwareDownload",		NULL,	do_PollingFirmwareDownload },
		{ "GetFirmwareValidation",			NULL,	do_GetFirmwareValidation },
		//{ "DoFirmwareUpgrade", 			NULL,	do_DoFirmwareUpgrade },
		{ "GetAccessControlSettings",		NULL,	do_GetAccessControlSettings },
		{ "SetAccessControlSettings", 		NULL,	do_SetAccessControlSettings },
#ifdef MYDLINK_ENABLE
		{ "GetMyDLinkSettings",				NULL,	do_GetMyDLinkSettings },
		{ "SetMyDLinkSettings",				NULL,	do_SetMyDLinkSettings },
		{ "SetMyDLinkunregistration",		NULL,	do_SetMyDLinkunregistration },
#endif
		{ "GetInterfaceStatistics",			NULL,	do_GetInterfaceStatistics },	
		{ "GetClientInfo",					NULL,	do_GetClientInfo },
		{ "GetDirectServer",				NULL,	do_GetDirectServer },
		{ "GetGroupSettings",				NULL,	do_GetGroupSettings },
		{ "SetGroupSettings",				NULL,	do_SetGroupSettings },
		{ "GetMACFilters2",					NULL,	do_GetMACFilters2 },
		{ "SetMACFilters2",					NULL,	do_SetMACFilters2 },
		{ "GetWLanRadios",					NULL,	do_GetWLanRadios },	
		{ "GetOperationMode",				NULL,	do_GetOperationMode },
		{ "SetTriggerWirelessSiteSurvey",	NULL,	do_SetTriggerWirelessSiteSurvey },	
		{ "GetCurrentInternetStatus",		NULL,	do_GetCurrentInternetStatus },
		{ "GetSiteSurvey",					NULL,	do_GetSiteSurvey },
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