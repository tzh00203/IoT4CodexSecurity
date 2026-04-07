/*
 *      Utiltiy function for setting wlan application 
 *
 */

/*-- System inlcude files --*/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <arpa/inet.h>
#include <unistd.h>
    
#include "apmib.h"
#include "sysconf.h"
#include "sys_utility.h"

//#define SDEBUG(fmt, args...) printf("[%s %d]"fmt,__FUNCTION__,__LINE__,## args)
#define SDEBUG(fmt, args...) {}

extern int SetWlan_idx(char * wlan_iface_name);
//extern int wlan_idx;	// interface index 
//extern int vwlan_idx;	// initially set interface index to root   
extern int apmib_initialized;

#define IWCONTROL_PID_FILE "/var/run/iwcontrol.pid"
#define PATHSEL_PID_FILE1 "/var/run/pathsel-wlan0.pid"
#define PATHSEL_PID_FILE2 "/var/run/pathsel-wlan1.pid"
#define PATHSEL_PID_FILE3 "/var/run/pathsel-wlan-msh.pid"
#define IAPP_PID_FILE "/var/run/iapp.pid"
#define MESH_PATHSEL "/bin/pathsel" 
#define MAX_CHECK_PID_NUM 10
#ifdef CONFIG_RTL_WAPI_SUPPORT
#define MAX_WAPI_CONF_NUM 10
int asIpExist(WAPI_ASSERVER_CONF_T *pwapiconf, const unsigned char wapiCertSel, char *asipaddr, int *index)
{
	int j;
	if(NULL == pwapiconf || NULL == asipaddr || NULL == index )
		return 0;
	for(j=0;j<MAX_WAPI_CONF_NUM;j++)
	{
		if(pwapiconf[j].valid){
			if((!memcmp(pwapiconf[j].wapi_asip,asipaddr,4)) && (pwapiconf[j].wapi_cert_sel == wapiCertSel)){
				*index=j;
				return 1;
			}
		} else {
			*index = j;
			break;
		}		
	}
	return 0;
}
#endif

int setWlan_Applications(char *action, char *argv)
{
	int pid=-1;
	char strPID[10];
	char iface_name[16];
	char tmpBuff[100], tmpBuff1[100], arg_buff[200],wlan_wapi_asipaddr[100],iapp_interface[200]= {0};
	int wlan_wapi_cert_sel;
	int _enable_1x=0, _use_rs=0;
	int wlan_mode_root=0,wlan_disabled_root=0, wlan_wpa_auth_root=0,wlan1_wpa_auth_root=0;
	int wlan0_mode=1, wlan1_mode=1, both_band_ap=0;
	int wlan_iapp_disabled_root=0,wlan_wsc_disabled_root=0, wlan_network_type_root=0, wlan0_wsc_disabled_vxd=1, wlan1_wsc_disabled_vxd=1;
	int wlan_1x_enabled_root=0, wlan_encrypt_root=0, wlan_mac_auth_enabled_root=0,wlan_wapi_auth=0;
	int wlan_disabled=0, wlan_mode=0, wlan_wds_enabled=0, wlan_wds_num=0;
	int wlan_encrypt=0, wlan_wds_encrypt=0;
	int wlan_wpa_auth=0, wlan_mesh_encrypt=0;
	int wlan_1x_enabled=0,wlan_mac_auth_enabled=0;
	int wlan_root_auth_enable=0, wlan_vap_auth_enable=0;
	int wlan_network_type=0, wlan_wsc_disabled=0, wlan_iapp_disabled=0,wlan0_hidden_ssid_enabled=0,wlan1_hidden_ssid_enabled=0;
	char tmp_iface[30]={0}, wlan_role[30]={0}, wlan_vap[30]={0}, wlan_vxd[30]={0};
	char valid_wlan_interface[200]={0}, all_wlan_interface[200]={0};
	int vap_not_in_pure_ap_mode=0, deamon_created=0;
	int isWLANEnabled=0, isAP=0, isIAPPEnabled=0, intValue=0;
	char bridge_iface[30]={0};
	char *token=NULL, *savestr1=NULL;
	int wps_debug=0, use_iwcontrol=1;
	int WSC=1, WSC_UPNP_Enabled=0;
	
	FILE *fp;
	char wsc_pin_local[16]={0},wsc_pin_peer[16]={0};
	int wait_fifo=0;
	char *cmd_opt[16]={0};
	int cmd_cnt = 0;
	int check_cnt = 0;
	//Added for virtual wlan interface
	int i=0, set_val=0, wlan_encrypt_virtual=0;
	char wlan_vname[16];
#ifdef CONFIG_RTL_WAPI_SUPPORT
	/*assume MAX 10 configuration*/
	char wlan_name[10];
	int wlan_index;
	int index;
	int apAsAS;
	int wlanBand2G5GSelect;
	WAPI_ASSERVER_CONF_T wapiconf[MAX_WAPI_CONF_NUM];
	int wlan_wapi_auth_root;
#endif

#if defined(CONFIG_RTL_92D_SUPPORT)
	int wlan_wsc1_disabled = 1 ;
	int wlan1_disabled_root = 1;
#endif	

#if defined(CONFIG_RTK_MESH)
    int wlan0_mesh_enabled=0,wlan1_mesh_enabled=0;
#endif
#ifdef WLAN_HS2_CONFIG
	int wlan0_hs2_enable = 0, wlan0_va0_hs2_enable = 0, wlan0_va1_hs2_enable = 0, wlan0_va2_hs2_enable = 0, wlan0_va3_hs2_enable = 0;	
	int wlan1_hs2_enable = 0, wlan1_va0_hs2_enable = 0, wlan1_va1_hs2_enable = 0, wlan1_va2_hs2_enable = 0, wlan1_va3_hs2_enable = 0;	
	int wlan0_hs2_conf_enable = 0;//, wlan0_va0_hs2_conf_enable = 0, wlan0_va1_hs2_conf_enable = 0, wlan0_va2_hs2_conf_enable = 0, wlan0_va3_hs2_conf_enable = 0;	
	int wlan1_hs2_conf_enable = 0;//, wlan1_va0_hs2_conf_enable = 0, wlan1_va1_hs2_conf_enable = 0, wlan1_va2_hs2_conf_enable = 0, wlan1_va3_hs2_conf_enable = 0;	
	
#endif

#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)
	int isRptEnabled1=0;
	int isRptEnabled2=0;

	SetWlan_idx("wlan0-vxd");
	apmib_get(MIB_REPEATER_ENABLED1, (void *)&isRptEnabled1);
	apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&wlan0_wsc_disabled_vxd);
				
#if defined(CONFIG_RTL_92D_SUPPORT)		
	SetWlan_idx("wlan1-vxd");
	apmib_get(MIB_REPEATER_ENABLED2, (void *)&isRptEnabled2);
	apmib_get( MIB_WLAN_WSC_DISABLE, (void *)&wlan1_wsc_disabled_vxd);
#endif
#endif //#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)

#ifdef WLAN_HS2_CONFIG
	SetWlan_idx("wlan0");
	apmib_get( MIB_WLAN_HS2_ENABLE, (void *)&wlan0_hs2_conf_enable);
	printf("wlan0_hs2_conf_enable=%d\n",wlan0_hs2_conf_enable);
	SetWlan_idx("wlan1");
	apmib_get( MIB_WLAN_HS2_ENABLE, (void *)&wlan1_hs2_conf_enable);
	printf("wlan1_hs2_conf_enable=%d\n",wlan1_hs2_conf_enable);
#endif


#if defined(FOR_DUAL_BAND)
	SetWlan_idx("wlan0");
	apmib_get(MIB_WLAN_MODE, (void *)&wlan0_mode);
	apmib_get(MIB_WLAN_WLAN_DISABLED, (void *)&wlan_disabled_root);
	apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&wlan_wsc_disabled);
	apmib_get(MIB_WLAN_HIDDEN_SSID, (void *)&wlan0_hidden_ssid_enabled);
	apmib_get(MIB_WLAN_WPA_AUTH, (void *)&wlan_wpa_auth_root);
	SetWlan_idx("wlan1");
	apmib_get( MIB_WLAN_MODE, (void *)&wlan1_mode);
	apmib_get(MIB_WLAN_WLAN_DISABLED, (void *)&wlan1_disabled_root);
	apmib_get(MIB_WLAN_WSC_DISABLE, (void *)&wlan_wsc1_disabled);
	apmib_get(MIB_WLAN_HIDDEN_SSID, (void *)&wlan1_hidden_ssid_enabled);
	apmib_get(MIB_WLAN_WPA_AUTH, (void *)&wlan1_wpa_auth_root);
	if( ((wlan0_mode == AP_MODE) || (wlan0_mode == AP_WDS_MODE) || (wlan0_mode == AP_MESH_MODE)) && ((wlan1_mode == 0) || (wlan1_mode == AP_WDS_MODE) || (wlan1_mode == AP_MESH_MODE))
		&& (wlan_disabled_root == 0) && (wlan1_disabled_root == 0) && (wlan_wsc_disabled == 0) && (wlan_wsc1_disabled == 0) && (wlan0_hidden_ssid_enabled ==0) && (wlan1_hidden_ssid_enabled ==0)
		&&(wlan_wpa_auth_root != WPA_AUTH_AUTO)&&(wlan1_wpa_auth_root != WPA_AUTH_AUTO) )
	{
#if defined(CONFIG_WPS_EITHER_AP_OR_VXD)
		if ( (isRptEnabled1 == 1 && wlan_disabled_root == 0 && wlan_wsc_disabled == 0) 
			|| (isRptEnabled2 == 1 && wlan1_disabled_root == 0 && wlan_wsc1_disabled == 0))
			both_band_ap = 0;
		else
			both_band_ap = 1;
#else
	#if (NUM_WLAN_INTERFACE==2)
			both_band_ap = 1;	
	#else
			both_band_ap = 0;	
	#endif
#endif
	}


	
	SetWlan_idx("wlan0");
#endif

	
#ifdef CONFIG_RTL_P2P_SUPPORT							
	int p2p_mode=0;
#endif

	token=NULL;
	savestr1=NULL;	     
	sprintf(arg_buff, "%s", argv);	

    SDEBUG("arg_buff=[%s]\n",arg_buff);

	token = strtok_r(arg_buff," ", &savestr1);
	do{
		if (token == NULL){/*check if the first arg is NULL*/
			break;
		}else{        
			sprintf(iface_name, "%s", token);                                           		
			if(strncmp(iface_name, "wlan", 4)==0){//wlan iface   
				if(all_wlan_interface[0]==0x0){
					sprintf(all_wlan_interface, "%s",iface_name); 
				}else{
					sprintf(tmp_iface, " %s", iface_name);
					strcat(all_wlan_interface, tmp_iface);
				}
			}else{
				sprintf(bridge_iface, "%s", iface_name);
			}
		}
		token = strtok_r(NULL, " ", &savestr1);
	}while(token !=NULL);
	//printf("bridge_iface=%s\n", bridge_iface);
	
	
	
	if(isFileExist(IWCONTROL_PID_FILE)){
		pid=getPid_fromFile(IWCONTROL_PID_FILE);
		if(pid != -1){
			sprintf(strPID, "%d", pid);
			RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
		}
		unlink(IWCONTROL_PID_FILE);
	}

//for mesh========================================================
#if defined(CONFIG_RTK_MESH)    
	if(isFileExist(PATHSEL_PID_FILE1)){
		pid=getPid_fromFile(PATHSEL_PID_FILE1);
		if(pid != -1){
			sprintf(strPID, "%d", pid);
			RunSystemCmd(NULL_FILE, "kill", "-9", strPID,NULL_STR);
		}
		unlink(PATHSEL_PID_FILE1);
		RunSystemCmd(NULL_FILE, "brctl", "meshsignaloff",NULL_STR);
		
	}
	if(isFileExist(PATHSEL_PID_FILE2)){
		pid=getPid_fromFile(PATHSEL_PID_FILE2);
		if(pid != -1){
			sprintf(strPID, "%d", pid);
			RunSystemCmd(NULL_FILE, "kill", "-9", strPID,NULL_STR);
		}
		unlink(PATHSEL_PID_FILE2);
		RunSystemCmd(NULL_FILE, "brctl", "meshsignaloff",NULL_STR);
		
	}
	if(isFileExist(PATHSEL_PID_FILE3)){
		pid=getPid_fromFile(PATHSEL_PID_FILE3);
		if(pid != -1){
			sprintf(strPID, "%d", pid);
			RunSystemCmd(NULL_FILE, "kill", "-9", strPID,NULL_STR);
		}
		unlink(PATHSEL_PID_FILE3);
		RunSystemCmd(NULL_FILE, "brctl", "meshsignaloff",NULL_STR);
		
	}    
#endif

	token=NULL;
	savestr1=NULL;	     
	sprintf(arg_buff, "%s", all_wlan_interface);
	token = strtok_r(arg_buff," ", &savestr1);
	do{
		if (token == NULL){/*check if the first arg is NULL*/
			break;
		}else{                
			sprintf(iface_name, "%s", token);    	
			if(strncmp(iface_name, "wlan", 4)==0){//wlan iface   
				sprintf(tmpBuff, "/var/run/auth-%s.pid",iface_name);
				if(isFileExist(tmpBuff)){
					pid=getPid_fromFile(tmpBuff);
						if(pid != -1){
							sprintf(strPID, "%d", pid);
							RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
						}
					unlink(tmpBuff);
					sprintf(tmpBuff1, "/var/run/auth-%s-vxd.pid",iface_name);
					if(isFileExist(tmpBuff1)){
					pid=getPid_fromFile(tmpBuff1);
						if(pid != -1){
							sprintf(strPID, "%d", pid);
							RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
						}
					unlink(tmpBuff1);
					}
				}
#if 1 //def FOR_DUAL_BAND
			if(both_band_ap == 1)
				sprintf(tmpBuff1, "/var/run/wscd-wlan0-wlan1.pid");
			else
				sprintf(tmpBuff1, "/var/run/wscd-%s.pid",iface_name);
#endif
				if(isFileExist(tmpBuff1)){
					pid=getPid_fromFile(tmpBuff1);
					if(pid != -1){
						sprintf(strPID, "%d", pid);
						RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
					}
					unlink(tmpBuff1);
				}
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)
   if (find_pid_by_name("wscd") > 0)
   {
     // printf("\n\nkillall wscd.\n\n"); // joe debug
      RunSystemCmd(NULL_FILE, "killall", "-9", "wscd", NULL_STR);
   }
/* //joe, if open the code belows, it would happen "Too many open files" unexpected bugs. 
				do{
					sprintf(tmpBuff1, "/var/run/wscd-%s-vxd.pid",iface_name);
					if(isFileExist(tmpBuff1))
					{
						pid=getPid_fromFile(tmpBuff1);
						if(pid != -1){
							sprintf(strPID, "%d", pid);
							RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
						}
						else
							break;
						unlink(tmpBuff1);
						sleep(1);					
					}										

				}while(find_pid_by_name("wscd") > 0);
				*/
#endif
        
				//RunSystemCmd("/proc/gpio", "echo", "0", NULL_STR);///is it need to do this for other interface??????except wps
			}
			
			do{
				if(isFileExist("/var/run/wscd-wlan0.pid"))
				{
					pid=getPid_fromFile("/var/run/wscd-wlan0.pid");
					if(pid != -1){
						sprintf(strPID, "%d", pid);
						RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
					}
					else
						break;
					unlink("/var/run/wscd-wlan0.pid");
					sleep(1);					
				}
				else if(isFileExist("/var/run/wscd-wlan1.pid"))
				{
					pid=getPid_fromFile("/var/run/wscd-wlan1.pid");
					if(pid != -1){
						sprintf(strPID, "%d", pid);
						RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
					}
					else
						break;
					unlink("/var/run/wscd-wlan1.pid");
					sleep(1);					
				}
				else if(isFileExist("/var/run/wscd-wlan0-wlan1.pid"))
				{
					pid=getPid_fromFile("/var/run/wscd-wlan0-wlan1.pid");
					if(pid != -1){
						sprintf(strPID, "%d", pid);
						RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
					}
					else
						break;
					unlink("/var/run/wscd-wlan0-wlan1.pid");
					sleep(1);					
				}
				else
					break;
			}while(find_pid_by_name("wscd") > 0);
		}	
		token = strtok_r(NULL, " ", &savestr1);
	}while(token !=NULL);
	
	if(isFileExist(IAPP_PID_FILE)){
		pid=getPid_fromFile(IAPP_PID_FILE);
		if(pid != -1){
			sprintf(strPID, "%d", pid);
			RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
		}
		unlink(IAPP_PID_FILE);
	}

#ifdef WLAN_HS2_CONFIG
    if(isFileExist("/tmp/hs2_pidname")){
        FILE *fp;
        char line[100];
        fp = fopen("/tmp/hs2_pidname", "r");
        if (fp != NULL) {
            fgets(line, 100, fp);
            fclose(fp);
            //line[strlen(line)-1]='\0';
            if(isFileExist(line)){
                pid = getPid_fromFile(line);
                if (pid != -1){
                    printf("kill hs2:%d\n",pid);
                    sprintf(strPID, "%d", pid);
                    RunSystemCmd(NULL_FILE, "kill", "-9", strPID, NULL_STR);
                }
                unlink(line);
            }
        }
		unlink("/tmp/hs2_pidname");
    }
#endif
	//RunSystemCmd(NULL_FILE, "rm", "-f", "/var/*.fifo", NULL_STR);
	system("rm -f /var/*.fifo");
	if(!strcmp(action, "kill"))
		return 0;
	printf("Init Wlan application...\n");	
	//get root setting first//no this operate in script
	if(SetWlan_idx("wlan0")){
		apmib_get( MIB_WLAN_WLAN_DISABLED, (void *)&wlan_disabled_root);
		apmib_get( MIB_WLAN_MODE, (void *)&wlan_mode_root); 
		apmib_get( MIB_WLAN_IAPP_DISABLED, (void *)&wlan_iapp_disabled_root);
		apmib_get( MIB_WLAN_WSC_DISABLE, (void *)&wlan_wsc_disabled_root);
		apmib_get( MIB_WLAN_ENABLE_1X, (void *)&wlan_1x_enabled_root);
		apmib_get( MIB_WLAN_ENCRYPT, (void *)&wlan_encrypt_root);
		

#ifdef CONFIG_RTL_WAPI_SUPPORT
		apmib_get(MIB_WLAN_WAPI_AUTH, (void *)&wlan_wapi_auth_root);
#endif
		
		apmib_get( MIB_WLAN_MAC_AUTH_ENABLED, (void *)&wlan_mac_auth_enabled_root);
		apmib_get( MIB_WLAN_NETWORK_TYPE, (void *)&wlan_network_type_root);
		apmib_get( MIB_WLAN_WPA_AUTH, (void *)&wlan_wpa_auth_root);
		apmib_get( MIB_WLAN_WSC_UPNP_ENABLED, (void *)&WSC_UPNP_Enabled);

#ifdef CONFIG_RTL_P2P_SUPPORT							
		apmib_get( MIB_WLAN_P2P_TYPE, (void *)&p2p_mode); 
#endif							


		// For WAPI.now not support  VAP
//		apmib_get(MIB_WLAN_WAPI_AUTH, (void *)&wlan_wapi_auth);
//		memset(wlan_wapi_asipaddr,0x00,sizeof(wlan_wapi_asipaddr));
//		apmib_get(MIB_WLAN_WAPI_ASIPADDR,  (void*)wlan_wapi_asipaddr);
		
	}

	token=NULL;
	savestr1=NULL;
	sprintf(arg_buff, "%s", all_wlan_interface);
	token = strtok_r(arg_buff," ", &savestr1);
	do{
		_enable_1x=0;
		_use_rs=0;

		if (token == NULL){/*check if the first arg is NULL*/
			break;
		}else{                
			sprintf(iface_name, "%s", token); 
			if(strncmp(iface_name, "wlan", 4)==0){//wlan iface   
					
				if(strlen(iface_name)>=9){
					wlan_vap[0]=iface_name[6];
					wlan_vap[1]=iface_name[7];	
				}else{
					wlan_vap[0]=0;
				}
				
				if(SetWlan_idx(iface_name)){
					
					apmib_get( MIB_WLAN_WLAN_DISABLED, (void *)&wlan_disabled);
					apmib_get( MIB_WLAN_MODE, (void *)&wlan_mode); 
					apmib_get( MIB_WLAN_WDS_ENABLED, (void *)&wlan_wds_enabled);
					apmib_get( MIB_WLAN_WDS_NUM, (void *)&wlan_wds_num);
					apmib_get( MIB_WLAN_ENCRYPT, (void *)&wlan_encrypt);
					apmib_get( MIB_WLAN_WPA_AUTH, (void *)&wlan_wpa_auth);

#ifdef WLAN_HS2_CONFIG
					//printf("iface_name=%s\nwlan_disabled=%d\nwlan_encrypt=%d\nwlan_wpa_auth=%d\n",iface_name,wlan_disabled,wlan_encrypt,wlan_wpa_auth);
					
					if (!strcmp(iface_name, "wlan0")) {
						if (wlan0_hs2_conf_enable == 0)
							wlan0_hs2_enable = 0;
						else if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1)) 
							wlan0_hs2_enable = 1;
						else
							wlan0_hs2_enable = 0;
					}
					else if (!strcmp(iface_name, "wlan0-va0")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan0_va0_hs2_enable = 0;
                        else
                            wlan0_va0_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan0-va1")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan0_va1_hs2_enable = 0;
                        else
                            wlan0_va1_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan0-va2")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan0_va2_hs2_enable = 0;
                        else
                            wlan0_va2_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan0-va3")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan0_va3_hs2_enable = 0;
                        else
                            wlan0_va3_hs2_enable = 0;
                    }					
					else if (!strcmp(iface_name, "wlan1")) {
                        if (wlan1_hs2_conf_enable == 0)
							wlan1_hs2_enable = 0;
						else if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan1_hs2_enable = 1;
                        else
                            wlan1_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan1-va0")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan1_va0_hs2_enable = 0;
                        else
                            wlan1_va0_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan1-va1")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan1_va1_hs2_enable = 0;
                        else
                            wlan1_va1_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan1-va2")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan1_va2_hs2_enable = 0;
                        else
                            wlan1_va2_hs2_enable = 0;
                    }
					else if (!strcmp(iface_name, "wlan1-va3")) {
                        if ((wlan_disabled == 0) && (wlan_encrypt == 4) && (wlan_wpa_auth == 1))
                            wlan1_va3_hs2_enable = 0;
                        else
                            wlan1_va3_hs2_enable = 0;
                    }
					else
						printf("unknown hs2 iface name:%s\n", iface_name);
#endif
					
					if(wlan_disabled==0 && wlan_mode >3){
						apmib_get( MIB_WLAN_MESH_ENCRYPT, (void *)&wlan_mesh_encrypt);
					}
					if(wlan_disabled==0 && (wlan_mode ==2 || wlan_mode ==3) && (wlan_wds_enabled !=0) &&(wlan_wds_num!=0)){
						apmib_get( MIB_WLAN_WDS_ENCRYPT, (void *)&wlan_wds_encrypt);
						if(wlan_wds_encrypt==3 || wlan_wds_encrypt==4){
							sprintf(tmpBuff, "/var/wpa-wds-%s.conf",iface_name);//encrytp conf file
							RunSystemCmd(NULL_FILE, "flash", "wpa", iface_name, tmpBuff, "wds", NULL_STR); 
							RunSystemCmd(NULL_FILE, "auth", iface_name, bridge_iface, "wds", tmpBuff, NULL_STR); 
							sprintf(tmpBuff1, "/var/run/auth-%s.pid",iface_name);//auth pid file
							check_cnt = 0;
							do{
								if(isFileExist(tmpBuff1)){//check pid file is exist or not
									break;
								}else{
									sleep(1);
								}
								check_cnt++;
							}while(check_cnt < MAX_CHECK_PID_NUM);
						}
						
					}
 
					if(wlan_encrypt < 2){
						apmib_get( MIB_WLAN_ENABLE_1X, (void *)&wlan_1x_enabled);
						apmib_get( MIB_WLAN_MAC_AUTH_ENABLED, (void *)&wlan_mac_auth_enabled);
						if(wlan_1x_enabled != 0 || wlan_mac_auth_enabled != 0){
							_enable_1x=1;
							_use_rs=1;
						}
					}else{
						if(wlan_encrypt != 7){	// not wapi
							_enable_1x=1;
							if(wlan_wpa_auth ==1){
								_use_rs=1;
							}
						}
					}
					
					////////for mesh start
					if(wlan_disabled==0 && wlan_mode >3){
						if(wlan_mesh_encrypt != 0){
							
							sprintf(tmpBuff, "/var/wpa-%s-msh0.conf",iface_name);//encrytp conf file
							RunSystemCmd(NULL_FILE, "flash", "wpa", iface_name, tmpBuff, "msh", NULL_STR); 
							RunSystemCmd(NULL_FILE, "auth", iface_name, bridge_iface,"msh", tmpBuff, NULL_STR); 
							sprintf(tmpBuff1, "/var/run/auth-%s.pid",iface_name);//auth pid file
							check_cnt = 0;
							do{
								if(isFileExist(tmpBuff1)){//check pid file is exist or not
									break;
								}else{
									sleep(1);
								}
								check_cnt++;
							}while(check_cnt < MAX_CHECK_PID_NUM);
							
							
						}
					}
					
					///////for mesh end
					if(_enable_1x !=0 && wlan_disabled==0){
						
						sprintf(tmpBuff, "/var/wpa-%s.conf",iface_name);//encrytp conf file
						
						RunSystemCmd(NULL_FILE, "flash", "wpa", iface_name, tmpBuff, NULL_STR); 
						if(wlan_mode==1){//client mode
							apmib_get( MIB_WLAN_NETWORK_TYPE, (void *)&wlan_network_type);
							if(wlan_network_type==0){
								sprintf(wlan_role, "%s", "client-infra");
							}else{
								sprintf(wlan_role, "%s", "client-adhoc");
							}
						}else{
							sprintf(wlan_role, "%s", "auth");
						}
						
						if(wlan_vap[0]=='v' && wlan_vap[1]=='a'){
							if(wlan_mode_root != 0 && wlan_mode_root != 3){
								vap_not_in_pure_ap_mode=1;
							}
						}
						if(wlan_mode != 2 && vap_not_in_pure_ap_mode==0){
							
							if(wlan_wpa_auth != 2 || _use_rs !=0 ){
								deamon_created=1;
								RunSystemCmd(NULL_FILE, "auth", iface_name, bridge_iface, wlan_role, tmpBuff, NULL_STR); 
								
								if(wlan_vap[0]=='v' && wlan_vap[1]=='a')
									wlan_vap_auth_enable=1;
								else
									wlan_root_auth_enable=1;
							}
						} 
					}
					
					if(wlan_vap[0]=='v' && wlan_vap[1]=='x' && wlan_disabled==0){
						if(strcmp(wlan_role, "auth") || (!strcmp(wlan_role, "auth") && (_use_rs !=0)))
						{
							strcat(wlan_vxd," ");
							strcat(wlan_vxd,iface_name);
						}
					}
					
					if(wlan_vap[0]=='v' && wlan_vap[1]=='a'){
						if(wlan_disabled==0){
							if(wlan_iapp_disabled_root==0 || wlan_vap_auth_enable==1){
								if(valid_wlan_interface[0]==0){
									sprintf(valid_wlan_interface, "%s",iface_name); 
								}else{
									sprintf(tmp_iface, " %s", iface_name);
									strcat(valid_wlan_interface, tmp_iface);
								}
							}
						}
					}else{
						if(wlan_vap[0] !='v' && wlan_vap[1] !='x'){
							apmib_get( MIB_WLAN_IAPP_DISABLED, (void *)&wlan_iapp_disabled);
							apmib_get( MIB_WLAN_WSC_DISABLE, (void *)&wlan_wsc_disabled); 
							if(wlan_root_auth_enable==1 || wlan_iapp_disabled==0 || wlan_wsc_disabled==0){
								if(valid_wlan_interface[0]==0){
									sprintf(valid_wlan_interface, "%s",iface_name); 
								}else{
									sprintf(tmp_iface, " %s", iface_name);
									strcat(valid_wlan_interface, tmp_iface);
								}
							}
						}
					}
						
						if((wlan_vap[0] !='v' && wlan_vap[1] !='a') && (wlan_vap[0] !='v' && wlan_vap[1] !='x')){
							 if(wlan_disabled==0)
							 	isWLANEnabled=1;
							 if(wlan_mode ==0 || wlan_mode ==3 || wlan_mode ==4 || wlan_mode ==6)
							 	isAP=1;
							 if(wlan_iapp_disabled==0)
							 	isIAPPEnabled=1;
						}
				}	
			}
		}
		token = strtok_r(NULL, " ", &savestr1);
	}while(token !=NULL);
		
	if(isWLANEnabled==1 && isAP==1 && isIAPPEnabled==1){
#if defined(CONFIG_RTL_ULINKER)
		//fixme: disable iapp temporary
#else
		token=NULL;
		savestr1=NULL;	     
		sprintf(arg_buff, "%s", valid_wlan_interface);
		token = strtok_r(arg_buff," ", &savestr1);
		do{
			if (token == NULL)
				break;
			if(!strcmp(token, "wlan0") //root if
				|| !strcmp(token, "wlan1")){
				SetWlan_idx(token);
				apmib_get( MIB_WLAN_MODE, (void *)&wlan_mode_root);
				apmib_get( MIB_WLAN_IAPP_DISABLED,(void *)&wlan_iapp_disabled_root);
				if((wlan_mode_root ==0 || wlan_mode_root ==3 || wlan_mode_root ==4 || wlan_mode_root ==6) && wlan_iapp_disabled_root==0){
					if(iapp_interface[0]==0){
						sprintf(iapp_interface, "%s",token); 
					}else{
						sprintf(tmp_iface, " %s", token);
						strcat(iapp_interface, tmp_iface);
					}
				}
					
			}
			else{
				if(iapp_interface[0]==0){
					sprintf(iapp_interface, "%s",token); 
				}else{
					sprintf(tmp_iface, " %s", token);
					strcat(iapp_interface, tmp_iface);
				}
			}
			token = strtok_r(NULL, " ", &savestr1);
		}while(token != NULL);
		
		sprintf(tmpBuff, "iapp %s %s",bridge_iface, iapp_interface);
		system(tmpBuff);
		
		deamon_created=1;
        if(isFileExist(RESTART_IAPP))
            unlink(RESTART_IAPP);
        RunSystemCmd(RESTART_IAPP, "echo", tmpBuff, NULL_STR);
#endif
	}
	
//for mesh========================================================
#if defined(CONFIG_RTK_MESH)
	SetWlan_idx("wlan0");
	apmib_get(MIB_WLAN_MODE, (void *)&wlan0_mode);
	apmib_get(MIB_WLAN_MESH_ENABLE,(void *)&wlan0_mesh_enabled);
    if(wlan0_mode != AP_MESH_MODE && wlan0_mode != MESH_MODE) {
        wlan0_mesh_enabled = 0;
    }

#if defined(FOR_DUAL_BAND)
    SetWlan_idx("wlan1");
    apmib_get(MIB_WLAN_MODE, (void *)&wlan1_mode);
    apmib_get(MIB_WLAN_MESH_ENABLE,(void *)&wlan1_mesh_enabled);
    if(wlan1_mode != AP_MESH_MODE && wlan1_mode != MESH_MODE){
        wlan1_mesh_enabled = 0;
    }
#endif

    #ifdef CONFIG_RTL_MESH_CROSSBAND
    if(wlan0_mesh_enabled) {
        system("pathsel -i wlan0 -P -d");
    }

    if(wlan1_mesh_enabled) {
        system("pathsel -i wlan1 -P -d");
    }
    #else
	if(wlan0_mesh_enabled || wlan1_mesh_enabled){
        system("pathsel -i wlan-msh -P -d");
	}
    #endif

#endif

//========================================================
//for HS2
#ifdef WLAN_HS2_CONFIG
	printf("hs2_wlan0_enable= %d,%d,%d,%d,%d\n", wlan0_hs2_enable, wlan0_va0_hs2_enable, wlan0_va1_hs2_enable, wlan0_va2_hs2_enable, wlan0_va3_hs2_enable);
	printf("hs2_wlan1_enable= %d,%d,%d,%d,%d\n", wlan1_hs2_enable, wlan1_va0_hs2_enable, wlan1_va1_hs2_enable, wlan1_va2_hs2_enable, wlan1_va3_hs2_enable);

	int isEnableHS2 = (wlan0_hs2_enable || wlan1_hs2_enable)?1:0;
	if(isEnableHS2) {
		system("killall -9 hs2 2> /dev/null");
		if (isFileExist("/bin/hs2")) {		
			//memset(tmpBuff, 0x00, 100);
			memset(cmd_opt, 0x00, 16);
	        cmd_cnt=0;
	        cmd_opt[cmd_cnt++] = "hs2";

			strcat(tmpBuff, "hs2 ");
			if (isFileExist("/etc/hs2_wlan0.conf") && (wlan0_hs2_enable == 1)) {			
				//strcat(tmpBuff, "-c /tmp/hs2_wlan0.conf ");			
				cmd_opt[cmd_cnt++] = "-c";
				cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0.conf";
				//cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0.conf";
			}	
			if (isFileExist("/etc/hs2_wlan0_va0.conf") && (wlan0_va0_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va0.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0_va0.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan0_va1.conf") && (wlan0_va1_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va1.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0_va1.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan0_va2.conf") && (wlan0_va2_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va2.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0_va2.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan0_va3.conf") && (wlan0_va3_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va3.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan0_va3.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan1.conf") && (wlan1_hs2_enable == 1)) {
				//strcat(tmpBuff, "-c /tmp/hs2_wlan0.conf ");			
				cmd_opt[cmd_cnt++] = "-c";
				cmd_opt[cmd_cnt++] = "/etc/hs2_wlan1.conf";
			}	
			if (isFileExist("/etc/hs2_wlan1_va0.conf") && (wlan1_va0_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va0.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan1_va0.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan1_va1.conf") && (wlan1_va1_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va1.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan1_va1.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan1_va2.conf") && (wlan1_va2_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va2.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan1_va2.conf";
	        }   
			if (isFileExist("/etc/hs2_wlan1_va3.conf") && (wlan1_va3_hs2_enable == 1)) {
	            //strcat(tmpBuff, "-c /tmp/hs2_wlan0_va3.conf ");         
	            cmd_opt[cmd_cnt++] = "-c";
	            cmd_opt[cmd_cnt++] = "/etc/hs2_wlan1_va3.conf";
	        }   
			//printf("hs2 cmd==> %s\n", tmpBuff);
			//system(tmpBuff);
			cmd_opt[cmd_cnt++] = 0;		
	        DoCmd(cmd_opt, NULL_FILE);                                                    	
			wait_fifo=3;
			do{
				if(isFileExist("/tmp/hs2_pidname")){//check pid file is exist or not
					break;
	            } 
				else {
					wait_fifo--;
					sleep(1);
	            }
	        }while(wait_fifo != 0);
		}
		else 
			printf("/bin/hs2 do not exist\n");
		
		system("telnetd"); // for sigma
	}
#endif

//========================================================
//for WPS
	if (isFileExist("/bin/wscd")) {
		memset(tmpBuff, 0x00, 100);
		memset(tmpBuff1, 0x00, 100);
		token=NULL;
		savestr1=NULL;	     
		sprintf(arg_buff, "%s", valid_wlan_interface);
		
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)
		

//printf("\r\n isRptEnabled1=[%d],__[%s-%u]\r\n",isRptEnabled1,__FILE__,__LINE__);
//printf("\r\n wlan0_wsc_disabled_vxd=[%d],__[%s-%u]\r\n",wlan0_wsc_disabled_vxd,__FILE__,__LINE__);
//printf("\r\n isRptEnabled2=[%d],__[%s-%u]\r\n",isRptEnabled2,__FILE__,__LINE__);
//printf("\r\n wlan1_wsc_disabled_vxd=[%d],__[%s-%u]\r\n",wlan1_wsc_disabled_vxd,__FILE__,__LINE__);

		memset(wlan_vxd, 0x00, sizeof(wlan_vxd));
		if(isRptEnabled1 == 1 && wlan_wsc_disabled_root == 0			
#if defined(CONFIG_RTL_ULINKER)
			&& wlan_mode_root != CLIENT_MODE
#endif			
#if defined(CONFIG_ONLY_SUPPORT_CLIENT_REPEATER_WPS)
			&& wlan_mode_root == CLIENT_MODE
#endif			
		)
		{
			sprintf(wlan_vxd, "%s", "wlan0-vxd");
		}

#if defined(CONFIG_RTL_92D_SUPPORT)
		if(isRptEnabled2 == 1 && wlan_wsc1_disabled == 0
#if defined(CONFIG_RTL_ULINKER)
			&& wlan_mode_root != CLIENT_MODE
#endif			
#if defined(CONFIG_ONLY_SUPPORT_CLIENT_REPEATER_WPS)
			&& wlan_mode_root == CLIENT_MODE
#endif			
		)
		{			
			strcat(wlan_vxd, " wlan1-vxd");
		}
#endif

		sprintf(tmpBuff," %s",wlan_vxd);
		strcat(arg_buff, tmpBuff);
#endif	//#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)			

//printf("\r\n arg_buff=[%s],__[%s-%u]\r\n",arg_buff,__FILE__,__LINE__);

		token = strtok_r(arg_buff," ", &savestr1);
			
		//token = strtok_r(valid_wlan_interface," ", &savestr1);
		do{
			if (token == NULL){
				break;
			}else{
				unsigned char wscConfFile[40];
				unsigned char wscFifoFile[40];
				memset(wscConfFile, 0x00, sizeof(wscConfFile));
				memset(wscFifoFile, 0x00, sizeof(wscFifoFile));

				_enable_1x=0;
				wps_debug=0;
				WSC=1;
				use_iwcontrol=1;
				
				if(!strcmp(token, "wlan0") //root if
					|| !strcmp(token, "wlan1") 
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)
					|| strstr(token, "-vxd")
#endif					
				)
				{
#if 1 //defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)
					//if(strcmp(token, "wlan0-vxd") == 0) // here we ONLY get vxd mib value 
					{						
						SetWlan_idx(token);
						
						//apmib_get( MIB_WLAN_DISABLED, (void *)&wlan_disabled_root);
						apmib_get( MIB_WLAN_MODE, (void *)&wlan_mode_root);
						if(!strcmp(token, "wlan1") && (both_band_ap == 1))
						{
								token = strtok_r(NULL, " ", &savestr1);
								continue;
						}
						//apmib_get( MIB_WSC_DISABLE, (void *)&wlan_wsc_disabled_root);
						apmib_get( MIB_WLAN_ENABLE_1X, (void *)&wlan_1x_enabled_root);						
						apmib_get( MIB_WLAN_ENCRYPT, (void *)&wlan_encrypt_root);					
						
#ifdef CONFIG_RTL_WAPI_SUPPORT
						apmib_get(MIB_WLAN_WAPI_AUTH, (void *)&wlan_wapi_auth_root);
#endif
						apmib_get( MIB_WLAN_MAC_AUTH_ENABLED, (void *)&wlan_mac_auth_enabled_root);
						//apmib_get( MIB_WLAN_NETWORK_TYPE, (void *)&wlan_network_type_root);
						apmib_get( MIB_WLAN_WPA_AUTH, (void *)&wlan_wpa_auth_root);
						apmib_get( MIB_WLAN_WSC_UPNP_ENABLED, (void *)&WSC_UPNP_Enabled);
						
//fprintf(stderr,"\r\n WSC_UPNP_Enabled=[%d],__[%s-%u]",WSC_UPNP_Enabled,__FILE__,__LINE__);						
						wlan_disabled_root = 0;
						wlan_network_type_root = 0;
					}
#endif					
					if(wlan_encrypt_root < 2){ //ENCRYPT_DISABLED=0, ENCRYPT_WEP=1, ENCRYPT_WPA=2, ENCRYPT_WPA2=4, ENCRYPT_WPA2_MIXED=6 ,ENCRYPT_WAPI=7
						
						if(wlan_1x_enabled_root != 0 || wlan_mac_auth_enabled_root !=0)
							_enable_1x=1;
					}else{
						if(wlan_encrypt_root != 7)	//not wapi
							_enable_1x=1;
					}
						
					if(!strcmp(token, "wlan0") && ((wlan_wsc_disabled_root != 0) || (wlan_disabled_root != 0) || (wlan_mode_root == 2)||(wlan0_hidden_ssid_enabled))){
							WSC=0;
					}

#if defined(CONFIG_RTL_92D_SUPPORT)					
					if(!strcmp(token, "wlan1") && ((wlan_wsc1_disabled != 0) || (wlan1_disabled_root != 0) || (wlan_mode_root == 2)||(wlan1_hidden_ssid_enabled))){
							WSC=0;
					}
#endif
					
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT)					
					else if(!strcmp(token, "wlan0-vxd") && (wlan0_wsc_disabled_vxd != 0))
					{
							WSC=0;
					}
#if defined(CONFIG_RTL_92D_SUPPORT)					
					else if(!strcmp(token, "wlan1-vxd") && (wlan1_wsc_disabled_vxd != 0))
					{
							WSC=0;
					}
#endif					
#endif
					else{
						
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT) && defined(CONFIG_WPS_EITHER_AP_OR_VXD)
							if(WSC == 1 && strlen(token) == 5 && !strcmp(token, "wlan0") && isRptEnabled1 == 1)
							{
									WSC=0;
							}
							
#if defined(CONFIG_RTL_92D_SUPPORT)												
							if(WSC == 1 && strlen(token) == 5 && !strcmp(token, "wlan1") && isRptEnabled2 == 1)
							{
									WSC=0;
							}
  #endif
#endif
#if defined(UNIVERSAL_REPEATER) && defined(CONFIG_REPEATER_WPS_SUPPORT) && defined(CONFIG_WPS_1665_REPEATER_WIZARD)
							//joe add for 1665 repeater wps setting
							/*if(WSC == 1 && strlen(token) == 5 && !strcmp(token, "wlan0") && isFileExist("/var/wps_start_pbc") && (isRptEnabled1 || isRptEnabled2) )
							{
									WSC=0;
							}
							if(WSC == 1 && strlen(token) == 5 && !strcmp(token, "wlan1") && isFileExist("/var/wps_start_pbc_wlan1") && (isRptEnabled1 || isRptEnabled2) )
							{
									WSC=0;
							}*/
							if(WSC == 1 && strlen(token) == 9 && !strcmp(token, "wlan0-vxd") )
							{
									both_band_ap = 0;WSC=0;
							}
							if(WSC == 1 && strlen(token) == 9 && !strcmp(token, "wlan1-vxd") )
							{
									both_band_ap = 0;WSC=0;
							}
							if(WSC == 1 && strlen(token) == 9 && !strcmp(token, "wlan0-vxd") && (!isFileExist("/var/wps_start_pbc")))
							{
									WSC=0;
							}
							if(WSC == 1 && strlen(token) == 9 && !strcmp(token, "wlan1-vxd") && !(isFileExist("/var/wps_start_pbc_wlan1")))
							{
									WSC=0;
							}
#endif
							if(WSC == 1 && wlan_mode_root ==1){
								if(wlan_network_type_root != 0)
									WSC=0;
							}
							if(WSC == 1 && wlan_mode_root ==0){
								if(wlan_encrypt_root < 2 && _enable_1x !=0 )
									WSC=0;		
								if(wlan_encrypt_root >= 2 && wlan_encrypt_root != 7 && wlan_wpa_auth_root ==1 )
									WSC=0;
#ifdef CONFIG_RTL_WAPI_SUPPORT
									if(wlan_encrypt_root == 7 && wlan_wapi_auth_root == 1)
										WSC=0;
#endif
								}
						
					}						
						
					if(WSC==1){ //start wscd 
						//For win7 logo, WEP mode on WPS
						apmib_get( MIB_WLAN_WEP_DEFAULT_KEY, (void *)&set_val );
						if (set_val > 3){
							set_val = 0;
							apmib_set( MIB_WLAN_WEP_DEFAULT_KEY, (void *)&set_val );
							//apmib_update(CURRENT_SETTING);
						}
						apmib_get( MIB_WLAN_WSC_CONFIGBYEXTREG, (void *)&set_val);
						if (set_val == 2)
							set_val = 1;
						else
							set_val = 0;
						apmib_set(MIB_WLAN_WSC_MANUAL_ENABLED, (void *)&set_val);
						apmib_update(CURRENT_SETTING);
						// For win7 logo
						
						memset(cmd_opt, 0x00, 16);
						cmd_cnt=0;
						cmd_opt[cmd_cnt++] = "wscd";
						if(isFileExist("/var/wps/simplecfgservice.xml")==0){ //file is not exist
							if(isFileExist("/var/wps"))
								RunSystemCmd(NULL_FILE, "rm", "/var/wps", "-rf", NULL_STR);
							RunSystemCmd(NULL_FILE, "mkdir", "/var/wps", NULL_STR); 
							system("cp /etc/simplecfg*.xml /var/wps");
						}
						if(wlan_mode_root ==1 
#ifdef CONFIG_RTL_P2P_SUPPORT
							|| (wlan_mode_root ==8 && (p2p_mode == P2P_DEVICE || p2p_mode == P2P_CLIENT))
#endif							
							){
							WSC_UPNP_Enabled=0;
							cmd_opt[cmd_cnt++] = "-mode";
							cmd_opt[cmd_cnt++] = "2";
						}else{
							cmd_opt[cmd_cnt++] = "-start";
						}
						if(WSC_UPNP_Enabled==1){
							RunSystemCmd(NULL_FILE, "route", "del", "-net", "239.255.255.250", "netmask", "255.255.255.255", bridge_iface, NULL_STR); 
							RunSystemCmd(NULL_FILE, "route", "add", "-net", "239.255.255.250", "netmask", "255.255.255.255", bridge_iface, NULL_STR); 
						}


						if(both_band_ap == 1)
						{	
							if(!strstr(token, "vxd"))
								cmd_opt[cmd_cnt++] = "-both_band_ap";
#if  defined(CONFIG_TRIGGER_TWO_VXD_WPS)
							else if(vxd_client_num == 2)
								cmd_opt[cmd_cnt++] = "-both_vxd_client";
#endif
						}
						
						sprintf(wscConfFile,"/var/wsc-%s.conf",token);
						RunSystemCmd(NULL_FILE, "flash", "upd-wsc-conf", "/etc/wscd.conf", wscConfFile, token, NULL_STR); 


						cmd_opt[cmd_cnt++] = "-c";
						#ifdef CONFIG_RTL_COMAPI_CFGFILE
						  #if !defined(CONFIG_RTL_819X)
						    #define WSC_CFG "/var/RTL8190N.dat"
						  #else
						    #define WSC_CFG "/var/RTL8192CD.dat"
						  #endif
						#else
						  #define WSC_CFG "/var/wsc.conf"
						#endif

						cmd_opt[cmd_cnt++] = wscConfFile;

						cmd_opt[cmd_cnt++] = "-w";

						cmd_opt[cmd_cnt++] = token;

						if(wps_debug==1){
							/* when you would like to open debug, you should add define in wsc.h for debug mode enable*/
							cmd_opt[cmd_cnt++] = "-debug";
						}
						if(use_iwcontrol==1){
							cmd_opt[cmd_cnt++] = "-fi";

							sprintf(wscFifoFile,"/var/wscd-%s.fifo",token);
							cmd_opt[cmd_cnt++] = wscFifoFile;

							deamon_created=1;
						}
						if(isFileExist("/var/wps_start_pbc")){
							cmd_opt[cmd_cnt++] = "-start_pbc";
							unlink("/var/wps_start_pbc");
							system("ifconfig wlan0 up"); // wlan0 up for wizard wps
						}
						else if(isFileExist("/var/wps_start_pbc_wlan1")){
							cmd_opt[cmd_cnt++] = "-start_pbc";
							unlink("/var/wps_start_pbc_wlan1"); //joe add for dual band wlan0, wlan1 pbc
							system("ifconfig wlan1 up");
						}
						
						if(isFileExist("/var/wps_start_pin")){
							cmd_opt[cmd_cnt++] = "-start";
							system("ifconfig wlan0 up");
							unlink("/var/wps_start_pin");
						}
						else if(isFileExist("/var/wps_start_pin_wlan1")){ //joe add for dual band wlan0, wlan1 pin
							cmd_opt[cmd_cnt++] = "-start";
							system("ifconfig wlan1 up");
							unlink("/var/wps_start_pin");
						}
						
						if(isFileExist("/var/wps_local_pin")){
							fp=fopen("/var/wps_local_pin", "r");
							if(fp != NULL){
								fscanf(fp, "%s", tmpBuff1);
								fclose(fp);
							}
							sprintf(wsc_pin_local, "%s", tmpBuff1);
							cmd_opt[cmd_cnt++] = "-local_pin";
							cmd_opt[cmd_cnt++] = wsc_pin_local;
							unlink("/var/wps_local_pin");
						}
						if(isFileExist("/var/wps_peer_pin")){
							fp=fopen("/var/wps_peer_pin", "r");
							if(fp != NULL){
								fscanf(fp, "%s", tmpBuff1);
								fclose(fp);
							}
							sprintf(wsc_pin_peer, "%s", tmpBuff1);
							cmd_opt[cmd_cnt++] = "-peer_pin";
							cmd_opt[cmd_cnt++] = wsc_pin_peer;
							unlink("/var/wps_peer_pin");
						}
						
						cmd_opt[cmd_cnt++] = "-daemon";
						
						cmd_opt[cmd_cnt++] = 0;
						//for (pid=0; pid<cmd_cnt;pid++)
							//printf("cmd index=%d, opt=%s \n", pid, cmd_opt[pid]);
						DoCmd(cmd_opt, NULL_FILE);
					}
					
					wait_fifo=5;
					do{

						if(isFileExist(wscFifoFile))
						{
							wait_fifo=0;
						}else{
							wait_fifo--;
							sleep(1);
						}
						
					}while(use_iwcontrol !=0 && wait_fifo !=0);		
				}
		
			}   
			token = strtok_r(NULL, " ", &savestr1);

		}while(token !=NULL);
		///////// Add for 1665 WSC LOCK file 20130703 /////////
		apmib_get( MIB_RESERVED_WORD_8, (void *)&set_val);
		if(set_val == 1)
		{
		  sprintf(tmpBuff, "echo 1 > /tmp/wscd_lock_stat");
		  system(tmpBuff);
		}
	}

	if(deamon_created==1){
		if(wlan_vxd[0]){
				sprintf(tmpBuff, "iwcontrol %s %s",valid_wlan_interface, wlan_vxd);
		}else{
				sprintf(tmpBuff, "iwcontrol %s",valid_wlan_interface);
		}
		system(tmpBuff);	

	}
	if(wlan_vxd[0]) // joe repeater test
	{
	  //printf("\n[%s][%d] iwpriv wlan0-vxd set_mib band=76\n\n",__FILE__,__LINE__);//joe debug
	  system("iwpriv wlan0-vxd set_mib band=76");
	}
/*for WAPI*/
	//first, to kill daemon related wapi-cert
	//in order to avoid multiple daemon existing
#ifdef CONFIG_RTL_WAPI_SUPPORT
#ifdef CONFIG_RTL_WAPI_LOCAL_AS_SUPPORT
	RunSystemCmd(NULL_FILE, "killall", "-9", "aseUdpServer", NULL_STR); 
#endif
	RunSystemCmd(NULL_FILE, "killall", "-9", "aeUdpClient", NULL_STR);

	///////////////////////////////
	//no these operations in script
	//should sync with WLAN_INTERFACE_LIST: "wlan0,wlan0-va0,wlan0-va1,wlan0-va2,wlan0-va3"
	//At first, check virtual wlan interface
	apAsAS=0;//Initial, note: as IP only need to be set once because all wlan interfaces use the same as IP setting
	memset(wapiconf,0x0,sizeof(WAPI_ASSERVER_CONF_T)*MAX_WAPI_CONF_NUM);
	apmib_get(MIB_WLAN_BAND2G5G_SELECT,(void *)&wlanBand2G5GSelect);	
	for(wlan_index=0; wlan_index<NUM_WLAN_INTERFACE; wlan_index++)
	{
		if((wlanBand2G5GSelect!=BANDMODEBOTH)&&(wlan_index>0))
			break;
			
		sprintf(wlan_name,"wlan%d",wlan_index);
		for(i=0;i<4;i++)
		{
			memset(wlan_vname,0,sizeof(wlan_vname));
			sprintf(wlan_vname, "%s-va%d",wlan_name,i);
			if(SetWlan_idx(wlan_vname)){
				apmib_get( MIB_WLAN_ENCRYPT, (void *)&wlan_encrypt_virtual);
				apmib_get(MIB_WLAN_WAPI_AUTH, (void *)&wlan_wapi_auth);
				memset(wlan_wapi_asipaddr,0x00,sizeof(wlan_wapi_asipaddr));
				apmib_get(MIB_WLAN_WAPI_ASIPADDR,  (void*)wlan_wapi_asipaddr);
				apmib_get(MIB_WLAN_WAPI_CERT_SEL,  (void*)&wlan_wapi_cert_sel);
				apmib_get(MIB_WLAN_WLAN_DISABLED,(void *)&wlan_disabled);
			}
	//		printf("%s(%d): wlan_vname(%s), wlan_encrypt_virtual(%d), wlan_wapi_auth(%d), wlan_wapi_asipaddr(%s)\n",
	//			__FUNCTION__,__LINE__,wlan_vname, wlan_encrypt_virtual,wlan_wapi_auth,inet_ntoa(*((struct in_addr *)wlan_wapi_asipaddr)));//Added for test
			if(wlan_encrypt_virtual == 7 && wlan_disabled==0){
				if(wlan_wapi_auth == 1){
#ifdef CONFIG_RTL_WAPI_LOCAL_AS_SUPPORT
					if(!apAsAS){
						apmib_get(MIB_IP_ADDR,  (void*)tmpBuff1);
						if(!memcmp(wlan_wapi_asipaddr, tmpBuff1, 4)){
							apAsAS=1;
						}
					}
#endif
					if(asIpExist(wapiconf, (unsigned char)wlan_wapi_cert_sel,wlan_wapi_asipaddr,&index)){
						if(wapiconf[index].valid){
							strcat(wapiconf[index].network_inf,",");
							strcat(wapiconf[index].network_inf,wlan_vname);
						}
					}else {
						memcpy(wapiconf[index].wapi_asip,wlan_wapi_asipaddr,4);
						strcpy(wapiconf[index].network_inf,wlan_vname);
						wapiconf[index].wapi_cert_sel=wlan_wapi_cert_sel;
						wapiconf[index].valid=1;
					}
				}
			}
		}
		////////////////////////////////////

		//At last, check root wlan interface
		if(SetWlan_idx(wlan_name)){
			apmib_get( MIB_WLAN_ENCRYPT, (void *)&wlan_encrypt_root);
			apmib_get(MIB_WLAN_WAPI_AUTH, (void *)&wlan_wapi_auth);
			memset(wlan_wapi_asipaddr,0x00,sizeof(wlan_wapi_asipaddr));
			apmib_get(MIB_WLAN_WAPI_ASIPADDR,  (void*)wlan_wapi_asipaddr);
			apmib_get(MIB_WLAN_WAPI_CERT_SEL,  (void*)&wlan_wapi_cert_sel);
			apmib_get(MIB_WLAN_WLAN_DISABLED,(void *)&wlan_disabled);
		}
	//	printf("%s(%d): wlan0, wlan_encrypt_root(%d), wlan_wapi_auth(%d), wlan_wapi_asipaddr(%s)\n",
	//		__FUNCTION__,__LINE__,wlan_encrypt_root,wlan_wapi_auth,inet_ntoa(*((struct in_addr *)wlan_wapi_asipaddr)));//Added for test
		if(wlan_encrypt_root == 7 && wlan_disabled==0){
			if(wlan_wapi_auth == 1){
#ifdef CONFIG_RTL_WAPI_LOCAL_AS_SUPPORT
				if(!apAsAS){
					apmib_get(MIB_IP_ADDR,  (void*)tmpBuff1);
					if(!memcmp(wlan_wapi_asipaddr, tmpBuff1, 4)){		
						apAsAS=1;
					}
				}
#endif
				if(asIpExist(wapiconf, (unsigned char)wlan_wapi_cert_sel,wlan_wapi_asipaddr,&index)){
					if(wapiconf[index].valid){
						strcat(wapiconf[index].network_inf,",");
						strcat(wapiconf[index].network_inf,wlan_name);
					}
				}else {
					memcpy(wapiconf[index].wapi_asip,wlan_wapi_asipaddr,4);
					strcpy(wapiconf[index].network_inf,wlan_name);
					wapiconf[index].wapi_cert_sel=wlan_wapi_cert_sel;
					wapiconf[index].valid=1;
				}
			}
		}
	}
	
#ifdef CONFIG_RTL_WAPI_LOCAL_AS_SUPPORT
	if(apAsAS){
		system("aseUdpServer &");
	}
#endif

	for(index=0;index<MAX_WAPI_CONF_NUM;index++)
	{
		if(wapiconf[index].valid){
			sprintf(arg_buff,"aeUdpClient -d %s -i %s -s %d &", inet_ntoa(*((struct in_addr *)wapiconf[index].wapi_asip)), wapiconf[index].network_inf, wapiconf[index].wapi_cert_sel);
			system(arg_buff);
		}
	}	
#endif
  	system("echo 1 > /tmp/wps_ready");	

return 0;	
	
		
}

 
 
 
 
 
 
 
 
 
 
 
