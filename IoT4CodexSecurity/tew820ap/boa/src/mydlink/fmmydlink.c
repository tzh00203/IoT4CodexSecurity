/*
 *      Web server handler routines for management (password, save config, f/w update)
 *
 *      Authors: David Hsu	<davidhsu@realtek.com.tw>
 *
 *      $Id: fmmgmt.c,v 1.45 2009/09/03 05:04:42 keith_huang Exp $
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>

#include "boa.h"
#include "globals.h"
#include "apmib.h"
#include "apform.h"
#include "utility.h"
#include "mibtbl.h"
#include "asp_page.h"

#define RET_ERR 1
#define RET_OK	0

unsigned char *mydlink_ret_str_ok="ok";
unsigned char *mydlink_ret_str_fail="fail";

#define SERVER_NAME "CAMEO-httpd"
//#define SERVER_PORT 80
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"


#define _PATH_DHCPS_LEASES	T("/var/lib/misc/udhcpd.leases")


static int getOneDhcpClient(char **ppStart, unsigned long *size, char *name, char *ip, char *mac, char *liveTime);
static int get_current_ip(webs_t wp);

extern void send_headers( request *wp, int status, char* title, char* extra_header, char* mime_type );
extern char *to_upper(char *str);

char no_cache[] =
"Expires: Mon, 01 Jul 1980 00:00:00 GMT\r\n"
"Cache-Control: no-cache, no-store, must-revalidate\r\n"
"Pragma: no-cache"
;

void form_network2(request *wp, char *path, char *query) // only for test
{
  //fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
}

void form_network(request *wp, char *path, char *query)
{
  
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	char *settingsChanged, *lan_network_address, *dhcp_server_enabled;
	struct in_addr inIp, inMask, inDHCP_start, inDHCP_end, inDHCP_gw;
	unsigned long ip_t, mask_t, start_t, end_t, dhcpgw_t;
	DHCP_T dhcp;
	char tmpBuf[250], str_ip[20];

	settingsChanged = websGetVar(wp, "settingsChanged", "");
	lan_network_address = req_get_cstream_var(wp, "config.lan_network_address", "");
	dhcp_server_enabled = req_get_cstream_var(wp, "config.dhcp_server_enabled", "");	//1=enable 0=disable
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s config.lan_network_address=%s config.dhcp_server_enabledt=%s\n", settingsChanged, lan_network_address, dhcp_server_enabled);
	
	if(settingsChanged[0])
	{
	  
		if(strcmp(settingsChanged, "1"))
		{
			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
			goto setErr_config;
		}

		fprintf(stderr,"### settingsChanged ### \n");
		
		if(lan_network_address[0])
		{
		  fprintf(stderr,"### lan_network_address ### \n");
			if(!inet_aton(lan_network_address, &inIp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(!apmib_set( MIB_IP_ADDR, (void *)&inIp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		
			dhcp = DHCP_DISABLED;
			if(!apmib_set(MIB_DHCP, (void *)&dhcp))
			{
	  			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		//debugging
		if(dhcp_server_enabled[0])
		{
			if(atoi(dhcp_server_enabled) < 0 || atoi(dhcp_server_enabled) > 1)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(atoi(dhcp_server_enabled) == 1)
			{
				apmib_get(MIB_IP_ADDR, (void *)tmpBuf);
				strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
				inet_aton(str_ip, &inIp);
				
				apmib_get(MIB_SUBNET_MASK, (void *)tmpBuf);
				strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
				inet_aton(str_ip, &inMask);
				
				apmib_get(MIB_DHCP_CLIENT_START, (void *)tmpBuf);
				strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
				inet_aton(str_ip, &inDHCP_start);
				
				apmib_get(MIB_DHCP_CLIENT_END, (void *)tmpBuf);
				strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
				inet_aton(str_ip, &inDHCP_end);
				
				apmib_get(MIB_RESERVED_STR_0, (void *)tmpBuf);
				strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
				inet_aton(str_ip, &inDHCP_gw);
				
				ip_t = *((unsigned long *)&inIp);
				mask_t = *((unsigned long *)&inMask);
				start_t = *((unsigned long *)&inDHCP_start);
				end_t = *((unsigned long *)&inDHCP_end);
				dhcpgw_t = *((unsigned long *)&inDHCP_gw);
				
				if((ip_t&mask_t) != (start_t&mask_t))
				{
					start_t = (start_t & (~mask_t)) | (ip_t & mask_t);
					end_t = (end_t & (~mask_t)) | (ip_t & mask_t);
					dhcpgw_t = (dhcpgw_t & (~mask_t)) | (ip_t & mask_t);
					
					inDHCP_start = *((struct in_addr *)&start_t);
					inDHCP_end = *((struct in_addr *)&end_t);
					inDHCP_gw = *((struct in_addr *)&dhcpgw_t);
										
					apmib_set(MIB_DHCP_CLIENT_START, (void *)&inDHCP_start);
					apmib_set(MIB_DHCP_CLIENT_END, (void *)&inDHCP_end);
					apmib_set(MIB_RESERVED_STR_0,  (void *)&inDHCP_gw);
					apmib_set(MIB_RESERVED_STR_1, (void *)&inDHCP_gw);
					apmib_set(MIB_RESERVED_STR_2, (void *)&inDHCP_gw);
				}
				dhcp = DHCP_SERVER;
				if(!apmib_set(MIB_DHCP, (void *)&dhcp))
				{
		  			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
			else
			{
				dhcp = DHCP_DISABLED;
				if(!apmib_set(MIB_DHCP, (void *)&dhcp))
				{
		  			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
		}
		
		apmib_update(CURRENT_SETTING);
		OK_MSG_TEXT_PLAIN_MYDLINK(mydlink_ret_str_ok);
		return;
	}
	
setErr_config:
	OK_MSG_TEXT_PLAIN_MYDLINK(mydlink_ret_str_fail);
}

//MYDLINK AUTH///////////////////
void form_login(request *wp, char *path, char *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	char_t *strUser, *strPassword;
	char tmpBuf[100];
	
	strUser = req_get_cstream_var(wp, ("login_n"), (""));
	strPassword = req_get_cstream_var(wp, ("login_pass"), (""));
	
	fprintf(stderr,"###MYDLINK DEBUG### login_n=%s login_pass=%s\n", strUser, strPassword);
	
	if(strUser[0])
	{
		if(strcasecmp(strUser, "admin"))
			goto setErr_pass;
	}
	else
		goto setErr_pass;
		
	apmib_get(MIB_USER_PASSWORD, (void *)tmpBuf);
	if(strcmp(strPassword, tmpBuf))
		goto setErr_pass;
	
	check_login(wp->client_ip);
	req_login_p->auth_login = 1;
	//if(sp_login_p != NULL)
	//	sp_login_p->auth_if=1;
	
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
	return;
setErr_pass:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}

void form_logout(request *wp, char *path, char *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	req_login_p->auth_login = 0;
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
	return;
setErr_pass:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}
///////////////////////////////////


static int pure_replace_special_char(char *str){
	char replace_str[256];
	int i, len, find, count;
	
	len = strlen(str);
	memset(replace_str, 0, sizeof(replace_str));
	
	find = 0;
	count = 0;

	for (i = 0; i < len; i++){	
		if (str[i] != '&'){
			replace_str[count++] = str[i];		
		}else{
			if (str[i+1] == 'a' && str[i+2] == 'm' && str[i+3] == 'p'
					&& str[i+4] == ';'){	// replace '&'
				replace_str[count++] = str[i];
				i += 4;			
				find = 1;				
			}else if (str[i+1] == 'q' && str[i+2] == 'u' && str[i+3] == 'o'
					&& str[i+4] == 't' && str[i+5] == ';'){	// replace '"'
				replace_str[count++] = '"';
				i += 5;				
				find = 1;
			}else if (str[i+1] == 'a' && str[i+2] == 'p' && str[i+3] == 'o'
					&& str[i+4] == 's' && str[i+5] == ';'){	// replace '''
				strcpy(&replace_str[count++], "'");
				i += 5;				
				find = 1;
			}else if (str[i+1] == 'l' && str[i+2] == 't' && str[i+3] == ';'){	// replace '<'
				replace_str[count++] = '<';
				i += 3;				
				find = 1;
			}else if (str[i+1] == 'g' && str[i+2] == 't' && str[i+3] == ';'){	// replace '>'
				replace_str[count++] = '>';
				i += 3;				
				find = 1;
			}
		}
	}

	replace_str[count]='\0';
	
	if (find){
		memset(str, 0, len);		
		memcpy(str, replace_str, count);
	}
	
	return find;
}


#define SPECIAL_CHAR_LEN 6
char xml_char[] = {'&', '<', '>', '"', '\'', '%'};

void encode_special_char(char *str){
	char *replace_str;	
	int i, j, len, find, count;
	
	len = strlen(str);
	replace_str = malloc(sizeof(char) * (len * 6));
	
	memset(replace_str, 0, sizeof(char) * (len * 6));
	count = 0;
	
	for (i = 0; i < len; i++){	
		find = 0;
		
		for (j = 0; j < SPECIAL_CHAR_LEN; j++){
			if (str[i] == xml_char[j]){	// find special characters
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
						replace_str[count++] = '#';
						replace_str[count++] = '3';
						replace_str[count++] = '9';
						replace_str[count++] = ';';
						/*
						replace_str[count++] = 'a';
						replace_str[count++] = 'p';
						replace_str[count++] = 'o';
						replace_str[count++] = 's';
						replace_str[count++] = ';';
						*/
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

void get_wlan_conn_info(webs_t wp, char *file_str, int flag)
{}



static int getOneDhcpClient(char **ppStart, unsigned long *size, char *name, char *ip, char *mac, char *liveTime)
{}

static int get_current_ip(webs_t wp)
{}

//MYDLINK CONFIG///////////////////
void form_apply(webs_t wp, char_t *path, char_t *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	int opmode=-1;
	char *arg;
	char_t *settingsChanged, *sta_type, *sta_reboot;
	
	settingsChanged = websGetVar(wp, T("settingsChanged"), T(""));
	sta_type = websGetVar(wp, T("Sta_type"), T(""));	//1=apply
	sta_reboot = websGetVar(wp, T("Sta_reboot"), T(""));	//1=reboot
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s Sta_type=%s Sta_reboot=%s\n", settingsChanged, sta_type, sta_reboot);
	if(settingsChanged[0])
	{
		if(strcmp(settingsChanged, "1"))
		{
			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
			goto setErr_config;
		}

		if(sta_type[0])
		{
			if(strcmp(sta_type, "1") == 0)
			{
				isNeedReboot = SAVE_NO_REBOOT;//1
				apmib_update(CURRENT_SETTING);
				OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
				return;
			}
		}
		
		if(sta_reboot[0])
		{
			if(strcmp(sta_reboot, "1") == 0)
			{
				apmib_get( MIB_OP_MODE, (void *)&opmode);
				if(opmode == WISP_MODE || opmode == GATEWAY_MODE)
					arg = "all";
				else 
					arg = "bridge";
				
				#ifdef REBOOT_CHECK
	        run_init_script_flag = 1;
        #endif
        isREBOOTASP = 2; // do select.c ToReInit
				isNeedReboot = INIT_REBOOT;//0	
				//run_init_script(arg);
				OK_MSG_TEXT_PLAIN("");
				return;
			}

		}
		OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
		return;
	}
setErr_config:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);	
}

void form_wireless(webs_t wp, char_t *path, char_t *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	int disabled, auto_chan, encrypt, cipherType, wepType, wepKeyLen=0, wepFomrat=0, wep_ifindex=0, radiusPort, chan;
	char_t *settingsChanged, *f_enable, *f_channel, *f_auto_channel, *f_ap_hidden, *f_ssid, *f_authentication, *f_wep_auth_type, *strVal;
	char_t *f_cipher, *f_wep_len, *f_wep_format, *f_wep_def_key, *f_wep, *f_wpa_psk, *f_wpa_psk_type, *f_radius_ip1, *f_radius_port1, *f_radius_secret1;
	char key[30];
	struct in_addr inIp;
	int intVal;
	
	settingsChanged = websGetVar(wp, T("settingsChanged"), T(""));
	f_enable = websGetVar(wp, T("f_enable"), T(""));	//1=enable 0=disable
	f_ssid = websGetVar(wp, T("f_ssid"), T(""));
	f_channel = websGetVar(wp, T("f_channel"), T(""));	//1-11 for DAP1360L
	f_auto_channel = websGetVar(wp, T("f_auto_channel"), T(""));	//1=on 0=off
	f_ap_hidden = websGetVar(wp, T("f_ap_hidden"), T(""));	//0=visible 1=invisible
	f_authentication = websGetVar(wp, T("f_authentication"), T(""));	//0=none 1=wep 2=wpa 4=wpa2 6=wpa/wpa2 
	f_wep_auth_type = websGetVar(wp, T("f_wep_auth_type"), T(""));	//1=open 2=share
	f_cipher = websGetVar(wp, T("f_cipher"), T(""));	//1=tkip 2=aes 3=auto
	f_wep_len = websGetVar(wp, T("f_wep_len"), T(""));	//0=64bit 1=128bit
	f_wep_format = websGetVar(wp, T("f_wep_format"), T(""));	//1=ascii 2=hex
	f_wep_def_key = websGetVar(wp, T("f_wep_def_key"), T(""));	//1=wep1 2=wep2 3=wep3 4=wep4 DAP1360L only use wep1
	f_wep = websGetVar(wp, T("f_wep"), T(""));	//wep always write to wep1 for DAP1360L
	f_wpa_psk = websGetVar(wp, T("f_wpa_psk"), T(""));
	f_wpa_psk_type = websGetVar(wp, T("f_wpa_psk_type"), T(""));	//1=eap 2=psk
	f_radius_ip1 = websGetVar(wp, T("f_radius_ip1"), T(""));
	f_radius_port1 = websGetVar(wp, T("f_radius_port1"), T(""));
	f_radius_secret1 = websGetVar(wp, T("f_radius_secret1"), T(""));
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s f_enable=%s f_channel=%s f_auto_channel=%s f_ap_hidden=%s f_ssid=%s f_authentication=%s f_wep_auth_type=%s\n",
	 settingsChanged, f_enable, f_channel, f_auto_channel, f_ap_hidden, f_ssid, f_authentication, f_wep_auth_type);
	fprintf(stderr,"###MYDLINK DEBUG### f_cipher=%s f_wep_len=%s f_wep_format=%s f_wep_def_key=%s f_wep=%s f_wpa_psk=%s f_wpa_psk_type=%s f_radius_ip1=%s f_radius_port1=%s f_radius_secret1=%s\n",
	 f_cipher, f_wep_len, f_wep_format, f_wep_def_key, f_wep, f_wpa_psk, f_wpa_psk_type, f_radius_ip1, f_radius_port1, f_radius_secret1);
	
	if(settingsChanged[0])
	{
		if(strcmp(settingsChanged, "1"))
		{
			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
			goto setErr_config;
		}

		if(f_enable[0])
		{
			if(strcmp(f_enable, "1")==0)
				disabled = 0;
			else if(strcmp(f_enable, "0")==0)
				disabled = 1;
			else
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			if(apmib_set(MIB_WLAN_WLAN_DISABLED, (void *)&disabled) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(f_ssid[0])
		{
			if(apmib_set(MIB_WLAN_SSID, (void *)f_ssid) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		else
			pure_replace_special_char(f_ssid);
		
		if(f_channel[0])
		{
		  if (wlan_idx == 1) // 2.4g channel
		  {
  			if(atoi(f_channel)>=1 && atoi(f_channel)<=11) //DAP1360L 1-11
  			{
  				//int errno = 0; // joe mark, could not used errno.
  				chan = strtol(f_channel, (char **)NULL, 10);
  				//if(errno)
  				//{
  				//	fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
  				//	goto setErr_config;
  				//}
  				//if(apmib_set(MIB_WLAN_TMP_CHAN, (void *)&chan) == 0)  // joe mark, 1665 no tmp_chan MIB.
  				//{
  				//	fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
  				//	goto setErr_config;
  				//}
  				if(apmib_set(MIB_WLAN_CHANNEL, (void *)&chan) == 0)
  				{
  					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
  					goto setErr_config;				
  				}
  			}
		  }
		  else if (wlan_idx == 0)
		  {
		    if(atoi(f_channel)>=36 && atoi(f_channel)<=165) //DAP1360L 1-11
  			{
  				chan = strtol(f_channel, (char **)NULL, 10);

  				if(apmib_set(MIB_WLAN_CHANNEL, (void *)&chan) == 0)
  				{
  					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
  					goto setErr_config;				
  				}
  			}
		  }
		}
		
		if(f_auto_channel[0])
		{
			if(strcmp(f_auto_channel, "1")==0)
			{
				auto_chan = 0;
			  if(apmib_set(MIB_WLAN_CHANNEL, (void *)&auto_chan) == 0)
			  {
				  fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				  goto setErr_config;
			  }
			}
			else if(strcmp(f_auto_channel, "0")==0)
			{
				//apmib_get(MIB_WLAN_TMP_CHAN, (void *)&auto_chan); // joe mark, 1665 no tmp_chan MIB.
			}
			else
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}

		}
		
		if(f_ap_hidden[0])
		{
			if(strcmp(f_ap_hidden, "1")==0 || strcmp(f_ap_hidden, "0")==0)
			{
				int ap_hidden = atoi(f_ap_hidden);
				if(apmib_set(MIB_WLAN_HIDDEN_SSID, (void *)&ap_hidden) == 0)
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
		}
		
		if(f_authentication[0])
		{
			if(strcmp(f_authentication, "0")==0)
				encrypt = ENCRYPT_DISABLED;
			else if(strcmp(f_authentication, "1")==0)
			{	
				encrypt = ENCRYPT_WEP;
				intVal = 0;
				apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);
			}
			else if(strcmp(f_authentication, "2")==0)
				encrypt = ENCRYPT_WPA;
			else if(strcmp(f_authentication, "4")==0)
				encrypt = ENCRYPT_WPA2;
			else if(strcmp(f_authentication, "6")==0)
				encrypt = ENCRYPT_WPA2_MIXED;
			else
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			if(apmib_set(MIB_WLAN_ENCRYPT, (void *)&encrypt) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(f_wep_auth_type[0])
		{
			//1=open 2=share
			//Ingore for WPS support issue
			// DAP-1665 has no open option for mydlink setting. 
			if(strcmp(f_wep_auth_type, "2")==0)
				intVal = AUTH_SHARED;
			else
				intVal = AUTH_BOTH;
				
			apmib_set(MIB_WLAN_AUTH_TYPE, (void *)&intVal);			
		}
		
		if(f_cipher[0])
		{
			if(strcmp(f_cipher, "1")==0)
				cipherType = 1;
			else if(strcmp(f_cipher, "2")==0)
				cipherType = 2;
			else if(strcmp(f_cipher, "3")==0)
				cipherType = 3;
			else
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			if(apmib_set(MIB_WLAN_WPA_CIPHER_SUITE, (void *)&cipherType) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
				
			if(apmib_set(MIB_WLAN_WPA2_CIPHER_SUITE, (void *)&cipherType) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(f_wep_len[0] && f_wep_format[0])
		{
			if(strcmp(f_wep_len, "0")==0) // wep 64
			{
				if(strcmp(f_wep_format, "1")==0) // f_wep_format, 1:ascii, 2:hex
				{
					//wepType = WEP64_ASCII;
					wepType   = WEP64;
					wepKeyLen = WEP64_KEY_LEN;
					wepFomrat = 0;
				}
				else if(strcmp(f_wep_format, "2")==0)
				{
					wepType   = WEP64;
					wepKeyLen = WEP64_KEY_LEN*2;
					wepFomrat = 1;
				}
				else
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
			else if(strcmp(f_wep_len, "1")==0)
			{
				if(strcmp(f_wep_format, "1")==0)
				{
					//wepType   = WEP128_ASCII;
					wepType   = WEP128;
					wepKeyLen = WEP128_KEY_LEN;
					wepFomrat = 0;
				}
				else if(strcmp(f_wep_format, "2")==0)
				{
					wepType = WEP128;
					wepKeyLen = WEP128_KEY_LEN*2;
					wepFomrat = 1;
				}
				else
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
			else
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			if(apmib_set(MIB_WLAN_WEP, (void *)&wepType) == 0)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
				
			if(apmib_set(MIB_WLAN_WEP_KEY_TYPE, (void *)&wepFomrat) == 0) // 1:hex, 0:ascii
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(f_wep_def_key[0])
		{
			//Ingore for DAP1360L always use wep1
		}
		
		if(f_wep[0])
		{
			if(strlen(f_wep) != wepKeyLen)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
				
			if (wepFomrat == 0) // ascii
				strcpy(key, f_wep);
			else { // hex
				if(!string_to_hex(f_wep, key, wepKeyLen))
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
			
			if((wepType == WEP64)||(wepType == WEP64_ASCII))
			{
				if (wepFomrat == 0) // ascii
					wep_ifindex |= 0x10;
				else
					wep_ifindex |= 0x1;
				
				if(apmib_set(MIB_WLAN_WEP64_KEY1, (void *)key) == 0)
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}
			else
			{
				if (wepFomrat == 0) // ascii
					wep_ifindex |= 0x10;
				else
					wep_ifindex |= 0x1;
				if(apmib_set(MIB_WLAN_WEP128_KEY1, (void *)key) == 0)
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
			}			
		}
		
		if(f_wpa_psk[0])
		{
			//psk ascii
			if(!apmib_set(MIB_WLAN_WPA_PSK, (void *)f_wpa_psk))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(f_wpa_psk_type[0])
		{
			//Ingore for WPS support issue
		}
		
		if(f_radius_ip1[0])
		{
			//Ingore for WPS support issue
			/*if(!inet_aton(f_radius_ip1, &inIp))
				goto setErr_config;
				
			if(!apmib_set(MIB_WLAN_RS_2IP, (void *)&inIp))
				goto setErr_config;*/
		}
		
		if(f_radius_port1[0])
		{
			//Ingore for WPS support issue
			/*if(!string_to_dec(strVal, &radiusPort) || radiusPort<=0 || radiusPort>65535)
				goto setErr_config;

			if(!apmib_set(MIB_WLAN_RS_2PORT, (void *)&radiusPort))
				goto setErr_config;*/

		}
		
		if(f_radius_secret1[0])
		{
			//Ingore for WPS support issue
			/*if(strlen(f_radius_secret1) > (MAX_RS_PASS_LEN -1))
				goto setErr_config;

			if(!apmib_set(MIB_WLAN_RS_2PASSWORD, (void *)f_radius_secret1))
				goto setErr_config;*/
		}
		
		apmib_update(CURRENT_SETTING);
		OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
		return;
	}
setErr_config:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}

void form_wireless_2g(webs_t wp, char_t *path, char_t *query)
{
  int tmp_wlan_idx;
  tmp_wlan_idx = wlan_idx;
  wlan_idx = 1;
  form_wireless( wp,  path,  query);
  wlan_idx = tmp_wlan_idx;
}

void form_wireless_5g(webs_t wp, char_t *path, char_t *query)
{
  int tmp_wlan_idx;
  tmp_wlan_idx = wlan_idx;
  wlan_idx = 0;
  form_wireless( wp,  path,  query);
  wlan_idx = tmp_wlan_idx;
}

void form_emailsetting(webs_t wp, char_t *path, char_t *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	char_t *settingsChanged, *config_log_to_syslog, *config_log_syslog_addr, *config_smtp_email_addr, *config_smtp_email_subject, *config_smtp_email_from_email_addr;
	char_t *config_smtp_email_server_addr, *config_smtp_email_acc_name, *config_smtp_email_pass, *config_smtp_email_action, *config_smtp_email_port, *config_smtp_email_secret, *config_smtp_email_enable_smtp_auth;
	struct in_addr inIp;
	
	settingsChanged = websGetVar(wp, T("settingsChanged"), T(""));
	config_log_to_syslog = websGetVar(wp, T("config.log_to_syslog"), T(""));	//"true" or "false"
	config_log_syslog_addr = websGetVar(wp, T("config.log_syslog_addr"), T(""));	//""
	config_smtp_email_addr = websGetVar(wp, T("config.smtp_email_addr"), T(""));
	config_smtp_email_subject = websGetVar(wp, T("config.smtp_email_subject"), T(""));
	config_smtp_email_from_email_addr = websGetVar(wp, T("config.smtp_email_from_email_addr"), T(""));
	config_smtp_email_server_addr = websGetVar(wp, T("config.smtp_email_server_addr"), T(""));
	config_smtp_email_acc_name = websGetVar(wp, T("config.smtp_email_acc_name"), T(""));
	config_smtp_email_pass = websGetVar(wp, T("config.smtp_email_pass"), T(""));
	config_smtp_email_action = websGetVar(wp, T("config.smtp_email_action"), T(""));
	config_smtp_email_port = websGetVar(wp, T("config.smtp_email_port"), T(""));
	config_smtp_email_secret = websGetVar(wp, T("config.smtp_email_secret"), T(""));
	config_smtp_email_enable_smtp_auth = websGetVar(wp, T("config.smtp_email_enable_smtp_auth"), T(""));
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s config.log_to_syslog=%s config.log_syslog_addr=%s\n", settingsChanged, config_log_to_syslog, config_log_syslog_addr);
	fprintf(stderr,"###MYDLINK DEBUG### config.smtp_email_addr=%s config.smtp_email_subject=%s config.smtp_email_from_email_addr=%s\n", config_smtp_email_addr, config_smtp_email_subject, config_smtp_email_from_email_addr);
	fprintf(stderr,"###MYDLINK DEBUG### config.smtp_email_server_addr=%s config.smtp_email_acc_name=%s config.smtp_email_pass=%s\n", config_smtp_email_server_addr, config_smtp_email_acc_name, config_smtp_email_pass);
	fprintf(stderr,"###MYDLINK DEBUG### config.smtp_email_action=%s config.smtp_email_port=%s config.smtp_email_secret=%s\n", config_smtp_email_action, config_smtp_email_port, config_smtp_email_secret);
	fprintf(stderr,"###MYDLINK DEBUG### config.smtp_email_enable_smtp_auth=%s\n", config_smtp_email_enable_smtp_auth);
	
	if(settingsChanged[0])
	{
		if(strcmp(settingsChanged, "1"))
		{
			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
			goto setErr_config;
		}
		
		if(config_log_to_syslog[0])
		{
			if(strcmp(config_log_to_syslog,"true") && strcmp(config_log_to_syslog,"false"))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			int log_to_syslog = 0;
			if(!strcasecmp(config_log_to_syslog, T("true")))
				log_to_syslog = 1;
			else
				log_to_syslog = 0;
			
			if(!apmib_set(MIB_REMOTELOG_ENABLED, (void *)&log_to_syslog))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		/* //1665 has no WDOG MIBs
		if(config_log_syslog_addr[0])
		{
			if(!inet_aton(config_log_syslog_addr, &inIp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			if(!apmib_set( MIB_REMOTELOG_SERVER, (void *)&inIp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_addr[0])
		{
			char strTmp[32];
			int len;
			len = strlen(config_smtp_email_addr);  
			memset(strTmp,0,sizeof(strTmp));
			strncpy(strTmp,config_smtp_email_addr,sizeof(strTmp)-1);
			if(!apmib_set(MIB_WDOG_RECEIVER_MAIL, (void *)strTmp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}

			memset(strTmp,0,sizeof(strTmp));
			if(len > (sizeof(strTmp)-1))
				strncpy(strTmp,config_smtp_email_addr+(sizeof(strTmp)-1),((len > ((sizeof(strTmp)-1)*2)))?sizeof(strTmp)-1:len-(sizeof(strTmp)-1));
			if(!apmib_set(MIB_RESERVED_STR_13, (void *)strTmp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
                        
			memset(strTmp,0,sizeof(strTmp));
			if(len > ((sizeof(strTmp)-1)*2))
				strncpy(strTmp,config_smtp_email_addr+((sizeof(strTmp)-1)*2),sizeof(strTmp)-1);
			if(!apmib_set(MIB_RESERVED_STR_14, (void *)strTmp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_subject[0])
		{

		}
		
		if(config_smtp_email_from_email_addr[0])
		{
			char strTmp[32];
			int len;
			len = strlen(config_smtp_email_from_email_addr);
			memset(strTmp,0,sizeof(strTmp));
			strncpy(strTmp,config_smtp_email_from_email_addr,sizeof(strTmp)-1);
			if(!apmib_set(MIB_WDOG_SENDER_MAIL, (void *)strTmp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}

			memset(strTmp,0,sizeof(strTmp));
			if(len > (sizeof(strTmp)-1))
				strncpy(strTmp,config_smtp_email_from_email_addr+(sizeof(strTmp)-1),((len > ((sizeof(strTmp)-1)*2)))?sizeof(strTmp)-1:len-(sizeof(strTmp)-1));
			if(!apmib_set(MIB_RESERVED_STR_11, (void *)strTmp))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_server_addr[0])
		{
			if(!apmib_set(MIB_WDOG_SMTP, (void *)config_smtp_email_server_addr))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_acc_name[0])
		{
			if(!apmib_set(MIB_WDOG_SMTP_AUTH_ACCOUNT, (void *)config_smtp_email_acc_name))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_pass[0])
		{
			if(!apmib_set(MIB_WDOG_SMTP_AUTH_PASSWD, (void *)config_smtp_email_pass))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_action[0])
		{
			if(strcmp(config_smtp_email_enable_smtp_auth,"true") && strcmp(config_smtp_email_enable_smtp_auth,"false"))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_port[0])
		{
			int intVal;
			intVal = atoi(config_smtp_email_port);
			if(!apmib_set(MIB_RESERVED_WORD_6, (void *)&intVal))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_secret[0])
		{
			if(strcmp(config_smtp_email_enable_smtp_auth,"true") && strcmp(config_smtp_email_enable_smtp_auth,"false"))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_smtp_email_enable_smtp_auth[0])
		{
			if(strcmp(config_smtp_email_enable_smtp_auth,"true") && strcmp(config_smtp_email_enable_smtp_auth,"false"))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			
			int smtp_auth_enable = 0;
			if(!strcasecmp(config_smtp_email_enable_smtp_auth, T("true")))
				smtp_auth_enable = 1;
			else
				smtp_auth_enable = 0;
			
			if(!apmib_set(MIB_WDOG_SMTP_AUTH_ENABLED, (void *)&smtp_auth_enable))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		*/
		
		apmib_update(CURRENT_SETTING);
		OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
		return;
	}
	
setErr_config:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}

void form_mydlink_log_opt(webs_t wp, char_t *path, char_t *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	char_t *settingsChanged, *config_log_enable, *config_log_userloginfo, *config_log_fwupgrade, *config_log_wirelesswarn;
	int log_enable, log_userloginfo, log_fwupgrade, log_wirelesswarn;
	
	settingsChanged = websGetVar(wp, T("settingsChanged"), T(""));
	config_log_enable = websGetVar(wp, T("config.log_enable"), T(""));
	config_log_userloginfo = websGetVar(wp, T("config.log_userloginfo"), T(""));
	config_log_fwupgrade = websGetVar(wp, T("config.log_fwupgrade"), T(""));
	config_log_wirelesswarn = websGetVar(wp, T("config.log_wirelesswarn"), T(""));
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s config_log_enable=%s config_log_userloginfo=%s config_log_fwupgrade=%s config_log_wirelesswarn=%s\n", settingsChanged, config_log_enable, config_log_userloginfo, config_log_fwupgrade, config_log_wirelesswarn);
	if(settingsChanged[0])
	{
		if(config_log_enable[0])
		{
			if(atoi(config_log_enable)<0 || atoi(config_log_enable)>1)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(atoi(config_log_enable)==1)
				log_enable = 1;
			else
				log_enable = 0;
				
			if(!apmib_set(MIB_MYDLINK_LOG_HISTORY, (void *)&log_enable))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_log_userloginfo[0])
		{
			if(atoi(config_log_userloginfo)<0 || atoi(config_log_userloginfo)>1)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(atoi(config_log_userloginfo)==1)
				log_userloginfo = 1;
			else
				log_userloginfo = 0;
				
			if(!apmib_set(MIB_MYDLINK_ONLINE_LOG, (void *)&log_userloginfo))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_log_fwupgrade[0])
		{
			if(atoi(config_log_fwupgrade)<0 || atoi(config_log_fwupgrade)>1)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(atoi(config_log_fwupgrade)==1)
				log_wirelesswarn = 1;
			else
				log_wirelesswarn = 0;
				
			if(!apmib_set(MIB_MYDLINK_FW_UPGRADE_LOG, (void *)&log_wirelesswarn))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		if(config_log_wirelesswarn[0])
		{
			if(atoi(config_log_wirelesswarn)<0 || atoi(config_log_wirelesswarn)>1)
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
			if(atoi(config_log_wirelesswarn)==1)
				log_fwupgrade = 1;
			else
				log_fwupgrade = 0;
				
			if(!apmib_set(MIB_MYDLINK_WARN_CONN_LOG, (void *)&log_fwupgrade))
			{
				fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
				goto setErr_config;
			}
		}
		
		apmib_update(CURRENT_SETTING);
		OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
		return;
	}
setErr_config:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}

void form_wlan_acl(webs_t wp, char_t *path, char_t *query)
{
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	char_t *settingsChanged, *mode, *strVal, *strEnable;
	int idx, old_vwlan_idx, enabled, i, j, k, intVal;
	MACFILTER_T macEntry;
	char varName[8],tmpStr[20], macStr[13];
	
	settingsChanged = websGetVar(wp, T("settingsChanged"), T(""));
	mode = websGetVar(wp, T("mode"), T(""));	//0=disable 1=allow pass 2=deny pass
	
	fprintf(stderr,"###MYDLINK DEBUG### settingsChanged=%s macFltMode=%s\n", settingsChanged, mode);

	if(settingsChanged[0])
	{
		old_vwlan_idx = vwlan_idx;
		
		for(idx=0; idx<=5; idx++)
		{
			vwlan_idx = idx;
			
			if(mode[0])
			{
				enabled = mode[0] - '0';
				if(enabled < 0 || enabled > 1)
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
				else
					if(enabled == 1)
						enabled = 2; //deny
				
				/*if ( !strcmp(enabled, T("1")))
					intVal = 0;
				else
					intVal = 1;
				apmib_set(MIB_WLAN_WSC_DISABLE, (void *)&intVal);*/
				
				if(idx == 5)
				{
					if(apmib_set(MIB_MACFILTER_ENABLED, (void *)&enabled) == 0)
					{
			  			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
						goto setErr_config;
					}
				}
				else
				{
					if(apmib_set(MIB_WLAN_MACAC_ENABLED, (void *)&enabled) == 0)
					{
			  			fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
						goto setErr_config;
					}
				}
			}
			
			memset(&macEntry, '\0', sizeof(macEntry));
			
			/* Delete entry && update entry*/
			if(idx == 5)
			{	
				if(!apmib_set(MIB_MACFILTER_DELALL ,(void *)&macEntry))
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
				
				for (k=0; k<20; k++)
				{	
					sprintf(varName, "mac_%d", k);
					strVal = websGetVar(wp, varName, T(""));
					sprintf(varName, "enable_%d", k);
					strEnable = websGetVar(wp, varName, T(""));
					if(strVal[0])
					{
						if(strncmp(strVal,"00:00:00:00:00:00", 17))
						{
							strcpy(tmpStr, strVal);
							memset(macStr, '\0', sizeof(macStr));			
							for(i=0,j=0;i<17;i++)
							{
								if( i==2 ||i==5 ||i==8 ||i==11 ||i==14 )
									continue;
								macStr[j] = tmpStr[i];
								j++;
							}
							
							if(strlen(macStr)!=12 || !string_to_hex(macStr, macEntry.macAddr, 12))
							{
								fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
								goto setErr_config;
							}
							if(strcmp(strEnable,"1") == 0)
								macEntry.comment[0] = '1';
							else
								macEntry.comment[0] = '\0';
							
							// set to MIB. try to delete it first to avoid duplicate case
							apmib_set(MIB_MACFILTER_DEL, (void *)&macEntry);						
							if(apmib_set(MIB_MACFILTER_ADD, (void *)&macEntry) == 0)
							{
								fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
								goto setErr_config;
							}
						}
					}
				}
			}
			else
			{
				if(!apmib_set(MIB_WLAN_AC_ADDR_DELALL ,(void *)&macEntry))
				{
					fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
					goto setErr_config;
				}
				
				for (k=0; k<20; k++)
				{
					sprintf(varName, "mac_%d", k);
					strVal = websGetVar(wp, varName, T(""));
					sprintf(varName, "enable_%d", k);
					strEnable = websGetVar(wp, varName, T(""));
					if(strVal[0])
					{
						if(strncmp(strVal,"00:00:00:00:00:00", 17))
						{
							strcpy(tmpStr, strVal);
							memset(macStr, '\0', sizeof(macStr));			
							for(i=0,j=0;i<17;i++)
							{
								if( i==2 ||i==5 ||i==8 ||i==11 ||i==14 )
									continue;
								macStr[j] = tmpStr[i];
								j++;
							}
							
							if(strlen(macStr)!=12 || !string_to_hex(macStr, macEntry.macAddr, 12))
							{
								fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
								goto setErr_config;
							}
							if(strcmp(strEnable,"1") == 0)
								macEntry.comment[0] = '1';
							else
								macEntry.comment[0] = '\0';
							// set to MIB. try to delete it first to avoid duplicate case
							apmib_set(MIB_WLAN_AC_ADDR_DEL, (void *)&macEntry);
							if(apmib_set(MIB_WLAN_AC_ADDR_ADD, (void *)&macEntry) == 0)
							{
								fprintf(stderr,"###MYDLINK DEBUG### Fail %d\n",__LINE__);
								goto setErr_config;
							}
						}
					}
				}
			}
			vwlan_idx = old_vwlan_idx;
		}
		apmib_update(CURRENT_SETTING);
		OK_MSG_TEXT_PLAIN(mydlink_ret_str_ok);
		return;
	}
setErr_config:
	OK_MSG_TEXT_PLAIN(mydlink_ret_str_fail);
}

void form_triggedevent_history_enable(webs_t wp, char_t *path, char_t *query)
{}
///////////////////////////////////



#if 0
//MYDLINK PORTAL SIGN//////////////////////
void mydlinkp_signup(webs_t wp, char_t * path, char_t * query)
{
}
void mydlinkp_signin(webs_t wp, char_t * path, char_t * query)
{	
}
void mydlinkp_addev(webs_t wp, char_t * path, char_t * query)
{
}
///////////////////////////////////
#endif

//MYDLINK GET//////////////////////
int form_basic_info(request *wp, char *path, char *query) // joe changed function name of form_basic_info from basicinfo
{
	struct sockaddr hwaddr;
	unsigned char *pMacAddr;
	unsigned char buffer[512];
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	websWrite(wp, T("model=%s\n"),MODEL_NAME_T);
	websWrite(wp, T("version=%s\n"),FW_VERSION_T);
	if(getInAddr(BRIDGE_IF, HW_ADDR, (void *)&hwaddr ))
	{
		pMacAddr = hwaddr.sa_data;
		buffer[0]='\0';
		sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x", pMacAddr[0], pMacAddr[1], pMacAddr[2], pMacAddr[3], pMacAddr[4], pMacAddr[5]);
		websWrite(wp, T("macaddr=%s\n"), to_upper(buffer));
	}
	
	return 0;
}

int mydlink_fwinfo(int eid, webs_t wp, int argc, char_t **argv, char *query)
{}

int mydlink_network(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	unsigned char buffer[512];
	unsigned char *pMacAddr;
	struct sockaddr hwaddr;
	int dhcp;
	time_t t;
	struct tm *tm_time;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
		
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<network>\n"));
	//Router_name
	memset(buffer,0,sizeof(buffer));
	if(!apmib_get(MIB_HOST_NAME, (void *)&buffer))
		websWrite(wp, T("<Router_name></Router_name>\n"));
	else
	{
		//pure_replace_special_char(buffer);
		//encode_special_char(buffer);
		websWrite(wp, T("<Router_name>%s</Router_name>\n"),buffer);
	}
	//Router_model	
	websWrite(wp, T("<Router_model>%s</Router_model>\n"),MODEL_NAME_T);
	//Router_ip
	get_current_ip(wp);
	//Dhcp_sta
	if(!apmib_get( MIB_DHCP, (void *)&dhcp))
		websWrite(wp, T("<Dhcp_sta></Dhcp_sta>\n"));
	else
	{
		if(dhcp == 2)
			websWrite(wp, T("<Dhcp_sta>1</Dhcp_sta>\n"));
		else
			websWrite(wp, T("<Dhcp_sta>0</Dhcp_sta>\n"));
	}
	//Router_MAC
	memset(buffer,0,sizeof(buffer));
	if(getInAddr(BRIDGE_IF, HW_ADDR, (void *)&hwaddr ))
	{
		pMacAddr = hwaddr.sa_data;
		buffer[0]='\0';
		sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x", pMacAddr[0], pMacAddr[1], pMacAddr[2], pMacAddr[3], pMacAddr[4], pMacAddr[5]);
		websWrite(wp, T("<Router_MAC>%s</Router_MAC>\n"), to_upper(buffer));
	}
	else
		websWrite(wp, T("<Router_MAC>00:00:00:00:00:00</Router_MAC>\n"));
	//Router_ver
	websWrite(wp, T("<Router_ver>%s</Router_ver>\n"),FW_VERSION_T);
	//Router_time
	time(&t);
	tm_time = localtime(&t);
	websWrite(wp, T("<Router_time>%d/%d/%d %d:%d:%d</Router_time>\n"), tm_time->tm_mon+1, tm_time->tm_mday, tm_time->tm_year+ 1900, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);
	//Router_lan
	websWrite(wp, T("<Router_lan>0</Router_lan>\n"));
	//Router_wireless
	websWrite(wp, T("<Router_wireless>0</Router_wireless>\n"));
	
	websWrite(wp, T("</network>\n"));
	
	return 0;
}

int mydlink_wireless(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int showPass = 0, disabled;
	unsigned char buffer[512];
	int channel, encrypt, wep_auth, cipherType, wepType, wepFormat, wpaType, rsPort;
	char_t *iface=NULL;
	unsigned char *pMacAddr;
	struct sockaddr hwaddr;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s, displaypass = %d\n",__FUNCTION__,displaypass);
	if(query != NULL)
	{
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
		if(strstr("displaypass=1",query) != NULL)
			showPass = 1;
	}
	
	showPass = displaypass; //joe add for ?displaypass ...
	
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<Wireless>\n"));
	//Wireless_sta
	if(!apmib_get( MIB_WLAN_WLAN_DISABLED, (void *)&disabled))
		websWrite(wp, T("<Wireless_sta></Wireless_sta>\n"));
	else
	{
		if(disabled == 0)
			websWrite(wp, T("<Wireless_sta>1</Wireless_sta>\n"));
		else
			websWrite(wp, T("<Wireless_sta>0</Wireless_sta>\n"));
	}
	//dns
	websWrite(wp, T("<dns></dns>\n"));
	//ssid
	memset(buffer,0,sizeof(buffer));
	if(!apmib_get(MIB_WLAN_SSID, (void *)&buffer))
		websWrite(wp, T("<ssid></ssid>\n"));
	else
	{
		//pure_replace_special_char(buffer);
		encode_special_char(buffer);
		websWrite(wp, T("<ssid>%s</ssid>\n"),buffer);
	}
	//f_auto_channel
	//channel	
	if(!apmib_get( MIB_WLAN_CHANNEL, (void *)&channel))
		websWrite(wp, T("<f_auto_channel></f_auto_channel>\n"));
	else
	{
		if(channel == 0)
		{
			websWrite(wp, T("<f_auto_channel>1</f_auto_channel>\n"));
			bss_info bss;
			memset(buffer,0,sizeof(buffer));
			if(getWlBssInfo(WLAN_IF, &bss) < 0)
				websWrite(wp, T("<channel></channel>\n"));
			else
			{
				if (bss.channel)
					websWrite(wp, T("<channel>%d</channel>\n"), bss.channel);
				else
					websWrite(wp, T("<channel></channel>\n"));
			}
		}
		else
		{
			websWrite(wp, T("<f_auto_channel>0</f_auto_channel>\n"));
			websWrite(wp, T("<channel>%d</channel>\n"),channel);
		}
	}
	//mac
	iface = WLAN_IF;
	if(getInAddr(iface, HW_ADDR, (void *)&hwaddr))
	{
		pMacAddr = hwaddr.sa_data;
		buffer[0]='\0';
		sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x", pMacAddr[0], pMacAddr[1],
			       	pMacAddr[2], pMacAddr[3], pMacAddr[4], pMacAddr[5]);
		websWrite(wp, T("<mac>%s</mac>\n"), to_upper(buffer));
	}
	else
		websWrite(wp, T("<mac>00:00:00:00:00:00</mac>\n"));
	//f_authentication			
	if(!apmib_get(MIB_WLAN_ENCRYPT, (void *)&encrypt))
		websWrite(wp, T("<f_authentication></f_authentication>\n"));
	else
	{
		if(encrypt == ENCRYPT_DISABLED)
			websWrite(wp, T("<f_authentication>0</f_authentication>\n"));
		else if(encrypt == ENCRYPT_WEP)
			websWrite(wp, T("<f_authentication>1</f_authentication>\n"));
		else if(encrypt == ENCRYPT_WPA)
			websWrite(wp, T("<f_authentication>2</f_authentication>\n"));
		else if(encrypt == ENCRYPT_WPA2)
			websWrite(wp, T("<f_authentication>4</f_authentication>\n"));
		else if(encrypt == ENCRYPT_WPA2_MIXED)
			websWrite(wp, T("<f_authentication>6</f_authentication>\n"));
		else
			websWrite(wp, T("<f_authentication></f_authentication>\n"));
	}
	//f_wep_auth_type
	//typedef enum { AUTH_OPEN=0, AUTH_SHARED, AUTH_BOTH } AUTH_TYPE_T;
	if(!apmib_get( MIB_WLAN_AUTH_TYPE, (void *)&wep_auth))
		websWrite(wp, T("<f_wep_auth_type></f_wep_auth_type>\n"));
	else
	{
	  // mib value is 0:open, 1:shared, 2:both
	  // but mydlink tag <f_wep_auth_type> is 1:open, 2:shared
	  // DAP 1665 can only set "shared" or "both", can't set open 
	  if(wep_auth == 0)//open
			websWrite(wp, T("<f_wep_auth_type>1</f_wep_auth_type>\n"));
		if(wep_auth == 1)//shared
			websWrite(wp, T("<f_wep_auth_type>2</f_wep_auth_type>\n"));
		else//both, mydlink does not define both value of the <f_wep_auth_type> tag
			websWrite(wp, T("<f_wep_auth_type>2</f_wep_auth_type>\n"));
	  
	}
	//cipher_type
	if(!apmib_get(MIB_WLAN_WPA_CIPHER_SUITE, (void *)&cipherType))
		websWrite(wp, T("<cipher_type></cipher_type>\n"));
	else
		websWrite(wp, T("<cipher_type>%d</cipher_type>\n"), cipherType);
	//f_wep_len
	if(!apmib_get( MIB_WLAN_WEP, (void *)&wepType))
		websWrite(wp, T("<f_wep_len></f_wep_len>\n"));
	else
	{
		if(wepType == WEP64_ASCII || wepType == WEP64)
			websWrite(wp, T("<f_wep_len>0</f_wep_len>\n"));
		else if(wepType == WEP128_ASCII || wepType == WEP128)
			websWrite(wp, T("<f_wep_len>1</f_wep_len>\n"));
		else
			websWrite(wp, T("<f_wep_len></f_wep_len>\n"));
	}
	//f_wep_format
	if(!apmib_get( MIB_WLAN_WEP_KEY_TYPE, (void *)&wepFormat))
		websWrite(wp, T("<f_wep_format></f_wep_format>\n"));
	else
	{
		if(wepFormat == 0)
			websWrite(wp, T("<f_wep_format>1</f_wep_format>\n"));
		else if(wepFormat == 1)
			websWrite(wp, T("<f_wep_format>2</f_wep_format>\n"));
		else
			websWrite(wp, T("<f_wep_format></f_wep_format>\n"));
	}
	//f_wep_def_key
	websWrite(wp, T("<f_wep_def_key>1</f_wep_def_key>\n"));
	//f_wep
	if(showPass == 1)
	{
		memset(buffer,0,sizeof(buffer));
		if(wepType == WEP64_ASCII || wepType == WEP64)
		{
			if(!apmib_get(MIB_WLAN_WEP64_KEY1, (void *)&buffer))
				websWrite(wp, T("<f_wep></f_wep>\n"));
			else
				websWrite(wp, T("<f_wep>%s</f_wep>\n"),buffer);
		}
		else if(wepType == WEP128_ASCII || wepType == WEP128)
		{
			if(!apmib_get(MIB_WLAN_WEP128_KEY1, (void *)&buffer))
				websWrite(wp, T("<f_wep></f_wep>\n"));
			else
				websWrite(wp, T("<f_wep>%s</f_wep>\n"),buffer);
		}
		else
		{
			websWrite(wp, T("<f_wep>%s</f_wep>\n"),buffer);
		}
	}
	//f_wpa_psk_type
	if(!apmib_get( MIB_WLAN_WPA_AUTH, (void *)&wpaType))
		websWrite(wp, T("<f_wpa_psk_type></f_wpa_psk_type>\n"));
	else
	{
		if(wpaType == WPA_AUTH_AUTO)
			websWrite(wp, T("<f_wpa_psk_type>1</f_wpa_psk_type>\n"));
		else if(wpaType == WPA_AUTH_PSK)
			websWrite(wp, T("<f_wpa_psk_type>2</f_wpa_psk_type>\n"));
		else
			websWrite(wp, T("<f_wpa_psk_type></f_wpa_psk_type>\n"));
	}
	//f_wps_psk
	if(showPass == 1)
	{
		memset(buffer,0,sizeof(buffer));
		if(!apmib_get(MIB_WLAN_WPA_PSK, (void *)&buffer))
			websWrite(wp, T("<f_wps_psk></f_wps_psk>\n"));
		else
		{
			//pure_replace_special_char(buffer);
			//encode_special_char(buffer);
			websWrite(wp, T("<f_wps_psk>%s</f_wps_psk>\n"),buffer);
		}
	}
	//f_radius_ip1
	memset(buffer,0,sizeof(buffer));
	if(!apmib_get( MIB_WLAN_RS_IP,  (void *)buffer))
		websWrite(wp, T("<f_radius_ip1></f_radius_ip1>\n"));
	else
	{
		if(!memcmp(buffer, "\x0\x0\x0\x0", 4))
			websWrite(wp, T("<f_radius_ip1>0.0.0.0</f_radius_ip1>\n"));
		else
	   		websWrite(wp, T("<f_radius_ip1>%s</f_radius_ip1>\n"),inet_ntoa(*((struct in_addr *)buffer)));
   	}
   	//f_radius_port1
   	if(!apmib_get( MIB_WLAN_RS_PORT,  (void *)&rsPort))
		websWrite(wp, T("<f_radius_port1></f_radius_port1>\n"));
	else
		websWrite(wp, T("<f_radius_port1>%d</f_radius_port1>\n"), rsPort);
   	//f_radius_secret1
   	memset(buffer,0,sizeof(buffer));
	if(!apmib_get( MIB_WLAN_RS_PASSWORD,  (void *)buffer))
		websWrite(wp, T("<f_radius_secret1></f_radius_secret1>\n"));
	else
	{
		//pure_replace_special_char(buffer);
		//encode_special_char(buffer);
		websWrite(wp, T("<f_radius_secret1>%s</f_radius_secret1>\n"), buffer);
	}

	websWrite(wp, T("</Wireless>\n"));
	
	return 0;
}

// ALL WDOG MIBs removed
int mydlink_email(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int showPass = 0, authEnable, smtpPort, logToRemote;
	unsigned char buffer[512];
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
	{
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
		if(strstr("displaypass=1",query) != NULL)
			showPass = 1;
	}
	
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<emailsetting>\n"));
	//config.log_to_syslog
	if(!apmib_get( MIB_REMOTELOG_ENABLED,  (void *)&logToRemote))
		websWrite(wp, T("<config.log_to_syslog></config.log_to_syslog>\n"));
	else
		if(logToRemote == 1)
			websWrite(wp, T("<config.log_to_syslog>true</config.log_to_syslog>\n"));
		else
			websWrite(wp, T("<config.log_to_syslog>false</config.log_to_syslog>\n"));
	//config.log_syslog_addr
	memset(buffer,0,sizeof(buffer));
	if(!apmib_get( MIB_REMOTELOG_SERVER,  (void *)buffer))
		websWrite(wp, T("<config.log_syslog_addr></config.log_syslog_addr>\n"));
	else
	{
		if (!memcmp(buffer, "\x0\x0\x0\x0", 4))
			websWrite(wp, T("<config.log_syslog_addr></config.log_syslog_addr>\n"));
		else
			websWrite(wp, T("<config.log_syslog_addr>%s</config.log_syslog_addr>\n"), inet_ntoa(*((struct in_addr *)buffer)));
	}
	//log_opt_system
	websWrite(wp, T("<log_opt_system></log_opt_system>\n"));
	//log_opt_dropPackets
	websWrite(wp, T("<log_opt_dropPackets></log_opt_dropPackets>\n"));
	//log_opt_SysActivity
	websWrite(wp, T("<log_opt_SysActivity></log_opt_SysActivity>\n"));
	//config.smtp_email_addr
	memset(buffer,0,sizeof(buffer));

		if(!apmib_get( MIB_RESERVED_STR_13,  (void *)buffer+31))
			websWrite(wp, T("<config.smtp_email_addr></config.smtp_email_addr>\n"));
		else
			if(!apmib_get( MIB_RESERVED_STR_14,  (void *)buffer+62))
				websWrite(wp, T("<config.smtp_email_addr></config.smtp_email_addr>\n"));
			else
				websWrite(wp, T("<config.smtp_email_addr>%s</config.smtp_email_addr>\n"), buffer);
	//config.smtp_email_subject
	websWrite(wp, T("<config.smtp_email_subject></config.smtp_email_subject>\n"));
	//config.smtp_email_from_email_addr
	memset(buffer,0,sizeof(buffer));

		if(!apmib_get( MIB_RESERVED_STR_11,  (void *)buffer+31))
			websWrite(wp, T("<config.smtp_email_from_email_addr></config.smtp_email_from_email_addr>\n"));
		else
			if(!apmib_get( MIB_RESERVED_STR_12,  (void *)buffer+62))
				websWrite(wp, T("<config.smtp_email_from_email_addr></config.smtp_email_from_email_addr>\n"));
			else
				websWrite(wp, T("<config.smtp_email_from_email_addr>%s</config.smtp_email_from_email_addr>\n"), buffer);
	//config.smtp_email_server_addr
	memset(buffer,0,sizeof(buffer));

		websWrite(wp, T("<config.smtp_email_server_addr>%s</config.smtp_email_server_addr>\n"), buffer);
	//config.smtp_email_enable_smtp_auth	

		if(authEnable == 1)
			websWrite(wp, T("<config.smtp_email_enable_smtp_auth>true</config.smtp_email_enable_smtp_auth>\n"));
		else
			websWrite(wp, T("<config.smtp_email_enable_smtp_auth>false</config.smtp_email_enable_smtp_auth>\n"));
	//config.smtp_email_acc_name
	memset(buffer,0,sizeof(buffer));

		websWrite(wp, T("<config.smtp_email_acc_name>%s</config.smtp_email_acc_name>\n"), buffer);
	//config.smtp_email_pass
	if(showPass == 1)
	{
		memset(buffer,0,sizeof(buffer));
			websWrite(wp, T("<config.smtp_email_pass>%s</config.smtp_email_pass>\n"), buffer);
	}
	//config.smtp_email_port
	if(!apmib_get( MIB_RESERVED_WORD_6,  (void *)&smtpPort))
		websWrite(wp, T("<config.smtp_email_port></config.smtp_email_port>\n"));
	else
		websWrite(wp, T("<config.smtp_email_port>%d</config.smtp_email_port>\n"), smtpPort);
	//config.smtp_email_secret
	websWrite(wp, T("<config.smtp_email_secret>false</config.smtp_email_secret>\n"));
	websWrite(wp, T("</emailsetting>\n"));
	
	return 0;
}

int mydlink_dhcpclient(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	FILE *fp;
	int element=0, ret;
	char ipAddr[40], macAddr[40], liveTime[80], *buf=NULL, *ptr, tmpBuf[100], hostName[64];
	struct stat status;
	int pid;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
	
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<dhcp_clients>\n"));
	
	// siganl DHCP server to update lease file
	snprintf(tmpBuf, 100, "%s/%s.pid", _DHCPD_PID_PATH, _DHCPD_PROG_NAME);
	pid = getPid(tmpBuf);
	if ( pid > 0)
		kill(pid, SIGUSR1);
	usleep(1000);

	if ( stat(_PATH_DHCPS_LEASES, &status) < 0 )
		goto err;

	buf = malloc(status.st_size);
	if ( buf == NULL )
		goto err;

	fp = fopen(_PATH_DHCPS_LEASES, "r");
	if ( fp == NULL )
		goto err;

	fread(buf, sizeof(status.st_size)/*1*/, status.st_size, fp);
	fclose(fp);

	ptr = buf;
	
	while (1) {
		ret = getOneDhcpClient(&ptr, &status.st_size, hostName, ipAddr, macAddr, liveTime);

		if (ret < 0)
			break;
		if (ret == 0)
			continue;
		websWrite(wp, T("<client>\n"));
		websWrite(wp, T("<ip_address>%s</ip_address>\n"), ipAddr);
		websWrite(wp, T("<mac>%s</mac>\n"), to_upper(macAddr));
		//pure_replace_special_char(hostName);
		//encode_special_char(hostName);
		websWrite(wp, T("<host_name>%s</host_name>\n"), hostName);
		websWrite(wp, T("<seconds_remaining>%s</seconds_remaining>\n"), liveTime);
		websWrite(wp, T("<is_reservation>0</is_reservation>\n"));
		websWrite(wp, T("<learned_by>0</learned_by>\n"));
		element++;
		websWrite(wp, T("</client>\n"));
	}
err:
	if (buf)
		free(buf);
		
	websWrite(wp, T("</dhcp_clients>\n"));
	
	return 0;
}

int mydlink_logopt(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int enable, userloginfo, fwupgrade, wirelesswarn;
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);

	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<mydlink_logopt>\n"));
	//config.log_enable
	if(!apmib_get( MIB_MYDLINK_LOG_HISTORY,  (void *)&enable))
		websWrite(wp, T("<config.log_enable></config.log_enable>\n"));
	else
		websWrite(wp, T("<config.log_enable>%d</config.log_enable>\n"), enable);
	//config.log_userloginfo
	if(!apmib_get( MIB_MYDLINK_ONLINE_LOG,  (void *)&userloginfo))
		websWrite(wp, T("<config.log_userloginfo></config.log_userloginfo>\n"));
	else
		websWrite(wp, T("<config.log_userloginfo>%d</config.log_userloginfo>\n"), userloginfo);
	//config.log_fwupgrade
	if(!apmib_get( MIB_MYDLINK_FW_UPGRADE_LOG,  (void *)&fwupgrade))
		websWrite(wp, T("<config.log_fwupgrade></config.log_fwupgrade>\n"));
	else
		websWrite(wp, T("<config.log_fwupgrade>%d</config.log_fwupgrade>\n"), fwupgrade);
	//config.log_wirelesswarn
	if(!apmib_get( MIB_MYDLINK_WARN_CONN_LOG,  (void *)&wirelesswarn))
		websWrite(wp, T("<config.wirelesswarn></config.wirelesswarn>\n"));
	else
		websWrite(wp, T("<config.wirelesswarn>%d</config.wirelesswarn>\n"), wirelesswarn);
	websWrite(wp, T("</mydlink_logopt>\n"));
	
	return 0;
}

int mydlink_triggedevent_history_enable(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int enable;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
		
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	//enable
	if(!apmib_get( MIB_MYDLINK_TRIG_EVENT,  (void *)&enable))
		websWrite(wp, T("<enable></enable>\n"));
	else
		websWrite(wp, T("<enable>%d</enable>\n"), enable);
	
	return 0;
}

int mydlink_triggedevent_history(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int count, i;
	FILE *fp;
	time_t t = time(NULL);
	struct tm *timeinfo = localtime(&t);
	
	i = count =0;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
	
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<mydlink_triggedevent_history>\n"));
	
	//WLAN conn_success
	get_wlan_conn_info(wp, "/proc/wlan0/conn_success",1);
	//WLAN conn_fail
	get_wlan_conn_info(wp, "/proc/wlan0/conn_fail",0);
	
	websWrite(wp, T("</mydlink_triggedevent_history>\n"));
	return 0;
}

int mydlink_wlan_acl(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	int enable, entryNum, i, item;
	MACFILTER_T entry;
	
	fprintf(stderr,"###MYDLINK DEBUG### %s\n",__FUNCTION__);
	if(query != NULL)
		fprintf(stderr,"###MYDLINK DEBUG### query=%s\n",query);
	
	websWrite(wp, T("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"));
	websWrite(wp, T("<mydlink_wlan_acl>\n"));
	//mode
	//if(!apmib_get( MIB_MYDLINK_TRIG_EVENT,  (void *)&enable))
	if(!apmib_get( MIB_MACFILTER_ENABLED,  (void *)&enable))
		websWrite(wp, T("<mode></mode>\n"));
	else
		//if(enable == 2)
		if(enable == 1 || enable == 2) //eded test
			websWrite(wp, T("<mode>1</mode>\n"));
		else if(enable == 0)
			websWrite(wp, T("<mode>0</mode>\n"));
		else
			websWrite(wp, T("<mode></mode>\n"));

	//each entry
	if(!apmib_get(MIB_MACFILTER_TBL_NUM, (void *)&entryNum))
		websWrite(wp, T("</mydlink_wlan_acl>\n"));
	else
	{
		for(i=1;i<=entryNum;i++)
		{
			char tmpBuf[100];
			item = i;
			*((char *)&entry) = (char)item;
			if(!apmib_get(MIB_MACFILTER_TBL, (void *)&entry))
				continue;
			else
			{
				/*websWrite(wp, T("<mac>\n"));
				if(strcmp(entry.comment,"1") == 0 )
						websWrite(wp, T("<enable>1</enable>\n"));
				else
					websWrite(wp, T("<enable>0</enable>\n"));
				snprintf(tmpBuf, 100, T("%02x:%02x:%02x:%02x:%02x:%02x"),
				entry.macAddr[0], entry.macAddr[1], entry.macAddr[2],
				entry.macAddr[3], entry.macAddr[4], entry.macAddr[5]);
				websWrite(wp, T("<addr>%s</addr>\n"), to_upper(tmpBuf));
				websWrite(wp, T("</mac>\n"));*/
				//============eded test start==============
				if(strcmp(entry.comment,"1") == 0 && enable != 0) 
				{
						websWrite(wp, T("<mac>\n"));
						websWrite(wp, T("<enable>1</enable>\n"));
						snprintf(tmpBuf, 100, T("%02x:%02x:%02x:%02x:%02x:%02x"),
						entry.macAddr[0], entry.macAddr[1], entry.macAddr[2],
						entry.macAddr[3], entry.macAddr[4], entry.macAddr[5]);
						websWrite(wp, T("<addr>%s</addr>\n"), to_upper(tmpBuf));
						websWrite(wp, T("</mac>\n"));
				}
				//============eded test end==============
			}
		}
		websWrite(wp, T("</mydlink_wlan_acl>\n"));
	}
	
	return 0;
}
///////////////////////////////////


//have no mydlink accout, 
//1. reg. a new mydlink account
//2. sign in mydlink 
//3. add this device 
void form_mydlink_signup(webs_t wp, char_t * path, char_t * query)
{
  
  printf("\n\n\n\n\n\n\n\n##################\n%s:%d eded test mydlink.c \n###############\n\n\n\n\n",__FILE__,__LINE__);//eded test 
	char *f_acct, *f_passwd, *f_fname, *f_lname;
	char *f_wzlang;
	char ret_content[256];
	
	f_acct = req_get_cstream_var(wp, "mdl_acc_name", "");
	f_passwd = req_get_cstream_var(wp, "mdl_acc_passwd", "");
	f_fname = req_get_cstream_var(wp, "mdl_first_name", "");
	f_lname = req_get_cstream_var(wp, "mdl_last_name", "");
	f_wzlang = req_get_cstream_var(wp, "mdl_wizard_lang", "");
printf("%s(%d):f_acct=%s,f_passwd=%s!\n",__FUNCTION__,__LINE__,f_acct,f_passwd);
	if(apmib_set(MIB_MYDLINK_EMAILACCOUNT, (void *)f_acct) == 0)
	{
		goto setErr_config;
	}
	if(apmib_set(MIB_MYDLINK_ACCOUNTPASSWORD, (void *)f_passwd) == 0)
	{
		goto setErr_config;
	}
	if(apmib_set(MIB_MYDLINK_FIRSTNAME, (void *)f_fname) == 0)
	{
		goto setErr_config;
	}
	if(apmib_set(MIB_MYDLINK_LASTNAME, (void *)f_lname) == 0)
	{
		goto setErr_config;
	}
	if(apmib_set(MIB_MYDLINK_WZ_LANG, (void *)f_wzlang) == 0)
	{
		goto setErr_config;
	}
	apmib_update(CURRENT_SETTING);
	
	if (mydlink_signup(ret_content)==RET_ERR)
	{
		goto setErr_config;
	}
	/*
	else
		websRedirect(wp, "/setup_mydlink_chkmail.htm");
		*/

	if (mydlink_signin(ret_content)==RET_ERR)
	{
		goto setErr_config;
	}
	if (mydlink_addev(ret_content)==RET_ERR)
	{
		goto setErr_config;
	}
	
	OK_MSG0("http://www.mydlink.com", 5);
	return;
	
setErr_config:
	if (strlen(ret_content)==0)
	{
		ERR_MSG_FORWARD("/wizard_mydlink_reg.htm", retMLS("multi157"));	
	}
	else
	{
		ERR_MSG_FORWARD("/wizard_mydlink_reg.htm", ret_content);	
	}
}

void form_mydlink_signin(webs_t wp, char_t * path, char_t * query)
{
	char *f_acct, *f_passwd;
	char *f_wzlang;
	char *a_page;
	char ret_content[256];
	
	a_page = req_get_cstream_var(wp, "action_page", "");
	printf("%s(%d):%s!\n",__FUNCTION__,__LINE__,a_page);
	if (strcmp(a_page, "sign_in") == 0)
	{
		f_acct = req_get_cstream_var(wp, "mdl_acc_name", "");
		f_passwd = req_get_cstream_var(wp, "mdl_acc_passwd", "");
printf("%s(%d):a_page=%s,f_acct=%s,f_passwd=%s!\n",__FUNCTION__,__LINE__,a_page,f_acct,f_passwd);
		if(apmib_set(MIB_MYDLINK_EMAILACCOUNT, (void *)f_acct) == 0)
		{
			goto setErr_config;
		}
		if(apmib_set(MIB_MYDLINK_ACCOUNTPASSWORD, (void *)f_passwd) == 0)
		{
			goto setErr_config;
		}
		apmib_update(CURRENT_SETTING);
	}
	if (mydlink_signin(ret_content)==RET_ERR)
	{
		goto setErr_config;
	}
	if (mydlink_addev(ret_content)==RET_ERR)
	{
		goto setErr_config;
	}

	OK_MSG0("http://www.mydlink.com", 5);
	return;
	
setErr_config:
	if (strlen(ret_content)==0)
	{
	  ERR_MSG_FORWARD("/wizard_mydlink_reg.htm", retMLS("multi155"));	
	}
	else
	{
		ERR_MSG_FORWARD("/wizard_mydlink_reg.htm", ret_content);	
	}
	
}
void websRedirect(webs_t  wp, char_t *url)
{
    time_t now;
    char timebuf[100];

    websWrite( wp, "%s %d %s\r\n", PROTOCOL, 302, "Redirect" );
    websWrite( wp, "Server: %s\r\n", SERVER_NAME );
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    websWrite( wp, "Date: %s\r\n", timebuf );
    websWrite( wp, "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
    websWrite( wp, "Content-Type: text/html\r\n");
    websWrite( wp, "Location: %s\r\n", url);
    websWrite( wp, "\r\n" );

   // websDone(wp, 302);
 }

void form_check_internet(webs_t wp, char_t * path, char_t * query)
{
	if (!check_internet())
	  websRedirect(wp, "/wizard_mydlink.htm"); //eded test mark 
	  //websRedirect(wp, "/wizard_mydlink_reg.htm");
	else
		websRedirect(wp, "/chk_mydlink.htm"); //eded test mark
}

int mydlink_check_internet(int eid, webs_t wp, int argc, char_t **argv, char *query)
{
	if (!check_internet())
		websWrite(wp, T("good"));
	else{
		websWrite(wp, T("bad"));
	}
	return 0;
}


struct term_urls {
	char *lango;
	char *code;
	int   Idx;
	char *hyperl;
};

struct term_urls term_url[] = {
{"Czech", 							"cs", 		12,	"http://eu.mydlink.com/termsOfUse?lang=cs#"},  
{"Danish", 							"da",			13,	"http://eu.mydlink.com/termsOfUse?lang=da#"},
{"German", 							"de", 		3,	"http://eu.mydlink.com/termsOfUse?lang=de#"},
{"Greek", 							"el", 		14,	"http://eu.mydlink.com/termsOfUse?lang=el#"},
{"English", 						"en",	 		1,	"http://eu.mydlink.com/termsOfUse?lang=en#"},
{"Spanish", 						"es", 		2,	"http://eu.mydlink.com/termsOfUse?lang=es#"},
{"Finnish", 						"fi", 		15,	"http://eu.mydlink.com/termsOfUse?lang=fi#"},
{"French", 							"fr", 		4,	"http://eu.mydlink.com/termsOfUse?lang=fr#"},
{"Croatian", 						"hr", 		16,	"http://eu.mydlink.com/termsOfUse?lang=hr#"},
{"Hungarian", 					"hu", 		17,	"http://eu.mydlink.com/termsOfUse?lang=hu#"},
{"Italian", 						"it", 		5,	"http://eu.mydlink.com/termsOfUse?lang=it#"},
{"Japanese", 						"ja", 		8,	"http://eu.mydlink.com/termsOfUse?lang=ja#"},
{"Korean", 							"ko", 		11,	"http://eu.mydlink.com/termsOfUse?lang=ko#"},
{"Dutch", 							"nl", 		18,	"http://eu.mydlink.com/termsOfUse?lang=nl#"},
{"Norwegian", 					"no", 		19,	"http://eu.mydlink.com/termsOfUse?lang=no#"},
{"Polish", 							"pl", 		20,	"http://eu.mydlink.com/termsOfUse?lang=pl#"},
{"Portuguese", 					"pt", 		7,	"http://eu.mydlink.com/termsOfUse?lang=pt#"},
{"Romanian", 						"ro", 		22,	"http://eu.mydlink.com/termsOfUse?lang=ro#"},
{"Russian", 						"ru", 		6,	"http://eu.mydlink.com/termsOfUse?lang=ru#"}, 
{"Slovenian", 					"sl", 		23,	"http://eu.mydlink.com/termsOfUse?lang=sl#"},
{"Swedish", 						"sv", 		24,	"http://eu.mydlink.com/termsOfUse?lang=sv#"},
{"Simplified Chinese",	"zh_CN", 	10,	"http://cn.mydlink.com/termsOfUse?lang=zh_CN#"},
{"Traditional Chinese", "zh_TW", 	9,	"http://tw.mydlink.com/termsOfUse?lang=zh_TW#"},
{NULL, NULL, NULL}
};

int mydlink_TermURL(int eid, webs_t wp, int argc, char_t **argv, char *query, char *buffer)
{
	char mib_wzlang[32]={0x0};
	struct term_urls *web_term_url;
	char *tmpstr;
	
	tmpstr = term_url[4].hyperl;	//default english
	if( apmib_get(MIB_MYDLINK_WZ_LANG, (void *)mib_wzlang) )
	{
		for(web_term_url = term_url; web_term_url->code; web_term_url++)
		{
			if( match(web_term_url->code, mib_wzlang) )
			{
				tmpstr = web_term_url->hyperl;
				break;
			}
		}		
	}
	strcpy(buffer, tmpstr);
	//websWrite(wp, T("%s"), tmpstr);
	return 0;
}

int set_mydlink_wz_lang( int index )
{
}

int get_mydlink_wz_lang_idx( void )
{
}