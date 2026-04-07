/*
 *      Web server handler routines for TCP/IP stuffs
 *
 *      Authors: David Hsu	<davidhsu@realtek.com.tw>
 *
 *      $Id: fmtcpip.c,v 1.24 2009/08/24 10:31:08 bradhuang Exp $
 *
 */

/*-- System inlcude files --*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
/*-- Local inlcude files --*/
#include "boa.h"
#include "globals.h"
#include "apmib.h"
#include "apform.h"
#include "utility.h"
#include "mibtbl.h"
#include "asp_page.h"

#ifdef __i386__
#define _LITTLE_ENDIAN_
#endif

#define _DHCPD_PROG_NAME	"udhcpd"
#define _DHCPD_PID_PATH		"/var/run"
#define _DHCPC_PROG_NAME	"udhcpc"
#define _DHCPC_PID_PATH		"/etc/udhcpc"
#define _PATH_DHCPS_LEASES	"/var/lib/misc/udhcpd.leases"


/*-- Macro declarations --*/
#ifdef _LITTLE_ENDIAN_
#define ntohdw(v) ( ((v&0xff)<<24) | (((v>>8)&0xff)<<16) | (((v>>16)&0xff)<<8) | ((v>>24)&0xff) )

#else
#define ntohdw(v) (v)
#endif

#define RECONNECT_MSG(url) { \
	req_format_write(wp, "<html><body><blockquote><h4>Change setting successfully!<BR><BR>If IP address was modified, you have to re-connect the WebServer" \
		"<BR>with the new address.<BR><BR>" \
                "<form><input type=button value=\"  OK  \" OnClick=window.location.replace(\"%s\")></form></blockquote></body></html>", url);\
}

/*-- Forward declarations --*/
static int getOneDhcpClient(char **ppStart, unsigned long *size, char *name, char *ip, char *mac, char *liveTime);
//static int getOneDhcpClient(char **ppStart, unsigned long *size, char *name, char *ip, char *mac, char *liveTime);
#if 0
static DHCP_T wanDhcpTmp=(DHCP_T)-1;
#endif

//////////////////////////////////////////////////////////////////////////////
static int isValidName(char *str)
{
	int i, len=strlen(str);

	for (i=0; i<len; i++) {
		if (str[i] == ' ' || str[i] == '"' || str[i] == '\x27' || str[i] == '\x5c' || str[i] == '$' ||
		    str[i] == '/' || str[i] == '@' || str[i] == '#' || str[i] == '%' || str[i] == '&' ||
		    str[i] == '*' || str[i] == '(' || str[i] == ')' || str[i] == '+' || str[i] == '=' ||
		    str[i] == '{' || str[i] == '[' || str[i] == '}' || str[i] == ']' || str[i] == '?' ||
		    str[i] == '>' || str[i] == '<' || str[i] == ',' || str[i] == '~' || str[i] == '`' ||
		    str[i] == '!' || str[i] == '|')
			return 0;
	}
	if(str[0] == '0' || str[0] == '1' || str[0] == '2' || str[0] == '3' || str[0] == '4' || str[0] == '5' ||
	   str[0] == '6' || str[0] == '7' || str[0] == '8' || str[0] == '9')
	   return 0; 
	return 1;
}

//////////////////////////////////////////////////////////////////////////////
static int getOneDhcpClient(char **ppStart, unsigned long *size, char *name, char *ip, char *mac, char *liveTime)
{
	struct dhcpOfferedAddr {
        	u_int8_t chaddr[16];
        	u_int32_t yiaddr;       /* network order */
        	u_int32_t expires;      /* host order */
			char hostname[64];	/* host name */
			u_int32_t isUnAvailableCurr;
	};
	
	struct dhcpOfferedAddr entry;
	 u_int8_t empty_haddr[16]; 
    
     memset(empty_haddr, 0, 16);
	if ( *size < sizeof(entry))
		return -1;

	entry = *((struct dhcpOfferedAddr *)*ppStart);
	*ppStart = *ppStart + sizeof(entry);
	*size = *size - sizeof(entry);

	if (entry.expires == 0){
		return 0;
	}

	if(!memcmp(entry.chaddr, empty_haddr, 16)){
		//fprintf(stderr, "got a unavailable entry for ip=%s\n",inet_ntoa(*((struct in_addr *)&entry.yiaddr)));
		return 0;
	}

	if (entry.hostname[0])
		strcpy(name, (char *)&entry.hostname);
	else
		strcpy(name, "UnKnow");
	strcpy(ip, inet_ntoa(*((struct in_addr *)&entry.yiaddr)) );
	snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
			entry.chaddr[0],entry.chaddr[1],entry.chaddr[2],entry.chaddr[3],
			entry.chaddr[4], entry.chaddr[5]);
			
	//if(entry.expires == 0xffffffff)
    //    sprintf(liveTime,"%s", "Always");
    //    else
		snprintf(liveTime, 10, "%lu", (unsigned long)ntohl(entry.expires));

	return 1;
}


///////////////////////////////////////////////////////////
int getPid(char *filename)
{
	struct stat status;
	char buff[100];
	FILE *fp;

	if ( stat(filename, &status) < 0)
		return -1;
	fp = fopen(filename, "r");
	if (!fp) {
        	fprintf(stderr, "Read pid file error!\n");
		return -1;
   	}
	fgets(buff, 100, fp);
	fclose(fp);

	return (atoi(buff));
}
int tcpipLanHandler(request *wp, char *tmpBuf)
{
	char	*strIp, *strMask, *strGateway, *strDhcp, *strStp, *strMac, *strDNS, *strLeaseTime, *strDomain, *strDhcpLeaseTime, *strLantype, *strVal, *strWINS, *strDHCP_Broadcast;;
	struct in_addr inIp, inMask, inGateway;
	struct in_addr dhcpGateway, dhcpWINS, dhcpDNS;
	DHCP_T dhcp, curDhcp;	
	
	int stp, leaseTime ,tmp;
	char	*strdhcpRangeStart, *strdhcpRangeEnd;
	struct in_addr dhcpRangeStart, dhcpRangeEnd;
	struct in_addr dns1, dns2, dns3;	
	unsigned long gateway_t;	//John fix radius server problem for NULL gateway 2006.11.14 	
	unsigned long mask_t, ip_t, mask_all=0xffffffff;
	char str_ip[20];	//John fix dhcp server problem for ip change 2006.11.14
	unsigned long start, end, dhcp_gw;
	int call_from_wizard = 0;
	
#ifdef HOME_GATEWAY		//John Huang add for 7-mode 2007.02.28
	int opmode=-1;
#endif

	int lan_dhcp_mode=0;
	int dhcp_lease_time=0;
	
	strDhcp = req_get_cstream_var(wp, ("dhcp"), "");
	if (!strDhcp[0]){
		printf("call_from_wizard activated.\n");
		call_from_wizard = 1;
	}

	// Set STP
	strStp = req_get_cstream_var(wp, ("stp"), "");
	if (strStp[0]) {
		if (strStp[0] == '0')
			stp = 0;
		else
			stp = 1;
		if ( !apmib_set(MIB_STP_ENABLED, (void *)&stp)) {
			strcpy(tmpBuf, ("Set STP mib error!"));
			goto setErr_tcpip;
		}
	}
#if (defined(CONFIG_TEW814DAP_HTML) || defined(CONFIG_TEW820AP_HTML))
#else
	strVal = req_get_cstream_var(wp, "hostName", "");
		if (strVal) {
			if (!isValidName(strVal)) {
				strcpy(tmpBuf, "Invalid Host Name! Please enter characters in A(a)~Z(z) or 0-9 or dot(.) or hyphen(-) without spacing. The first character of Device Name cannot be number");
				goto setErr_tcpip;				
			}			
			if ( !apmib_set(MIB_HOST_NAME, (void *)strVal)) {
					strcpy(tmpBuf, "Set MIB_HOST_NAME MIB error!");
				goto setErr_tcpip;
			}
		}
#endif					
#if 0 // Move to formStaticDHCP
	// Set static DHCP
	strStp = req_get_cstream_var(wp, ("static_dhcp"), "");
	if (strStp[0]) {
		if (strStp[0] == '0')
			stp = 0;
		else
			stp = 1;
		if ( !apmib_set(MIB_DHCPRSVDIP_ENABLED, (void *)&stp)) {
			strcpy(tmpBuf, ("Set static DHCP mib error!"));
			goto setErr_tcpip;
		}
	}
#endif

	// Set clone MAC address
	strMac = req_get_cstream_var(wp, ("lan_macAddr"), "");
	if (strMac[0]) {
		int orig_wlan_idx=0;
		int orig_vwlan_idx=0;
		int i;
		int j;
		if (strlen(strMac)!=12 || !string_to_hex(strMac, tmpBuf, 12)) {
			strcpy(tmpBuf, ("Error! Invalid MAC address."));
			goto setErr_tcpip;
		}
		if ( !apmib_set(MIB_ELAN_MAC_ADDR, (void *)tmpBuf)) {
			strcpy(tmpBuf, ("Set MIB_ELAN_MAC_ADDR mib error!"));
			goto setErr_tcpip;
		}
		
		orig_wlan_idx=wlan_idx;
		orig_vwlan_idx=vwlan_idx;
		if( !memcmp(strMac,"000000000000",12))
		{
			for(i=0;i<NUM_WLAN_INTERFACE;i++)
			{
				wlan_idx=i;
				for(j=0;j<NUM_VWLAN_INTERFACE;j++)
				{
					vwlan_idx=j;
					if ( !apmib_set(MIB_WLAN_WLAN_MAC_ADDR, (void *)tmpBuf)) {
						strcpy(tmpBuf, ("Set MIB_WLAN_WLAN_MAC_ADDR mib error!"));
						goto setErr_tcpip;
					}
				}
			}
		}
		else
		{
			for(i=0;i<NUM_WLAN_INTERFACE;i++)
			{
				wlan_idx=i;
				for(j=0;j<NUM_VWLAN_INTERFACE;j++)
				{
					vwlan_idx=j;
					if ( !apmib_set(MIB_WLAN_WLAN_MAC_ADDR, (void *)tmpBuf)) {
						strcpy(tmpBuf, ("Set MIB_WLAN_WLAN_MAC_ADDR mib error!"));
						goto setErr_tcpip;
					}
				tmpBuf[5]++;
			}
			tmpBuf[5]-=NUM_VWLAN_INTERFACE;
			tmpBuf[5]+=0x10;
			}
		}
		
		wlan_idx=orig_wlan_idx;
		vwlan_idx=orig_vwlan_idx;
	}

	// Read current DHCP setting for reference later
	if ( !apmib_get( MIB_DHCP, (void *)&curDhcp) ) {
		strcpy(tmpBuf, ("Get DHCP MIB error!"));
		goto setErr_tcpip;
	}

	strDhcp = req_get_cstream_var(wp, ("dhcp"), "");
	strLantype = req_get_cstream_var(wp, "lan_type", "");
	if ( strDhcp[0]  && strLantype[0]) {
		lan_dhcp_mode = atoi(strDhcp);
		
		if(lan_dhcp_mode != 0 && lan_dhcp_mode != 1 && lan_dhcp_mode != 2 && lan_dhcp_mode != 15  && lan_dhcp_mode != 19){
			strcpy(tmpBuf, ("Invalid DHCP value!"));
			goto setErr_tcpip;
		}
		if ( strLantype[0]!='0' && strLantype[0]!='1' ) {
			strcpy(tmpBuf, "Invalid DHCP value!");
			goto setErr_tcpip;
		}
	/*	if ( !apmib_set(MIB_DHCP, (void *)&lan_dhcp_mode)) {
	  		strcpy(tmpBuf, ("Set DHCP error!"));
			goto setErr_tcpip;
		}*/
		//dhcp = lan_dhcp_mode;
		if ( strLantype[0] == '1' ){
			dhcp = DHCP_CLIENT;
		}else if (strLantype[0] == '0'){
			if ( strDhcp[0] == '0' ){
				dhcp = DHCP_DISABLED;
			}else if (strDhcp[0] == '2'){
				dhcp = DHCP_SERVER;
			}
		}
	}
	else
	{
		if(strDhcp[0])
		{
			if ( strDhcp[0] == '0' )
				dhcp = DHCP_DISABLED;
			else
				dhcp = DHCP_SERVER;
		}
		else
			dhcp = curDhcp;
	}
	
	if ( dhcp == DHCP_DISABLED || dhcp == DHCP_SERVER || dhcp == DHCP_AUTO || DHCP_AUTO_WAN) {//0,2,15,unknown.
		strIp = req_get_cstream_var(wp, ("lan_ip"), "");
		if ( strIp[0] ) {
			if ( !inet_aton(strIp, &inIp) ) {
				strcpy(tmpBuf, ("Invalid IP-address value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set( MIB_IP_ADDR, (void *)&inIp)) {
				strcpy(tmpBuf, ("Set IP-address error!"));
				goto setErr_tcpip;
			}
		}
		else { // get current used IP
			if ( !getInAddr(BRIDGE_IF, IP_ADDR, (void *)&inIp) ) {
				strcpy(tmpBuf, ("Get IP-address error!"));
				goto setErr_tcpip;
			}
		}

		strMask = req_get_cstream_var(wp, ("lan_mask"), "");
		if ( strMask[0] ) {
			if ( !inet_aton(strMask, &inMask) ) {
				strcpy(tmpBuf, ("Invalid subnet-mask value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set(MIB_SUBNET_MASK, (void *)&inMask)) {
				strcpy(tmpBuf, ("Set subnet-mask error!"));
				goto setErr_tcpip;
			}
		}
		else { // get current used netmask
			if ( !getInAddr(BRIDGE_IF, SUBNET_MASK, (void *)&inMask )) {
				strcpy(tmpBuf, ("Get subnet-mask error!"));
				goto setErr_tcpip;
			}
		}
		//John H. 2006.01.10 move here to fix LAN ip bug
		if ( strIp[0] && strMask[0] ) {

			ip_t = *((unsigned long *)&inIp);
			mask_t = *((unsigned long *)&inMask);

		    if((((ip_t & 0xff000000) >> 24) == 127) ||
		        (((ip_t & 0xff000000) >> 24) == 224) ) {
			    strcpy(tmpBuf, "127 and 224 is special for Microsoft!");
			    goto setErr_tcpip;
		    }
		    if((ip_t & (~mask_t)) == (mask_all & (~mask_t))) {
				strcpy(tmpBuf, "Invalid IP-address value!");
				goto setErr_tcpip;
			}		        
		    if(mask_t == mask_all) {
				strcpy(tmpBuf, "Invalid subnet-mask value!");
				goto setErr_tcpip;
			}		        
		}
		strGateway = req_get_cstream_var(wp, ("lan_gateway"), "");
		printf("########dhcp=%d, lan_gateway=%s####\n", dhcp, strGateway);
		if ( (dhcp == DHCP_DISABLED && strGateway[0]) || 
			(dhcp == DHCP_SERVER && strGateway[0])	) {
			if ( !inet_aton(strGateway, &inGateway) ) {
				strcpy(tmpBuf, ("Invalid default-gateway value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set(MIB_DEFAULT_GATEWAY, (void *)&inGateway)) {
				strcpy(tmpBuf, ("Set default-gateway error!"));
				goto setErr_tcpip;
			}
			gateway_t = *((unsigned long *)&inGateway);
		    	if(ip_t == gateway_t) {
#ifdef HOME_GATEWAY		//John Huang add for 7-mode 2007.02.28
				apmib_get( MIB_OP_MODE, (void *)&opmode);
				if(opmode == BRIDGE_MODE)
				{
#endif
				strcpy(tmpBuf, "The gateway setting cannot be itself!");
				goto setErr_tcpip;
#ifdef HOME_GATEWAY		//John Huang add for 7-mode 2007.02.28
				}
#endif
			}		        			
		if(strcmp(strGateway,"0.0.0.0")!=0){
      	if((ip_t&mask_t) != (gateway_t&mask_t)) {
        	strcpy(tmpBuf, retLS("00lan047"));
          goto setErr_tcpip;
        }
     	}
		}

		if ( dhcp == DHCP_SERVER|| dhcp == DHCP_AUTO || DHCP_AUTO_WAN) {
			// Get/Set DHCP client range
			strdhcpRangeStart = req_get_cstream_var(wp, ("dhcpRangeStart"), "");
			if ( strdhcpRangeStart[0] ) {
				if ( !inet_aton(strdhcpRangeStart, &dhcpRangeStart) ) {
					strcpy(tmpBuf, retLS("00lan044"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DHCP_CLIENT_START, (void *)&dhcpRangeStart)) {
					strcpy(tmpBuf, ("Set DHCP client start address error!"));
					goto setErr_tcpip;
				}
			}
			strdhcpRangeEnd = req_get_cstream_var(wp, ("dhcpRangeEnd"), "");
			if ( strdhcpRangeEnd[0] ) {
				if ( !inet_aton(strdhcpRangeEnd, &dhcpRangeEnd) ) {
					strcpy(tmpBuf, retLS("00lan045"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DHCP_CLIENT_END, (void *)&dhcpRangeEnd)) {
					strcpy(tmpBuf, ("Set DHCP client end address error!"));
					goto setErr_tcpip;
				}
			}

			if ( strdhcpRangeStart[0] && strdhcpRangeEnd[0] ) {
				unsigned long start, end, mask, ip;
				int diff;

				start = *((unsigned long *)&dhcpRangeStart);
				end = *((unsigned long *)&dhcpRangeEnd);
				diff = (int) ( ntohdw(end) - ntohdw(start) );
				ip = *((unsigned long *)&inIp);
				mask = *((unsigned long *)&inMask);
				if (diff <= 0 ||
					diff > 256*3 ||
					(ip&mask) != (start&mask) ||
					(ip&mask) != (end& mask) ) {
					//strcpy(tmpBuf, ("Invalid DHCP client range!"));
					strcpy(tmpBuf, retLS("00lan048"));
					goto setErr_tcpip;
				}
				if((ip>=start)&&(ip<=end)) {
					strcpy(tmpBuf, retLS("00lan048"));
					goto setErr_tcpip;
				}
			}

            //John H. 2006.01.23 move here to fix DHCP bug
		    strLeaseTime = req_get_cstream_var(wp, "leaseTime", "");
		    if ( strLeaseTime[0] ) {
			    if ( !string_to_dec(strLeaseTime, &leaseTime) ) {
				    strcpy(tmpBuf, "Error! Invalid value of DHCP Lease Time.");
				    goto setErr_tcpip;
			    }
				
			    if ( !apmib_set(MIB_DHCP_LEASE_TIME, (void *)&leaseTime)) {
	  			    strcpy(tmpBuf, "Set DHCP LEASE TIME MIB error!");
				    goto setErr_tcpip;
			    }
		    }
            //John H. 2006.01.23 move here to fix DHCP bug

            //John H. 2006.01.10 move here to fix DHCP bug
            if ( strdhcpRangeStart[0] )
			if ( !apmib_set(MIB_DHCP_CLIENT_START, (void *)&dhcpRangeStart)) {
				strcpy(tmpBuf, "Set DHCP client start address error!");
				goto setErr_tcpip;
			}
			if ( strdhcpRangeEnd[0] )
			if ( !apmib_set(MIB_DHCP_CLIENT_END, (void *)&dhcpRangeEnd)) {
				strcpy(tmpBuf, "Set DHCP client end address error!");
				goto setErr_tcpip;
			}			

			strDHCP_Broadcast =req_get_cstream_var(wp, "always_broadcast", "");
			printf("strDHCP_Broadcast = %s\n",strDHCP_Broadcast);
			if ( strDHCP_Broadcast[0] ){
				tmp=atoi(strDHCP_Broadcast);
				printf("tmp = %d\n",tmp);
				if ( !apmib_set(MIB_RESERVED_WORD_5, (void *)&tmp)) {
					strcpy(tmpBuf, "Set DHCP Broadcast value error!");
					goto setErr_tcpip;
				}
			}
			
			strGateway =req_get_cstream_var(wp, "dhcpgateway", "");
			if (strGateway[0]) {
				if ( !inet_aton(strGateway, &dhcpGateway) ) {
					strcpy(tmpBuf, retLS("other056"));
					goto setErr_tcpip;
				}
				
				if ( !apmib_set( MIB_RESERVED_STR_0,  (void *)&dhcpGateway) )
					 		return -1;
			}
			
			
			strWINS = req_get_cstream_var(wp, "dhcpwins", "");
			if ( strWINS[0] ) {
				if ( !inet_aton(strWINS, &dhcpWINS) ) {
					strcpy(tmpBuf, retLS("other057"));
					goto setErr_tcpip;
				}

				if ( !apmib_set(MIB_RESERVED_STR_1, (void *)&dhcpWINS)) {
	  				strcpy(tmpBuf, "Set WINS MIB error!");
					goto setErr_tcpip;
				}
			}
			
			strDNS = req_get_cstream_var(wp, "dhcpdns", "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dhcpDNS) ) {
					strcpy(tmpBuf, retLS("wles1096"));
					goto setErr_tcpip;
				}
				
					if ( !apmib_set(MIB_RESERVED_STR_2, (void *)&dhcpDNS)) {
	  					strcpy(tmpBuf, "Set DNS MIB error!");
						goto setErr_tcpip;
					}
				
			}
			
			// If DHCP server is enabled in LAN, update dhcpd.conf
			strDNS = req_get_cstream_var(wp, "dns1", "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns1) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}

				if ( !apmib_set(MIB_DNS1, (void *)&dns1)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}

			strDNS = req_get_cstream_var(wp, ("dns2"), "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns2) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DNS2, (void *)&dns2)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}

			strDNS = req_get_cstream_var(wp, ("dns3"), "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns3) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DNS3, (void *)&dns3)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}

			if (!call_from_wizard) {
				strDhcpLeaseTime = req_get_cstream_var(wp, ("leaseTime"), "");
	                if ( strDhcpLeaseTime ) {
						dhcp_lease_time = atoi(strDhcpLeaseTime);
                    		if ( !apmib_set(MIB_DHCP_LEASE_TIME, (void *)&dhcp_lease_time)) {
		                        strcpy(tmpBuf, ("Set MIB_DHCP_LEASE_TIME MIB error!"));
                	        goto setErr_tcpip;
	                    }
        	        }
				strDomain = req_get_cstream_var(wp, ("domainName"), "");
				if ( strDomain ) {
					if (!isValidName(strDomain)) {
  						strcpy(tmpBuf, ("Invalid Domain Name! Please enter characters in A(a)~Z(z) or 0-9 without spacing."));
						goto setErr_tcpip;				
					}							
					if ( !apmib_set(MIB_DOMAIN_NAME, (void *)strDomain)) {
	  					strcpy(tmpBuf, ("Set MIB_DOMAIN_NAME MIB error!"));
						goto setErr_tcpip;
					}
				}else{
					 if ( !apmib_set(MIB_DOMAIN_NAME, (void *)"")){
	  					strcpy(tmpBuf, ("\"Set MIB_DOMAIN_NAME MIB error!\""));
						goto setErr_tcpip;
					}	
				}
			}//call wizard	
		}			
		// If DHCP server is enabled in LAN, update dhcpd.conf
		strDNS = req_get_cstream_var(wp, "dns1", "");
		if ( strDNS[0] ) {
			if ( !inet_aton(strDNS, &dns1) ) {
				strcpy(tmpBuf, retLS("wles1096"));
				goto setErr_tcpip;
			}

			if ( !apmib_set(MIB_DNS1, (void *)&dns1)) {
	  			strcpy(tmpBuf, "Set DNS MIB error!");
				goto setErr_tcpip;
			}
		}
		strDNS = req_get_cstream_var(wp, "dns2", "");
		if ( strDNS[0] ) {
			if ( !inet_aton(strDNS, &dns2) ) {
				strcpy(tmpBuf, retLS("wles1096"));
				goto setErr_tcpip;
			}

			if ( !apmib_set(MIB_DNS2, (void *)&dns2)) {
	  			strcpy(tmpBuf, "Set DNS MIB error!");
				goto setErr_tcpip;
			}
		}
		if ( strIp[0] )
		if ( !apmib_set( MIB_IP_ADDR, (void *)&inIp)) {
			strcpy(tmpBuf, "Set IP-address error!");
			goto setErr_tcpip;
		}
		if ( strMask[0] )
		if ( !apmib_set(MIB_SUBNET_MASK, (void *)&inMask)) {
			strcpy(tmpBuf, "Set subnet-mask error!");
			goto setErr_tcpip;
		}		
		//John H. 2006.05.03 move here to fix LAN ip bug
		//John fix dhcp server problem for ip change 2006.11.14
		if ( strIp[0] )
		{
#ifdef HOME_GATEWAY		//John Huang add for 7-mode 2007.02.28
			apmib_get( MIB_OP_MODE, (void *)&opmode);
			if(opmode != BRIDGE_MODE)
			{
				if ( !apmib_set(MIB_DEFAULT_GATEWAY, (void *)&inIp)) {
					strcpy(tmpBuf, "Set default-gateway error!");
					goto setErr_tcpip;
				}
			}
#endif
		
			//Modify dhcpGateway also change mask.
			apmib_get( MIB_DHCP_CLIENT_START,  (void *)tmpBuf);
			strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
			inet_aton(str_ip, &dhcpRangeStart);
			apmib_get( MIB_DHCP_CLIENT_END,  (void *)tmpBuf);
			strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
			inet_aton(str_ip, &dhcpRangeEnd);
			apmib_get( MIB_RESERVED_STR_0,  (void *)tmpBuf);
			strcpy(str_ip, inet_ntoa(*((struct in_addr *)tmpBuf)));
			inet_aton(str_ip, &dhcpGateway);
			start = *((unsigned long *)&dhcpRangeStart);
			end = *((unsigned long *)&dhcpRangeEnd);
			dhcp_gw = *((unsigned long *)&dhcpGateway);
			if((ip_t&mask_t) != (start&mask_t))
			{
				start = (start & (~mask_t)) | (ip_t & mask_t);
				end = (end & (~mask_t)) | (ip_t & mask_t);
				dhcp_gw = (dhcp_gw & (~mask_t)) | (ip_t & mask_t);
				dhcpRangeStart = *((struct in_addr *)&start);
				dhcpRangeEnd = *((struct in_addr *)&end);
				dhcpGateway = *((struct in_addr *)&dhcp_gw);
				apmib_set(MIB_DHCP_CLIENT_START, (void *)&dhcpRangeStart);
				apmib_set(MIB_DHCP_CLIENT_END, (void *)&dhcpRangeEnd);
				apmib_set( MIB_RESERVED_STR_0,  (void *)&dhcpGateway);
			}
		} 
		//John fix dhcp server problem for ip change 2006.11.14
		
		
	}
    //John H. 2006.01.23 move here to fix DHCP bug
	if ( strDhcp[0] ) {
		if ( !apmib_set(MIB_DHCP, (void *)&dhcp)) {
	  		strcpy(tmpBuf, "Set DHCP error!");
			goto setErr_tcpip;
		}
	}
    //John H. 2006.01.23 move here to fix DHCP bug
	return 0 ;
setErr_tcpip:
	return -1 ;	
}

///////////////////////////////////////////////////////////////////
#if defined(MIB_TLV)
extern int mib_search_by_id(const mib_table_entry_T *mib_tbl, unsigned short mib_id, unsigned char *pmib_num, const mib_table_entry_T **ppmib, unsigned int *offset);
extern mib_table_entry_T mib_root_table[];
#else
extern int update_linkchain(int fmt, void *Entry_old, void *Entry_new, int type_size);
#endif
void formTcpipSetup(request *wp, char *path, char *query)
{
	char tmpBuf[250];
	char *submitUrl ;
	char *dhcp_enable;
	
	//add by Pan 2010,0924
	isNeedReboot = atoi(req_get_cstream_var(wp, "isNeedReboot", ""));
	//fprintf(stderr,"isNeedReboot = %d\n",isNeedReboot);
	if((isNeedReboot==SAVE_NO_REBOOT) || (isNeedReboot==SAVE_AND_REBOOT))
	{	
	if(tcpipLanHandler(wp, tmpBuf) < 0){
		goto setErr_end ;
	}
#if (defined(CONFIG_TEW814DAP_HTML) || defined(CONFIG_TEW820AP_HTML))

#else
#ifdef CONFIG_IPV6

	if(!formAdvIpv6(wp,path,query))
		goto setErr_end;
#endif	

#endif
	apmib_update(CURRENT_SETTING);	// update configuration to flash
	}
	reboot_redirect_Handler(wp);
	return;

setErr_end:
	ERR_MSG(tmpBuf);
}

#ifdef RTK_USB3G
void kill_3G_ppp_inet(void)
{
    system("killall -15 ppp_inet 2> /dev/null");
    system("killall -15 pppd 2> /dev/null");
    system("rm /etc/ppp/connectfile >/dev/null 2>&1");
    system("rm /etc/ppp/link >/dev/null 2>&1");
}
#endif

#ifdef HOME_GATEWAY
int tcpipWanHandler(request *wp, char * tmpBuf, int *dns_changed)
{
	
	char	*strIp, *strMask, *strGateway, *strDNS, *strMode, *strConnect, *strMac;
	char  *strVal, *strType;
	int intVal;
	struct in_addr inIp, inMask,dns1, dns2, dns3, inGateway;
	DHCP_T dhcp, curDhcp;
	
#if defined(ROUTE_SUPPORT)	
	int orig_nat=0;
	int curr_nat=0;
#endif	
	DNS_TYPE_T dns, dns_old;

	char *submitUrl;
#ifndef NO_ACTION
	int pid;
#endif
#ifdef MULTI_PPPOE
	int flag = 0;
#endif
	int buttonState=0, call_from_wizard=0;
	
#if defined(CONFIG_DYNAMIC_WAN_IP)
	char *strPPPGateway, *strWanIpType;
	struct in_addr inPPPGateway;
	WAN_IP_TYPE_T wanIpType;
#if defined(CONFIG_GET_SERVER_IP_BY_DOMAIN)
	char *strGetServByDomain=NULL;
	char *strGatewayDomain;
#endif
#endif

	strVal = req_get_cstream_var(wp, ("lan_ip"), "");
	if (strVal[0])
		call_from_wizard = 1;	
	
	strVal = req_get_cstream_var(wp, ("isPocketWizard"), "");
	if (strVal[0])
	{
		if ( atoi(strVal) == 1 )
		{
			call_from_wizard = 1;
		}
	}
			
	submitUrl = req_get_cstream_var(wp, "submit-url", "");   // hidden page

	strConnect = req_get_cstream_var(wp, ("pppConnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
#ifdef MULTI_PPPOE		
		flag = 1;
#endif
		strMode = "ppp";
		goto set_ppp;
	}

        strConnect = req_get_cstream_var(wp, ("pppDisconnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
#ifdef MULTI_PPPOE		
		flag = 1;
#endif	
		strMode = "ppp";
		goto set_ppp;
	}
#ifdef  MULTI_PPPOE
	//second
	strConnect = req_get_cstream_var(wp, ("pppConnect2"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
#ifdef MULTI_PPPOE		
		flag = 2;
#endif	
		strMode = "ppp";
		goto set_ppp;
	}
		strConnect = req_get_cstream_var(wp, ("pppDisconnect2"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
#ifdef MULTI_PPPOE		
		flag = 2;
#endif			
		strMode = "ppp";
		goto set_ppp;
	}
	//thrid 
		strConnect = req_get_cstream_var(wp, ("pppConnect3"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
#ifdef MULTI_PPPOE		
		flag = 3;
#endif	
		strMode = "ppp";
		goto set_ppp;
	}

		strConnect = req_get_cstream_var(wp, ("pppDisconnect3"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
#ifdef MULTI_PPPOE		
		flag = 3;
#endif				
		strMode = "ppp";
		goto set_ppp;
	}
	//forth
		strConnect = req_get_cstream_var(wp, ("pppConnect4"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
#ifdef MULTI_PPPOE		
		flag = 4;
#endif			
		strMode = "ppp";
		goto set_ppp;
	}

		strConnect = req_get_cstream_var(wp, ("pppDisconnect4"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
#ifdef MULTI_PPPOE		
		flag = 4;
#endif			
		strMode = "ppp";
		goto set_ppp;
	}
#endif
	
	strConnect = req_get_cstream_var(wp, ("pptpConnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
		strMode = "pptp";
		goto set_ppp;
	}

        strConnect = req_get_cstream_var(wp, ("pptpDisconnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
		strMode = "pptp";
		goto set_ppp;
	}
	strConnect = req_get_cstream_var(wp, ("l2tpConnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 1;
		strMode = "l2tp";
		goto set_ppp;
	}

        strConnect = req_get_cstream_var(wp, ("l2tpDisconnect"), "");
	if (strConnect && strConnect[0]) {
		buttonState = 2;
		strMode = "l2tp";
		goto set_ppp;
	}

#ifdef RTK_USB3G
    strConnect = req_get_cstream_var(wp, ("USB3GConnect"), "");
    if (strConnect && strConnect[0]) {
        buttonState = 1;
        strMode = ("USB3G");
        goto set_ppp;
    }

    strConnect = req_get_cstream_var(wp, ("USB3GDisconnect"), "");
    if (strConnect && strConnect[0]) {
        buttonState = 2;
        strMode = ("USB3G");
        goto set_ppp;
    }
#endif /* #ifdef RTK_USB3G */

#if 0 //sc_yang
	strVal = req_get_cstream_var(wp, ("save"), "");
	if (!strVal || !strVal[0]) { // not save, wan type is changed
		strVal = req_get_cstream_var(wp, ("wanType"), "");
		wanDhcpTmp = (DHCP_T)(strVal[0] - '0');

		if (submitUrl && submitUrl[0])
			send_redirect_perm(wp, submitUrl);
		return;
	}
#endif
 	// Set clone MAC address
	strMac = req_get_cstream_var(wp, ("wan_macAddr"), "");
	if (strMac[0]) {
		if (strlen(strMac)!=12 || !string_to_hex(strMac, tmpBuf, 12)) {
			strcpy(tmpBuf, ("Error! Invalid MAC address."));
			goto setErr_tcpip;
		}
		if ( !apmib_set(MIB_WAN_MAC_ADDR, (void *)tmpBuf)) {
			strcpy(tmpBuf, ("Set MIB_WAN_MAC_ADDR mib error!"));
			goto setErr_tcpip;
		}
	}else{ //Brent<add>: if input empty string 
		unsigned char defMac[12]={0};
		if ( !apmib_set(MIB_WAN_MAC_ADDR, (void *)defMac)) {
			strcpy(tmpBuf, "Set MIB_WAN_MAC_ADDR mib error!");
			return -1;
		}
	}
	strMode = req_get_cstream_var(wp, ("dnsMode"), "");
	if ( strMode && strMode[0] ) {
		if (!strcmp(strMode, ("dnsAuto")))
			dns = DNS_AUTO;
		else if (!strcmp(strMode, ("dnsManual")))
			dns = DNS_MANUAL;
		else {
			strcpy(tmpBuf, ("Invalid DNS mode value!"));
			goto setErr_tcpip;
		}

		if ( !apmib_get(MIB_DNS_MODE, (void *)&dns_old)) {
	  		strcpy(tmpBuf, ("Get DNS MIB error!"));
			goto setErr_tcpip;
		}
		if (dns != dns_old)
			*dns_changed = 1;

		// Set DNS to MIB
		if ( !apmib_set(MIB_DNS_MODE, (void *)&dns)) {
	  		strcpy(tmpBuf, "Set DNS MIB error!");
			goto setErr_tcpip;
		}

		if ( dns == DNS_MANUAL ) {
			struct in_addr dns1_old, dns2_old, dns3_old;
			if ( !apmib_get(MIB_DNS1, (void *)&dns1_old)) {
	  			strcpy(tmpBuf, "Get DNS1 MIB error!");
				goto setErr_tcpip;
			}
			if ( !apmib_get(MIB_DNS2, (void *)&dns2_old)) {
	  			strcpy(tmpBuf, "Get DNS1 MIB error!");
				goto setErr_tcpip;
			}
			if ( !apmib_get(MIB_DNS3, (void *)&dns3_old)) {
	  			strcpy(tmpBuf, "Get DNS1 MIB error!");
				goto setErr_tcpip;
			}

			// If DHCP server is enabled in LAN, update dhcpd.conf
			strDNS = req_get_cstream_var(wp, ("dns1"), "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns1) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}

				if ( !apmib_set(MIB_DNS1, (void *)&dns1)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}
			else {
				if ( !apmib_get(MIB_DNS1, (void *)&dns1) ) {
					strcpy(tmpBuf, "Get DNS1 MIB error!");
					goto setErr_tcpip;
				}
			}
			strDNS = req_get_cstream_var(wp, ("dns2"), "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns2) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DNS2, (void *)&dns2)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}
			else {
				if ( !apmib_get(MIB_DNS2, (void *)&dns2) ) {
					strcpy(tmpBuf, ("Get DNS2 MIB error!"));
					goto setErr_tcpip;
				}
			}
			strDNS = req_get_cstream_var(wp, ("dns3"), "");
			if ( strDNS[0] ) {
				if ( !inet_aton(strDNS, &dns3) ) {
					strcpy(tmpBuf, ("Invalid DNS address value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_DNS3, (void *)&dns3)) {
	  				strcpy(tmpBuf, "Set DNS MIB error!");
					goto setErr_tcpip;
				}
			}
			else {
				if ( !apmib_get(MIB_DNS3, (void *)&dns3) ) {
					strcpy(tmpBuf, "Get DNS3 MIB error!");
					goto setErr_tcpip;
				}
			}

			if ( *((long *)&dns1) != *((long *)&dns1_old) ||
				 *((long *)&dns2) != *((long *)&dns2_old) ||
				  *((long *)&dns3) != *((long *)&dns3_old) )
				*dns_changed = 1;
		}
	}

	// Read current ip mode setting for reference later
	if ( !apmib_get( MIB_WAN_DHCP, (void *)&curDhcp) ) {
		strcpy(tmpBuf, ("Get WAN DHCP MIB error!"));
		goto setErr_tcpip;
	}
#if defined(ROUTE_SUPPORT)	
	if ( !apmib_get( MIB_NAT_ENABLED, (void *)&orig_nat) ) {
		strcpy(tmpBuf, ("Get NAT MIB error!"));
		goto setErr_tcpip;
	}
	
#endif	
	//sc_yang
	//strMode = req_get_cstream_var(wp, ("ipMode"), "");
	strMode = req_get_cstream_var(wp, ("wanType"), "");

#if defined(CONFIG_RTL_ULINKER)
	if ( strMode && strMode[0] )
		;
	else
	{
		strMode = req_get_cstream_var(wp, ("otg_wan_type"), "");
	}
#endif

set_ppp:
	if ( strMode && strMode[0] ) {
		if ( !strcmp(strMode, ("autoIp")))
			dhcp = DHCP_CLIENT;
		else if ( !strcmp(strMode, ("fixedIp")))
			dhcp = DHCP_DISABLED;
		else if ( !strcmp(strMode, "ppp")) {
			char	*strName, *strPassword, *strService;
			char 	*strConnectNumber;
			char  *strsubnetNumber;
			char	*strIp;
			char  *strSubNet;
			struct in_addr  inIp;
			int count;
			dhcp = PPPOE;
			strConnectNumber = req_get_cstream_var(wp, "pppoeNumber", "");
			count = strtol(strConnectNumber, (char**)NULL, 10);
	/* Carl: marked these to put mydlink into mib table
			if(strConnectNumber[0]){				
				if ( apmib_set(MIB_PPP_CONNECT_COUNT, (void *)&count) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}			
			strSubNet = req_get_cstream_var(wp, "pppSubNet_1", "");
			if(strSubNet[0]){
				if ( apmib_set(MIB_PPP_SUBNET1, (void *)strSubNet) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}	
			strSubNet = req_get_cstream_var(wp, "pppSubNet_2", "");
			if(strSubNet[0]){
				if ( apmib_set(MIB_PPP_SUBNET2, (void *)strSubNet) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}
			strSubNet = req_get_cstream_var(wp, "pppSubNet_3", "");
			if(strSubNet[0]){
				if ( apmib_set(MIB_PPP_SUBNET3, (void *)strSubNet) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}
			strSubNet = req_get_cstream_var(wp, "pppSubNet_4","");
			if(strSubNet[0]){
				if ( apmib_set(MIB_PPP_SUBNET4, (void *)strSubNet) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}
			strsubnetNumber = req_get_cstream_var(wp, "pppSubNet1","");
			count = strtol(strsubnetNumber, (char**)NULL, 10);
			if(strsubnetNumber[0]){
				if ( apmib_set(MIB_SUBNET1_COUNT, (void *)&count) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}		

			strsubnetNumber = req_get_cstream_var(wp, "pppSubNet2", "");
			count = strtol(strsubnetNumber, (char**)NULL, 10);
			if(strsubnetNumber[0]){
				if ( apmib_set(MIB_SUBNET2_COUNT, (void *)&count) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}

			strsubnetNumber = req_get_cstream_var(wp, "pppSubNet3", "");
			count = strtol(strsubnetNumber, (char**)NULL, 10);
			if(strsubnetNumber[0]){
				if ( apmib_set(MIB_SUBNET3_COUNT, (void *)&count) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}

			strsubnetNumber = req_get_cstream_var(wp, "pppSubNet4", "");
			count = strtol(strsubnetNumber, (char**)NULL, 10);
			if(strsubnetNumber[0]){
				if ( apmib_set(MIB_SUBNET4_COUNT, (void *)&count) == 0) {
					strcpy(tmpBuf, "Set PPPoE Number MIB error!");
					goto setErr_tcpip;
				}				
			}			
			// four ip seting,the first one
			strIp = req_get_cstream_var(wp, "S1_F1_start", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F1_START, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp, "S1_F1_end", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F1_END, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp, "S1_F2_start", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F2_START, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp, "S1_F2_end", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F2_END, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp, "S1_F3_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F3_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S1_F3_end", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET1_F3_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			
			//the second
			strIp = req_get_cstream_var(wp,"S2_F1_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F1_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp,"S2_F1_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F1_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S2_F2_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F2_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S2_F2_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F2_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp,"S2_F3_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F3_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S2_F3_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET2_F3_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			//the third 
			strIp = req_get_cstream_var(wp,"S3_F1_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F1_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp,"S3_F1_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F1_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S3_F2_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F2_START, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S3_F2_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F2_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp,"S3_F3_start","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F3_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S3_F3_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET3_F3_END, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			//the forth
			strIp = req_get_cstream_var(wp,"S4_F1_start", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F1_START, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp, "S4_F1_end", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F1_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S4_F2_start", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F2_START, (void *)&inIp)) {
					strcpy(tmpBuf, "Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp, "S4_F2_end", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F2_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			
			strIp = req_get_cstream_var(wp,"S4_F3_start", "");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf, "Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F3_START, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}
			strIp = req_get_cstream_var(wp,"S4_F3_end","");
			if ( strIp[0] ) {
				if ( !inet_aton(strIp, &inIp) ) {
					strcpy(tmpBuf,"Invalid subnet-mask value!");
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_SUBNET4_F3_END, (void *)&inIp)) {
					strcpy(tmpBuf,"Set subnet-mask error!");
					goto setErr_tcpip;
				}
			}			

  			strName = req_get_cstream_var(wp, ("pppUserName"), "");
			if ( strName[0] ) {
				if ( apmib_set(MIB_PPP_USER_NAME, (void *)strName) == 0) {
					strcpy(tmpBuf, ("Set PPP user name MIB error!"));
					goto setErr_tcpip;
				}
			}

 			strPassword = req_get_cstream_var(wp, ("pppPassword"), "");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_PPP_PASSWORD, (void *)strPassword) == 0) {
					strcpy(tmpBuf, ("Set PPP user password MIB error!"));
					goto setErr_tcpip;
				}
			}
			strName = req_get_cstream_var(wp,"pppUserName2" ,"");
			if ( strName[0] ) {
				if ( apmib_set(MIB_PPP_USER_NAME2, (void *)strName) == 0) {
					strcpy(tmpBuf,"Set PPP user name MIB error!");
					goto setErr_tcpip;
				}
			}

			strPassword = req_get_cstream_var(wp,"pppPassword2" ,"");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_PPP_PASSWORD2, (void *)strPassword) == 0) {
					strcpy(tmpBuf,"Set PPP user password MIB error!");
					goto setErr_tcpip;
				}
			}
			strName = req_get_cstream_var(wp,"pppUserName3","");
			if ( strName[0] ) {
				if ( apmib_set(MIB_PPP_USER_NAME3, (void *)strName) == 0) {
					strcpy(tmpBuf, "Set PPP user name MIB error!");
					goto setErr_tcpip;
				}
			}

			strPassword = req_get_cstream_var(wp,"pppPassword3","");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_PPP_PASSWORD3, (void *)strPassword) == 0) {
					strcpy(tmpBuf, "Set PPP user password MIB error!");
					goto setErr_tcpip;
				}
			}
			strName = req_get_cstream_var(wp, "pppUserName4","");
			if ( strName[0] ) {
				if ( apmib_set(MIB_PPP_USER_NAME4, (void *)strName) == 0) {
					strcpy(tmpBuf, "Set PPP user name MIB error!");
					goto setErr_tcpip;
				}
			}

			strPassword = req_get_cstream_var(wp,"pppPassword4", "");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_PPP_PASSWORD4, (void *)strPassword) == 0) {
					strcpy(tmpBuf, "Set PPP user password MIB error!");
					goto setErr_tcpip;
				}
			}

			strService = req_get_cstream_var(wp, ("pppServiceName"), "");
			if ( strService[0] ) {
				if ( apmib_set(MIB_PPP_SERVICE_NAME, (void *)strService) == 0) {
					strcpy(tmpBuf, ("Set PPP serice name MIB error!"));
					goto setErr_tcpip;
				}
			}else{
				if ( apmib_set(MIB_PPP_SERVICE_NAME, (void *)"") == 0) {
					strcpy(tmpBuf, ("Set PPP serice name MIB error!"));
					goto setErr_tcpip;
				}
			}
			strService = req_get_cstream_var(wp, "pppServiceName2","");
			if ( strService[0] ) {
				if ( apmib_set(MIB_PPP_SERVICE_NAME2, (void *)strService) == 0) {
					strcpy(tmpBuf,"Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}else{
				if ( apmib_set(MIB_PPP_SERVICE_NAME2, (void *)"") == 0) {
					strcpy(tmpBuf,"Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}
			strService = req_get_cstream_var(wp,"pppServiceName3","");
			if ( strService[0] ) {
				if ( apmib_set(MIB_PPP_SERVICE_NAME3, (void *)strService) == 0) {
					strcpy(tmpBuf, "Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}else{
				if ( apmib_set(MIB_PPP_SERVICE_NAME3, (void *)"") == 0) {
					strcpy(tmpBuf, "Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}
			strService = req_get_cstream_var(wp,"pppServiceName4" ,"");
			if ( strService[0] ) {
				if ( apmib_set(MIB_PPP_SERVICE_NAME4, (void *)strService) == 0) {
					strcpy(tmpBuf, "Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}else{
				if ( apmib_set(MIB_PPP_SERVICE_NAME4, (void *)"") == 0) {
					strcpy(tmpBuf, "Set PPP serice name MIB error!");
					goto setErr_tcpip;
				}
			}
			
			strType = req_get_cstream_var(wp, ("pppConnectType"), "");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, ("Invalid PPP type value!"));
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_PPP_CONNECT_TYPE, (void *)&type) == 0) {
   					strcpy(tmpBuf, ("Set PPP type MIB error!"));
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, ("pppIdleTime"), "");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_PPP_IDLE_TIME, (void *)&time) == 0) {
   							strcpy(tmpBuf, ("Set PPP idle time MIB error!"));
							goto setErr_tcpip;
						}
					}
				}
			}
			strType = req_get_cstream_var(wp, "pppConnectType2","");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, "Invalid PPP type value!");
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_PPP_CONNECT_TYPE2, (void *)&type) == 0) {
   					strcpy(tmpBuf, "Set PPP type MIB error!");
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, "pppIdleTime2","");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_PPP_IDLE_TIME2, (void *)&time) == 0) {
   							strcpy(tmpBuf, "Set PPP idle time MIB error!");
							goto setErr_tcpip;
						}
					}
				}
			}
						
			strType = req_get_cstream_var(wp, "pppConnectType3", "");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, "Invalid PPP type value!");
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_PPP_CONNECT_TYPE3, (void *)&type) == 0) {
   					strcpy(tmpBuf, "Set PPP type MIB error!");
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, "pppIdleTime3", "");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_PPP_IDLE_TIME3, (void *)&time) == 0) {
   							strcpy(tmpBuf, "Set PPP idle time MIB error!");
							goto setErr_tcpip;
						} 
					}
				}
			}	
			strType = req_get_cstream_var(wp, "pppConnectType4" ,"");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, "Invalid PPP type value!");
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_PPP_CONNECT_TYPE4, (void *)&type) == 0) {
   					strcpy(tmpBuf, "Set PPP type MIB error!");
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, "pppIdleTime4", "");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_PPP_IDLE_TIME4, (void *)&time) == 0) {
   							strcpy(tmpBuf, "Set PPP idle time MIB error!");
							goto setErr_tcpip;
						}
					}
				}
			}			
			strVal = req_get_cstream_var(wp, ("pppMtuSize"), "");
			if ( strVal[0] ) {
				int mtuSize;
 				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_PPP_MTU_SIZE, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, ("Set PPP mtu size MIB error!"));
					goto setErr_tcpip;
				}
			}
			strVal = req_get_cstream_var(wp,"pppMtuSize2","");
			if ( strVal[0] ) {
				int mtuSize;
				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_PPP_MTU_SIZE2, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, "Set PPP mtu size MIB error!");
					goto setErr_tcpip;
				}
			}
			
			strVal = req_get_cstream_var(wp, "pppMtuSize3","");
			if ( strVal[0] ) {
				int mtuSize;
				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_PPP_MTU_SIZE3, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, "Set PPP mtu size MIB error!");
					goto setErr_tcpip;
				}
			}
			strVal = req_get_cstream_var(wp, "pppMtuSize4", "");
			if ( strVal[0] ) {
				int mtuSize;
				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_PPP_MTU_SIZE4, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, "Set PPP mtu size MIB error!");
					goto setErr_tcpip;
				}
			}
			Carl end*/
		}
		else if ( !strcmp(strMode, "pptp")) {
			char	*strName, *strPassword;
			dhcp = PPTP;
  			strName = req_get_cstream_var(wp, ("pptpUserName"), "");
			if ( strName[0] ) {
				if ( apmib_set(MIB_PPTP_USER_NAME, (void *)strName) == 0) {
					strcpy(tmpBuf, ("Set PPTP user name MIB error!"));
					goto setErr_tcpip;
				}
			}
 			strPassword = req_get_cstream_var(wp, ("pptpPassword"), "");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_PPTP_PASSWORD, (void *)strPassword) == 0) {
					strcpy(tmpBuf, ("Set PPTP user password MIB error!"));
					goto setErr_tcpip;
				}
			}
#if defined(CONFIG_DYNAMIC_WAN_IP)
			strWanIpType = req_get_cstream_var(wp, ("wan_pptp_use_dynamic_carrier_radio"), (""));
			if ( strWanIpType[0] ) {
				if (!strcmp(strWanIpType, ("dynamicIP")))
				{
					wanIpType= DYNAMIC_IP;
					
				}
				else if (!strcmp(strWanIpType, ("staticIP")))
				{
					wanIpType = STATIC_IP;
				}
				else {
					strcpy(tmpBuf, ("Invalid PPTP wan IP type!"));
					goto setErr_tcpip;
				}

				if ( !apmib_set(MIB_PPTP_WAN_IP_DYNAMIC, (void *)&wanIpType)) {
			  		strcpy(tmpBuf, ("Set MIB_PPTP_WAN_IP_DYNAMIC error!"));
					goto setErr_tcpip;
				}
			}
			
			strPPPGateway = req_get_cstream_var(wp, ("pptpDefGw"), (""));
			if ( strPPPGateway[0] ) {
				if ( !inet_aton(strPPPGateway, &inPPPGateway) ) {
					strcpy(tmpBuf, ("Invalid pptp default gateway value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_PPTP_DEFAULT_GW, (void *)&inPPPGateway)) {
					strcpy(tmpBuf, ("Set pptp default gateway error!"));
					goto setErr_tcpip;
				}
			}
#endif

#if defined(CONFIG_DYNAMIC_WAN_IP)
			if(wanIpType==STATIC_IP){
#endif
				strIp = req_get_cstream_var(wp, ("pptpIpAddr"), "");
				if ( strIp[0] ) {
					if ( !inet_aton(strIp, &inIp) ) {
						strcpy(tmpBuf, ("Invalid IP-address value!"));
						goto setErr_tcpip;
					}
					if ( !apmib_set(MIB_PPTP_IP_ADDR, (void *)&inIp)) {
						strcpy(tmpBuf, ("Set IP-address error!"));
						goto setErr_tcpip;
					}
				}

				strMask = req_get_cstream_var(wp, ("pptpSubnetMask"), "");
				if ( strMask[0] ) {
					if ( !inet_aton(strMask, &inMask) ) {
						strcpy(tmpBuf, ("Invalid subnet-mask value!"));
						goto setErr_tcpip;
					}
					if ( !apmib_set(MIB_PPTP_SUBNET_MASK, (void *)&inMask)) {
						strcpy(tmpBuf, ("Set subnet-mask error!"));
						goto setErr_tcpip;
					}
				}
#if defined(CONFIG_DYNAMIC_WAN_IP)
			}
#endif

			strGateway = req_get_cstream_var(wp, ("pptpServerIpAddr"), "");
			if ( strGateway[0] ) {
				if ( !inet_aton(strGateway, &inGateway) ) {
					strcpy(tmpBuf, ("Invalid pptp server ip value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_PPTP_SERVER_IP_ADDR, (void *)&inGateway)) {
					strcpy(tmpBuf, ("Set pptp server ip error!"));
					goto setErr_tcpip;
				}
			}

#if defined(CONFIG_GET_SERVER_IP_BY_DOMAIN)
			strGetServByDomain = req_get_cstream_var(wp,"pptpGetServMode","");
			if(strGetServByDomain[0])
			{
				if(!strcmp(strGetServByDomain,"pptpGetServByDomainName"))
				{
					intVal=1;
					if(!apmib_set(MIB_PPTP_GET_SERV_BY_DOMAIN,(void*)&intVal))
					{
						strcpy(tmpBuf, ("Set pptp get server by domain error!"));
							goto setErr_tcpip;
					}
					strGatewayDomain = req_get_cstream_var(wp, ("pptpServerDomainName"), "");
					if(strGatewayDomain[0])
					{
						if ( !apmib_set(MIB_PPTP_SERVER_DOMAIN, (void *)strGatewayDomain)) {
							strcpy(tmpBuf, ("Set pptp server domain error!"));
							goto setErr_tcpip;
						}
					}
				}else
				{
					intVal=0;
					if(!apmib_set(MIB_PPTP_GET_SERV_BY_DOMAIN,(void*)&intVal))
					{
						strcpy(tmpBuf, ("Set pptp get server by domain error!"));
							goto setErr_tcpip;
					}
				}
			}
			
#endif

		strType = req_get_cstream_var(wp, ("pptpConnectType"), "");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, ("Invalid PPTP type value!"));
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_PPTP_CONNECTION_TYPE, (void *)&type) == 0) {
   					strcpy(tmpBuf, ("Set PPTP type MIB error!"));
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, ("pptpIdleTime"), "");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_PPTP_IDLE_TIME, (void *)&time) == 0) {
   							strcpy(tmpBuf, ("Set PPTP idle time MIB error!"));
							goto setErr_tcpip;
						}
					}
				}
			}
			strVal = req_get_cstream_var(wp, ("pptpMtuSize"), "");
			if ( strVal[0] ) {
				int mtuSize;
 				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_PPTP_MTU_SIZE, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, ("Set PPTP mtu size MIB error!"));
					goto setErr_tcpip;
				}
			}
			if (!call_from_wizard) { // not called from wizard
				strVal = req_get_cstream_var(wp, ("pptpSecurity"), "");
				if ( !strcmp(strVal, "ON"))
					intVal = 1;
				else
					intVal = 0;
				apmib_set(MIB_PPTP_SECURITY_ENABLED, (void *)&intVal);

				strVal = req_get_cstream_var(wp, ("pptpCompress"), "");
				if ( !strcmp(strVal, "ON"))
					intVal = 1;
				else
					intVal = 0;
				apmib_set(MIB_PPTP_MPPC_ENABLED, (void *)&intVal);				
			}			
		}
		/* # keith: add l2tp support. 20080515 */
		else if ( !strcmp(strMode, "l2tp")) {
			char	*strName, *strPassword;
			dhcp = L2TP;
  			strName = req_get_cstream_var(wp, ("l2tpUserName"), "");
			if ( strName[0] ) {
				if ( apmib_set(MIB_L2TP_USER_NAME, (void *)strName) == 0) {
					strcpy(tmpBuf, ("Set L2TP user name MIB error!"));
					goto setErr_tcpip;
				}
			}
 			strPassword = req_get_cstream_var(wp, ("l2tpPassword"), "");
			if ( strPassword[0] ) {
				if ( apmib_set(MIB_L2TP_PASSWORD, (void *)strPassword) == 0) {
					strcpy(tmpBuf, ("Set L2TP user password MIB error!"));
					goto setErr_tcpip;
				}
			}
#if defined(CONFIG_DYNAMIC_WAN_IP)
			strWanIpType = req_get_cstream_var(wp, ("wan_l2tp_use_dynamic_carrier_radio"), (""));
			if ( strWanIpType[0] ) {
				if (!strcmp(strWanIpType, ("dynamicIP")))
					wanIpType= DYNAMIC_IP;
				else if (!strcmp(strWanIpType, ("staticIP")))
					wanIpType = STATIC_IP;
				else {
					strcpy(tmpBuf, ("Invalid L2TP wan IP type!"));
					goto setErr_tcpip;
				}

				if ( !apmib_set(MIB_L2TP_WAN_IP_DYNAMIC, (void *)&wanIpType)) {
			  		strcpy(tmpBuf, ("Set MIB_L2TP_WAN_IP_DYNAMIC error!"));
					goto setErr_tcpip;
				}
			}
			
			strPPPGateway = req_get_cstream_var(wp, ("l2tpDefGw"), (""));
			if ( strPPPGateway[0] ) {
				if ( !inet_aton(strPPPGateway, &inPPPGateway) ) {
					strcpy(tmpBuf, ("Invalid l2tp default gateway value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_L2TP_DEFAULT_GW, (void *)&inPPPGateway)) {
					strcpy(tmpBuf, ("Set l2tp default gateway error!"));
					goto setErr_tcpip;
				}
			}
#endif	

#if defined(CONFIG_DYNAMIC_WAN_IP)
			if(wanIpType==STATIC_IP){
#endif		
				strIp = req_get_cstream_var(wp, ("l2tpIpAddr"), "");
				if ( strIp[0] ) {
					if ( !inet_aton(strIp, &inIp) ) {
						strcpy(tmpBuf, ("Invalid IP-address value!"));
						goto setErr_tcpip;
					}
					if ( !apmib_set(MIB_L2TP_IP_ADDR, (void *)&inIp)) {
						strcpy(tmpBuf, ("Set IP-address error!"));
						goto setErr_tcpip;
					}
				}

				strMask = req_get_cstream_var(wp, ("l2tpSubnetMask"), "");
				if ( strMask[0] ) {
					if ( !inet_aton(strMask, &inMask) ) {
						strcpy(tmpBuf, ("Invalid subnet-mask value!"));
						goto setErr_tcpip;
					}
					if ( !apmib_set(MIB_L2TP_SUBNET_MASK, (void *)&inMask)) {
						strcpy(tmpBuf, ("Set subnet-mask error!"));
						goto setErr_tcpip;
					}
				}
#if defined(CONFIG_DYNAMIC_WAN_IP)
			}
#endif		
			strGateway = req_get_cstream_var(wp, ("l2tpServerIpAddr"), "");
			if ( strGateway[0] ) {
				if ( !inet_aton(strGateway, &inGateway) ) {
					strcpy(tmpBuf, ("Invalid l2tp server ip value!"));
					goto setErr_tcpip;
				}
				if ( !apmib_set(MIB_L2TP_SERVER_IP_ADDR, (void *)&inGateway)) {
					strcpy(tmpBuf, ("Set pptp server ip error!"));
					goto setErr_tcpip;
				}
			}
			
#if defined(CONFIG_GET_SERVER_IP_BY_DOMAIN)
			strGetServByDomain = req_get_cstream_var(wp,"l2tpGetServMode","");
			if(strGetServByDomain[0])
			{
				if(!strcmp(strGetServByDomain,"l2tpGetServByDomainName"))
				{
					intVal=1;
					if(!apmib_set(MIB_L2TP_GET_SERV_BY_DOMAIN,(void*)&intVal))
					{
						strcpy(tmpBuf, ("Set l2tp get server by domain error!"));
							goto setErr_tcpip;
					}
					strGatewayDomain = req_get_cstream_var(wp, ("l2tpServerDomainName"), "");
					if(strGatewayDomain[0])
					{
						if ( !apmib_set(MIB_L2TP_SERVER_DOMAIN, (void *)strGatewayDomain)) {
							strcpy(tmpBuf, ("Set l2tp server domain error!"));
							goto setErr_tcpip;
						}
					}
				}else
				{
					intVal=0;
					if(!apmib_set(MIB_L2TP_GET_SERV_BY_DOMAIN,(void*)&intVal))
					{
						strcpy(tmpBuf, ("Set l2tp get server by domain error!"));
							goto setErr_tcpip;
					}
				}
			}
			
#endif

		strType = req_get_cstream_var(wp, ("l2tpConnectType"), "");
			if ( strType[0] ) {
				PPP_CONNECT_TYPE_T type;
				if ( strType[0] == '0' )
					type = CONTINUOUS;
				else if ( strType[0] == '1' )
					type = CONNECT_ON_DEMAND;
				else if ( strType[0] == '2' )
					type = MANUAL;
				else {
					strcpy(tmpBuf, ("Invalid L2TP type value!"));
					goto setErr_tcpip;
				}
				if ( apmib_set(MIB_L2TP_CONNECTION_TYPE, (void *)&type) == 0) {
   					strcpy(tmpBuf, ("Set L2TP type MIB error!"));
					goto setErr_tcpip;
				}
				if (type != CONTINUOUS) {
					char *strTime;
					strTime = req_get_cstream_var(wp, ("l2tpIdleTime"), "");
					if ( strTime[0] ) {
						int time;
 						time = strtol(strTime, (char**)NULL, 10) * 60;
						if ( apmib_set(MIB_L2TP_IDLE_TIME, (void *)&time) == 0) {
   							strcpy(tmpBuf, ("Set L2TP idle time MIB error!"));
							goto setErr_tcpip;
						}
					}
				}
			}
			strVal = req_get_cstream_var(wp, ("l2tpMtuSize"), "");
			if ( strVal[0] ) {
				int mtuSize;
 				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_L2TP_MTU_SIZE, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, ("Set L2TP mtu size MIB error!"));
					goto setErr_tcpip;
				}
			}
				
		}

#ifdef RTK_USB3G
        else if ( !strcmp(strMode, ("USB3G"))) {
            char  *strName, *strPassword, *strPIN, *strAPN, *strDialnum;
            dhcp = USB3G;
            strName = req_get_cstream_var(wp, ("USB3G_USER"), "");
            //if ( strName[0] ) {
                if ( apmib_set(MIB_USB3G_USER, (void *)strName) == 0) {
                    strcpy(tmpBuf, ("Set USB3G user name MIB error!"));
                    goto setErr_tcpip;
                }
            //}
            strPassword = req_get_cstream_var(wp, ("USB3G_PASS"), "");
            //if ( strPassword[0] ) {
                if ( apmib_set(MIB_USB3G_PASS, (void *)strPassword) == 0) {
                    strcpy(tmpBuf, ("Set USB3G user password MIB error!"));
                    goto setErr_tcpip;
                }
            //}
            strPIN = req_get_cstream_var(wp, ("USB3G_PIN"), "");
            //if ( strPIN[0] ) {
                if ( apmib_set(MIB_USB3G_PIN, (void *)strPIN) == 0) {
                    strcpy(tmpBuf, ("Set USB3G PIN MIB error!"));
                    goto setErr_tcpip;
                }
            //}            
            strAPN = req_get_cstream_var(wp, ("USB3G_APN"), "");
            if ( strAPN[0] ) {
                if ( apmib_set(MIB_USB3G_APN, (void *)strAPN) == 0) {
                    strcpy(tmpBuf, ("Set USB3G APN MIB error!"));
                    goto setErr_tcpip;
                }
            }
            strDialnum = req_get_cstream_var(wp, ("USB3G_DIALNUM"), "");
            if ( strDialnum[0] ) {
                if ( apmib_set(MIB_USB3G_DIALNUM, (void *)strDialnum) == 0) {
                    strcpy(tmpBuf, ("Set USB3G Dial number MIB error!"));
                    goto setErr_tcpip;
                }
            }

            strDialnum = req_get_cstream_var(wp, ("USB3GMtuSize"), "");
            if ( strDialnum[0] ) {
                if ( apmib_set(MIB_USB3G_MTU_SIZE, (void *)strDialnum) == 0) {
                    strcpy(tmpBuf, ("Set USB3G mtu size MIB error!"));
                    goto setErr_tcpip;
                }
            }

            strType = req_get_cstream_var(wp, ("USB3GConnectType"), "");
            if ( strType[0] ) {
                PPP_CONNECT_TYPE_T type;
                if (!strcmp(strType, "0"))
                    type = CONTINUOUS;
                else if (!strcmp(strType, "1"))
                    type = CONNECT_ON_DEMAND;
                else if (!strcmp(strType, "2"))
                    type = MANUAL;
                else {
                    strcpy(tmpBuf, ("Invalid USB3G type value!"));
                    goto setErr_tcpip;
                }
                if ( apmib_set(MIB_USB3G_CONN_TYPE, (void *)strType) == 0) {
                    strcpy(tmpBuf, ("Set USB3G type MIB error!"));
                    goto setErr_tcpip;
                }
                if (type != CONTINUOUS) {
                    char *strTime;
                    strTime = req_get_cstream_var(wp, ("USB3GIdleTime"), "");
                    if ( strTime[0] ) {
                        int time;
                        char buffer[8];
                        time = atoi(strTime) * 60;
                        sprintf(buffer, "%d", time);
                        if ( apmib_set(MIB_USB3G_IDLE_TIME, (void *)buffer) == 0) {
                            strcpy(tmpBuf, ("Set USB3G idle time MIB error!"));
                            goto setErr_tcpip;
                        }
                    }
                }
            }
        }
#endif /* #ifdef RTK_USB3G */

		else {
			strcpy(tmpBuf, ("Invalid IP mode value!"));
			goto setErr_tcpip;
		}
		if ( !apmib_set(MIB_WAN_DHCP, (void *)&dhcp)) {
	  		strcpy(tmpBuf, ("Set DHCP error!"));
			goto setErr_tcpip;
		}
#if defined(ROUTE_SUPPORT)		
	if ( (dhcp == PPPOE) || (dhcp == PPTP) || (dhcp == L2TP) || (dhcp == USB3G) ) {
		curr_nat=1;
		
		if(curr_nat !=orig_nat){//force NAT is enabled when pppoe/pptp/l2tp
			if ( !apmib_set( MIB_NAT_ENABLED, (void *)&curr_nat) ) {
				strcpy(tmpBuf, ("Get NAT MIB error!"));
				goto setErr_tcpip;
			}
			intVal=0;
			if (apmib_set( MIB_RIP_LAN_TX, (void *)&intVal) == 0) {
					strcpy(tmpBuf, ("\"Set RIP LAN Tx error!\""));
					goto setErr_tcpip;
			}
			if (apmib_set( MIB_RIP_WAN_TX, (void *)&intVal) == 0) {
					strcpy(tmpBuf, ("\"Set RIP WAN Tx error!\""));
					goto setErr_tcpip;
			}
			if (!apmib_set(MIB_IGMP_PROXY_DISABLED, (void *)&intVal)) {
				strcpy(tmpBuf, ("Set MIB_IGMP_PROXY_DISABLED error!"));
				goto setErr_tcpip;
			}
		}
	}
#endif	

        if ( buttonState == 1 && (dhcp == PPPOE || dhcp == PPTP || dhcp == L2TP || dhcp == USB3G) ) { // connect button is pressed
			int wait_time=30;
			int opmode=0;
			apmib_update_web(CURRENT_SETTING);	// update to flash
			apmib_get(MIB_OP_MODE, (void *)&opmode);
			
#ifdef MULTI_PPPOE
			if(buttonState == 1 && dhcp == PPPOE)
			{
				extern int PPPoE_Number;
				int ppp_num;
				FILE *pF;
				system("ifconfig |grep 'ppp'| cut -d ' ' -f 1 |  wc -l > /etc/ppp/lineNumber");	

				if(flag ==1){
					PPPoE_Number = 1;
					system("echo 1 > /etc/ppp/connfile1");
					system("rm /etc/ppp/disconnect_trigger1 >/dev/null 2>&1");
				}else if(flag ==2){					
					PPPoE_Number = 2;				
					system("echo 1 > /etc/ppp/connfile2");
					system("rm /etc/ppp/disconnect_trigger2 >/dev/null 2>&1");		
				}else if(flag ==3){
					PPPoE_Number = 3;				
					system("echo 1 > /etc/ppp/connfile3");
					system("rm /etc/ppp/disconnect_trigger3 >/dev/null 2>&1");
				}else if(flag ==4){								
					PPPoE_Number = 4;				
					system("echo 1 > /etc/ppp/connfile4");
					system("rm /etc/ppp/disconnect_trigger4 >/dev/null 2>&1");											
				}		
				system("rm /etc/ppp/connectfile >/dev/null 2>&1");
				if((pF = fopen("/etc/ppp/lineNumber","r+")) != NULL)
				{
					fscanf(pF,"%d",&ppp_num);
					if(ppp_num == 0)
					{						
						system("killall -9 ppp_inet 2> /dev/null");
						goto End;
					}
				}
				while (wait_time-- >0) {
					if (isConnectPPP()){
						printf("PPP is connected\n");
						break;
					}
					sleep(1);
				}
				if (isConnectPPP())
					strcpy(tmpBuf, ("Connected to server successfully.\n"));
				else
					strcpy(tmpBuf, ("Connect to server failed!\n"));	
				OK_MSG1(tmpBuf, submitUrl);			
				return 1;				
			}
End:			
#endif
			if(opmode==2)//WISP mode
				WAN_IF = ("wlan0");
			else if(opmode ==0) //Gateway mode
				WAN_IF = ("eth1");

			system("killall -9 igmpproxy 2> /dev/null");
			system("echo 1,0 > /proc/br_mCastFastFwd");
			system("killall -9 dnrd 2> /dev/null");
			if(dhcp == PPPOE || dhcp == PPTP)
				system("killall -15 pppd 2> /dev/null");
        #ifdef RTK_USB3G
            else if (dhcp == USB3G)
                kill_3G_ppp_inet();
        #endif
			else
				system("killall -9 pppd 2> /dev/null");
				
				system("disconnect.sh option");
#ifndef NO_ACTION
        #ifdef RTK_USB3G
            if (dhcp == USB3G)
                system("ppp_inet -t 16 -c 0 -x");
            else {
        #endif
			pid = fork();
        		if (pid)
	        		waitpid(pid, NULL, 0);
			else if (pid == 0) {
				if(dhcp == PPPOE){
					snprintf(tmpBuf, 100, "%s/%s", _CONFIG_SCRIPT_PATH, _PPPOE_SCRIPT_PROG);
					execl( tmpBuf, _PPPOE_SCRIPT_PROG, "connect", WAN_IF, NULL);
				}else if(dhcp == PPTP){
					snprintf(tmpBuf, 100, "%s/%s", _CONFIG_SCRIPT_PATH, _PPTP_SCRIPT_PROG);
					execl( tmpBuf, _PPTP_SCRIPT_PROG, "connect", WAN_IF, NULL);
				}else if(dhcp == L2TP){
					system("killall -9 l2tpd 2> /dev/null");
					system("rm -f /var/run/l2tpd.pid 2> /dev/null");
					snprintf(tmpBuf, 100, "%s/%s", _CONFIG_SCRIPT_PATH, _L2TP_SCRIPT_PROG);
					execl( tmpBuf, _L2TP_SCRIPT_PROG, "connect", WAN_IF, NULL);
				}
       				exit(1);
			}
        #ifdef RTK_USB3G
            }
        #endif
			while (wait_time-- >0) {
				if (isConnectPPP()){
					printf("PPP is connected\n");
					break;
				}
				sleep(1);
			}
			if (isConnectPPP())
				strcpy(tmpBuf, ("Connected to server successfully.\n"));
			else
				strcpy(tmpBuf, ("Connect to server failed!\n"));

			OK_MSG1(tmpBuf, submitUrl);
#endif
			return 1;
		}

		if ( buttonState == 2 && (dhcp == PPPOE || dhcp == PPTP || dhcp == L2TP || dhcp == USB3G) ) { // disconnect button is pressed
			apmib_update_web(CURRENT_SETTING);	// update to flash


#ifdef MULTI_PPPOE
		if ( buttonState == 2 && dhcp == PPPOE)
		{
			char ppp_name[5];
			int orderNumber,pppNumbers,index;
			FILE *order,*number;			
			int wait_time=30;
			if((order=fopen("/etc/ppp/ppp_order_info","r+"))==NULL)
			{
				printf("Cannot open this file\n");
				goto end;
			}
			if((number=fopen("/etc/ppp/lineNumber","r+"))==NULL)
			{
				printf("Cannot open this file\n");
				goto end;
			}
			fscanf(number,"%d",&pppNumbers);
			close(order);
			close(number);										
			for( index = 0 ; index < pppNumbers ; ++index)
			{			
				fscanf(order,"%d--%s",&orderNumber,ppp_name);
				if(flag == orderNumber)
				{
					int pid;
					char path[50],cmd[50];
					FILE *pidF;				
					extern int PPPoE_Number;
					sprintf(path,"/var/run/%s.pid",ppp_name);
					if((pidF=fopen(path,"r+")) == NULL)
						goto end;									
					fscanf(pidF,"%d",&pid);
					if(flag ==1){
						system("echo 1 > /etc/ppp/disconnect_trigger1");
						PPPoE_Number = 1;
					}
					else if(flag == 2){
						system("echo 1 > /etc/ppp/disconnect_trigger2");
						PPPoE_Number = 2;
					}
					else if(flag ==3){
						system("echo 1 > /etc/ppp/disconnect_trigger3");
						PPPoE_Number = 3;
					}
					else if(flag ==4){
						system("echo 1 > /etc/ppp/disconnect_trigger4");
						PPPoE_Number = 4;
					}
					sprintf(cmd,"kill %d  2> /dev/null",pid);
					system(cmd);									
					system("rm /etc/ppp/connectfile >/dev/null 2>&1");	
					while (wait_time-- >0) {
						if (!isConnectPPP()){
							printf("PPP is disconnected\n");
							break;
						}
						sleep(1);
					}
					if (!isConnectPPP())
						strcpy(tmpBuf, ("PPPoE disconnected.\n"));
					else
						strcpy(tmpBuf, ("Unknown\n"));
	
					OK_MSG1(tmpBuf, submitUrl);			
					return 1;			
				}
			}

		}
end:
#endif

#ifndef NO_ACTION
        #ifdef RTK_USB3G
            if (dhcp == USB3G)
                kill_3G_ppp_inet();
            else
        #endif
		if(dhcp != PPTP){
			pid = fork();
        		if (pid)
	             		waitpid(pid, NULL, 0);
        		else if (pid == 0) {
				snprintf(tmpBuf, 100, "%s/%s", _CONFIG_SCRIPT_PATH, _PPPOE_DC_SCRIPT_PROG);
				execl( tmpBuf, _PPPOE_DC_SCRIPT_PROG, "all", NULL);
                		exit(1);
        		}
        	}else{
        		system("killall -15 ppp_inet 2> /dev/null");
        		system("killall -15 pppd 2> /dev/null");
        	}

        		if(dhcp == PPPOE)	
			strcpy(tmpBuf, ("PPPoE disconnected.\n"));
			if(dhcp == PPTP)	
			strcpy(tmpBuf, ("PPTP disconnected.\n"));
			if(dhcp == L2TP)	
			strcpy(tmpBuf, ("L2TP disconnected.\n"));
            if(dhcp == USB3G)    
                strcpy(tmpBuf, ("USB3G disconnected.\n"));

			OK_MSG1(tmpBuf, submitUrl);
#endif
			return 1;
		}
	}
	else
		dhcp = curDhcp;

	if ( dhcp == DHCP_DISABLED ) {
		strIp = req_get_cstream_var(wp, ("wan_ip"), "");
		if ( strIp[0] ) {
			if ( !inet_aton(strIp, &inIp) ) {
				strcpy(tmpBuf, ("Invalid IP-address value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set(MIB_WAN_IP_ADDR, (void *)&inIp)) {
				strcpy(tmpBuf, ("Set IP-address error!"));
				goto setErr_tcpip;
			}
		}

		strMask = req_get_cstream_var(wp, ("wan_mask"), "");
		if ( strMask[0] ) {
			if ( !inet_aton(strMask, &inMask) ) {
				strcpy(tmpBuf, ("Invalid subnet-mask value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set(MIB_WAN_SUBNET_MASK, (void *)&inMask)) {
				strcpy(tmpBuf, ("Set subnet-mask error!"));
				goto setErr_tcpip;
			}
		}

		strGateway = req_get_cstream_var(wp, ("wan_gateway"), "");
		if ( strGateway[0] ) {
			if ( !inet_aton(strGateway, &inGateway) ) {
				strcpy(tmpBuf, ("Invalid default-gateway value!"));
				goto setErr_tcpip;
			}
			if ( !apmib_set(MIB_WAN_DEFAULT_GATEWAY, (void *)&inGateway)) {
				strcpy(tmpBuf, ("Set default-gateway error!"));
				goto setErr_tcpip;
			}
		}

		strVal = req_get_cstream_var(wp, ("fixedIpMtuSize"), "");
		if ( strVal[0] ) {
			int mtuSize;
			mtuSize = strtol(strVal, (char**)NULL, 10);
			if ( apmib_set(MIB_FIXED_IP_MTU_SIZE, (void *)&mtuSize) == 0) {
				strcpy(tmpBuf, ("Set FIXED-IP mtu size MIB error!"));
				goto setErr_tcpip;
			}
		}		
	}
	
	if (!call_from_wizard) { // not called from wizard
		if (dhcp == DHCP_CLIENT) {
			strVal = req_get_cstream_var(wp, ("dhcpMtuSize"), "");
			if ( strVal ) {
				int mtuSize;
				mtuSize = strtol(strVal, (char**)NULL, 10);
				if ( apmib_set(MIB_DHCP_MTU_SIZE, (void *)&mtuSize) == 0) {
					strcpy(tmpBuf, ("Set DHCP mtu size MIB error!"));
					goto setErr_tcpip;
				}
			}
			
			strVal = req_get_cstream_var(wp, ("hostName"), "");
			if (strVal) {
				if (!isValidName(strVal)) {
  					strcpy(tmpBuf, ("Invalid Host Name! Please enter characters in A(a)~Z(z) or 0-9 without spacing."));
					goto setErr_tcpip;				
				}			
				if ( !apmib_set(MIB_HOST_NAME, (void *)strVal)) {
  					strcpy(tmpBuf, ("Set MIB_HOST_NAME MIB error!"));
					goto setErr_tcpip;
				}
			}else{
				 if ( !apmib_set(MIB_HOST_NAME, (void *)"")){
	  					strcpy(tmpBuf, ("\"Set MIB_HOST_NAME MIB error!\""));
						goto setErr_tcpip;
				}	
			}					
		}	
		
		strVal = req_get_cstream_var(wp, ("upnpEnabled"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
		if ( !apmib_set(MIB_UPNP_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set MIB_UPNP_ENABLED error!"));
			goto setErr_tcpip;
		}
//Brad add for igmpproxy
		strVal = req_get_cstream_var(wp, ("igmpproxyEnabled"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 0;
		else
			intVal = 1;
		if ( !apmib_set(MIB_IGMP_PROXY_DISABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set MIB_IGMP_PROXY_DISABLED error!"));
			goto setErr_tcpip;
		}
//Brad add end
		strVal = req_get_cstream_var(wp, ("webWanAccess"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
		if ( !apmib_set(MIB_WEB_WAN_ACCESS_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set WEB_WAN_ACCESS_ENABLED error!"));
			goto setErr_tcpip;
		}
		
		strVal = req_get_cstream_var(wp, ("pingWanAccess"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
		if ( !apmib_set(MIB_PING_WAN_ACCESS_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set PING_WAN_ACCESS_ENABLED error!"));
			goto setErr_tcpip;
		}		
			
		strVal = req_get_cstream_var(wp, ("WANPassThru1"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;	
		if ( !apmib_set(MIB_VPN_PASSTHRU_IPSEC_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set VPN_PASSTHRU_IPSEC_ENABLED error!"));
			goto setErr_tcpip;
		}

		strVal = req_get_cstream_var(wp, ("WANPassThru2"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
		if ( !apmib_set(MIB_VPN_PASSTHRU_PPTP_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set VPN_PASSTHRU_PPTP_ENABLED error!"));
			goto setErr_tcpip;
		}
		
		strVal = req_get_cstream_var(wp, ("WANPassThru3"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
		if ( !apmib_set(MIB_VPN_PASSTHRU_L2TP_ENABLED, (void *)&intVal)) {
			strcpy(tmpBuf, ("Set VPN_PASSTHRU_L2TP_ENABLED error!"));
			goto setErr_tcpip;
		}
		strVal = req_get_cstream_var(wp, ("ipv6_passthru_enabled"), "");
		if ( !strcmp(strVal, "ON"))
			intVal = 1;
		else
			intVal = 0;
	        if ( !apmib_set(MIB_CUSTOM_PASSTHRU_ENABLED, (void *)&intVal))
	        {
	                strcpy(tmpBuf, ("Set custom passthru enabled error!"));
	                goto setErr_tcpip;
	        }
		
	}
	return 0 ;
setErr_tcpip:
	return -1 ;	
}	



////////////////////////////////////////////////////////////////////////////////

void formWanTcpipSetup(request *wp, char *path, char *query)
{


	char tmpBuf[100];
	int dns_changed=0;
	char *arg;
	char *submitUrl;
	int val ;
		
	submitUrl = req_get_cstream_var(wp, "submit-url", "");   // hidden page

	if((val = tcpipWanHandler(wp, tmpBuf, &dns_changed)) < 0 )
		goto setErr_end ;
	else if (val == 1) // return ok
		return ;

	apmib_update_web(CURRENT_SETTING);	// update to flash

	// run script
	if ( dns_changed )
		arg = "all";
	else
		arg = "wan";

#ifdef UNIVERSAL_REPEATER
	apmib_get(MIB_REPEATER_ENABLED1, (void *)&val);
	if (val) 
		arg = "all";	
#endif	

#ifndef NO_ACTION
	run_init_script(arg);                
#endif

	OK_MSG(submitUrl);

	return;
setErr_end:
	ERR_MSG(tmpBuf);
}
#endif
//////////////////////////////////////////////////////////////////////////////
//Static DHCP 
void formStaticDHCP(request *wp, char *path, char *query)
{
	char *strStp, *strIp, *strHostName, *strAddRsvIP, *strDelRsvIP, *strDelAllRsvIP, *strVal, *submitUrl;
	char tmpBuf[100];
	char buffer[100];
	int entryNum, i, stp;
	DHCPRSVDIP_T staticIPEntry, delEntry;
	struct in_addr inIp;
	struct in_addr inLanaddr_orig;
	struct in_addr inLanmask_orig;
	strAddRsvIP = req_get_cstream_var(wp, ("addRsvIP"), "");
	strDelRsvIP = req_get_cstream_var(wp, ("deleteSelRsvIP"), "");
	strDelAllRsvIP = req_get_cstream_var(wp, ("deleteAllRsvIP"), "");

//displayPostDate(wp->post_data);

	apmib_get( MIB_IP_ADDR,  (void *)buffer); //save the orig lan subnet
	memcpy((void *)&inLanaddr_orig, buffer, 4);
	
	apmib_get( MIB_SUBNET_MASK,  (void *)buffer); //save the orig lan mask
	memcpy((void *)&inLanmask_orig, buffer, 4);
	
	// Set static DHCP
	strStp = req_get_cstream_var(wp, ("static_dhcp"), "");
	if (strStp[0]) {
		if (strStp[0] == '0')
			stp = 0;
		else
			stp = 1;
		if ( !apmib_set(MIB_DHCPRSVDIP_ENABLED, (void *)&stp)) {
			strcpy(tmpBuf, ("Set static DHCP mib error!"));
			goto setErr_rsv;
		}
	}
	
	if (strAddRsvIP[0]) {
		memset(&staticIPEntry, '\0', sizeof(staticIPEntry));	
		strHostName = (char *)req_get_cstream_var(wp, ("hostname"), "");	
		if (strHostName[0])
			strcpy((char *)staticIPEntry.hostName, strHostName);				
		strIp = req_get_cstream_var(wp,( "ip_addr"), "");
		if (strIp[0]) {
			inet_aton(strIp, &inIp);
			memcpy(staticIPEntry.ipAddr, &inIp, 4);
		}
		strVal = req_get_cstream_var(wp, ("mac_addr"), "");
		if ( !strVal[0] ) {
	//		strcpy(tmpBuf, ("Error! No mac address to set."));
			goto setac_ret;
		}
		if (strlen(strVal)!=12 || !string_to_hex(strVal, staticIPEntry.macAddr, 12)) {
			strcpy(tmpBuf, ("Error! Invalid MAC address."));
			goto setErr_rsv;
		}
		if ( !apmib_get(MIB_DHCPRSVDIP_TBL_NUM, (void *)&entryNum)) {
			strcpy(tmpBuf, ("Get entry number error!"));
			goto setErr_rsv;
		}
		if ( (entryNum + 1) > MAX_DHCP_RSVD_IP_NUM) {
			strcpy(tmpBuf, ("Cannot add new entry because table is full!"));
			goto setErr_rsv;
		}
		if((inLanaddr_orig.s_addr & inLanmask_orig.s_addr) != (inIp.s_addr & inLanmask_orig.s_addr)){
			strcpy(tmpBuf, ("Cannot add new entry because the ip is not the same subnet as LAN network!"));
			goto setErr_rsv;
		}
	
	
		// set to MIB. try to delete it first to avoid duplicate case
		apmib_set(MIB_DHCPRSVDIP_DEL, (void *)&staticIPEntry);
		if ( apmib_set(MIB_DHCPRSVDIP_ADD, (void *)&staticIPEntry) == 0) {
			strcpy(tmpBuf, ("Add table entry error!"));
			goto setErr_rsv;
		}
	}

	/* Delete entry */
	if (strDelRsvIP[0]) {
		if ( !apmib_get(MIB_DHCPRSVDIP_TBL_NUM, (void *)&entryNum)) {
			strcpy(tmpBuf, ("Get entry number error!"));
			goto setErr_rsv;
		}
		for (i=entryNum; i>0; i--) {
			snprintf(tmpBuf, 20, "select%d", i);
			memset(&delEntry, '\0', sizeof(delEntry));	
			strVal = req_get_cstream_var(wp, tmpBuf, "");
			if ( !strcmp(strVal, "ON") ) {

				*((char *)&delEntry) = (char)i;
				if ( !apmib_get(MIB_DHCPRSVDIP_TBL, (void *)&delEntry)) {
					strcpy(tmpBuf, ("Get table entry error!"));
					goto setErr_rsv;
				}
				if ( !apmib_set(MIB_DHCPRSVDIP_DEL, (void *)&delEntry)) {
					strcpy(tmpBuf, ("Delete table entry error!"));
					goto setErr_rsv;
				}
			}
		}
	}

	/* Delete all entry */
	if ( strDelAllRsvIP[0]) {
		if ( !apmib_set(MIB_DHCPRSVDIP_DELALL, (void *)&staticIPEntry)) {
			strcpy(tmpBuf, ("Delete all table error!"));
			goto setErr_rsv;
		}
	}

setac_ret:
	apmib_update_web(CURRENT_SETTING);

#ifndef NO_ACTION
	run_init_script("all");
#endif

	submitUrl = req_get_cstream_var(wp, "submit-url", "");   // hidden page
	OK_MSG( submitUrl );
  	return;

setErr_rsv:
	ERR_MSG(tmpBuf);
}


int dhcpRsvdIp_List(request *wp, int argc, char **argv)
{
	int	entryNum, i;
	int nBytesSent=0;
	DHCPRSVDIP_T entry;
	char macaddr[30];
	apmib_get(MIB_DHCPRSVDIP_TBL_NUM, (void *)&entryNum);
	nBytesSent += req_format_write(wp, ("<tr>"
      	"<td align=center width=\"30%%\" bgcolor=\"#808080\"><font size=\"2\"><b>IP Address</b></font></td>\n"
      	"<td align=center width=\"30%%\" bgcolor=\"#808080\"><font size=\"2\"><b>MAC Address</b></font></td>\n"
      	"<td align=center width=\"30%%\" bgcolor=\"#808080\"><font size=\"2\"><b>Comment</b></font></td>\n"
      	"<td align=center width=\"10%%\" bgcolor=\"#808080\"><font size=\"2\"><b>Select</b></font></td></tr>\n"));
	for (i=1; i<=entryNum; i++) {
		*((char *)&entry) = (char)i;
		apmib_get(MIB_DHCPRSVDIP_TBL, (void *)&entry);
		if (!memcmp(entry.macAddr, "\x0\x0\x0\x0\x0\x0", 6))
			macaddr[0]='\0';
		else			
			sprintf(macaddr," %02x-%02x-%02x-%02x-%02x-%02x", entry.macAddr[0], entry.macAddr[1], entry.macAddr[2], entry.macAddr[3], entry.macAddr[4], entry.macAddr[5]);
		nBytesSent += req_format_write(wp, ("<tr>"
			"<td align=center width=\"30%%\" bgcolor=\"#C0C0C0\"><font size=\"2\">%s</td>\n"
			"<td align=center width=\"30%%\" bgcolor=\"#C0C0C0\"><font size=\"2\">%s</td>\n"
      			"<td align=center width=\"30%%\" bgcolor=\"#C0C0C0\"><font size=\"2\">%s</td>\n"
       			"<td align=center width=\"10%%\" bgcolor=\"#C0C0C0\"><input type=\"checkbox\" name=\"select%d\" value=\"ON\"></td></tr>\n"),
			inet_ntoa(*((struct in_addr*)entry.ipAddr)), macaddr,entry.hostName, i);	
	}
	return 0;
}
//eded test start for TEW815 log
int udhcpd_leases(request *wp, int argc, char **argv)
{
   
	FILE *fp;
	int nBytesSent=0;
	int element=0, ret;
	char ipAddr[40], macAddr[40], liveTime[80], *buf=NULL, *ptr, tmpBuf[100], hostName[64];
	struct stat status;
	int pid;
	//unsigned long fileSize=0;
	char cStr_1[16],cStr_2[16];
	int D_Quotient,D_Remainder,H_Quotient,H_Remainder,M_Quotient,M_Remainder;
	int lease_time;
	
	
	// siganl DHCP server to update lease file
	snprintf(tmpBuf, 100, "%s/%s.pid", _DHCPD_PID_PATH, _DHCPD_PROG_NAME);
	pid = getPid(tmpBuf);
	snprintf(tmpBuf, 100, "kill -SIGUSR1 %d\n", pid);
	
	if ( pid > 0)
		//kill(pid, SIGUSR1);
		system(tmpBuf);
	
	usleep(1000);

	if ( stat(_PATH_DHCPS_LEASES, &status) < 0 ){
		goto err;
	}
	//fileSize = status.st_size;
	buf = malloc(status.st_size);
	if ( buf == NULL ){
		goto err;
	}
	fp = fopen(_PATH_DHCPS_LEASES, "r");
	if ( fp == NULL ){
		goto err;
	}
	fread(buf, sizeof(status.st_size)/*1*/, status.st_size, fp);
	//fread(buf, 1, fileSize, fp);
	fclose(fp);
	ptr = buf;
	while (1) {
		ret = getOneDhcpClient(&ptr, &status.st_size, hostName, ipAddr, macAddr, liveTime);
		
		if (ret < 0)
			break;
		if (ret == 0)
			continue;
		 
		
	  
		nBytesSent += req_format_write(wp,
			("get_by_id(\"syslog_select\").options.add(new Option(\"%d\",%%d, false, false));"
			),
		hostName,element);
			element++;
	}
err:
	if (element == 0) {
		nBytesSent += req_format_write(wp,
			("get_by_id(\"syslog_select\").options.add(new Option(\"---\",0, false, false));"
			"\n"
			));
	}
	if (buf)
		free(buf);

	return nBytesSent;
}
//eded test end
/////////////////////////////////////////////////////////////////////////////
int dhcpClientList(request *wp, int argc, char **argv)
{
	FILE *fp;
	int nBytesSent=0;
	int element=0, ret;
	char ipAddr[40], macAddr[40], liveTime[80], *buf=NULL, *ptr, tmpBuf[100], hostName[64];
	//unsigned long fileSize;
	struct stat status_dhcplist;
	int pid;
	unsigned long fileSize=0;
	char cStr_1[16],cStr_2[16];
	int D_Quotient,D_Remainder,H_Quotient,H_Remainder,M_Quotient,M_Remainder;
	int lease_time;
	// siganl DHCP server to update lease file
	snprintf(tmpBuf, 100, "%s/%s.pid", _DHCPD_PID_PATH, _DHCPD_PROG_NAME);
	pid = getPid(tmpBuf);
	snprintf(tmpBuf, 100, "kill -SIGUSR1 %d\n", pid);
	
	if ( pid > 0)
		//kill(pid, SIGUSR1);
		system(tmpBuf);
	
	usleep(1000);

	if ( stat(_PATH_DHCPS_LEASES, &status_dhcplist) < 0 ){
		goto err;
	}

	fileSize = status_dhcplist.st_size;
	buf = malloc(fileSize);

	if ( buf == NULL ){
		goto err;
	}
	fp = fopen(_PATH_DHCPS_LEASES, "r");
	if ( fp == NULL ){
		goto err;
	}
	//fread(buf, sizeof(status_dhcplist.st_size)/*1*/, status_dhcplist.st_size, fp);
	fread(buf, 1, fileSize, fp);
	fclose(fp);
	ptr = buf;
   
	while (1) {
		ret = getOneDhcpClient(&ptr, &fileSize, hostName, ipAddr, macAddr, liveTime);
		if (ret < 0)
			break;
		if (ret == 0)
			continue;
		lease_time = atoi(liveTime);
		D_Quotient = lease_time / (24*60*60);
		D_Remainder = lease_time % (24*60*60);
		if(D_Quotient)
		{
			H_Quotient = D_Remainder / (60*60);
			strcpy(cStr_1, retLS("00lan027"));
			strcpy(cStr_2, retLS("00lan025"));
			sprintf(liveTime,"%d %s %d %s",D_Quotient, cStr_1, H_Quotient, cStr_2);
		}
		else
		{
			H_Quotient = D_Remainder / (60*60);
			H_Remainder = D_Remainder % (60*60);
			M_Quotient = H_Remainder / 60;
			M_Remainder = H_Remainder % 60;
			strcpy(cStr_1, retLS("00lan025"));
			strcpy(cStr_2, retLS("00lan059"));
			sprintf(liveTime,"%d %s %d %s",H_Quotient, cStr_1, M_Quotient, cStr_2);
		}	
		
	
		nBytesSent += req_format_write(wp,
			("<tr bgcolor=#b7b7b7><td><font size=2>%s</td>"
			"<td><font size=2>%s</td>"
			"<td><font size=2>%s</td>"
			"<td><font size=2>%s</td>"
			"</tr>\n"
			),
		hostName, ipAddr, macAddr, liveTime);
			element++;
	}
err:
	if (element == 0) {
		nBytesSent += req_format_write(wp,
			("<tr bgcolor=#b7b7b7><td><font size=2>None</td>"
			"<td><font size=2>----</td>"			
			"<td><font size=2>----</td>"
			"<td><font size=2>----</td>"
			"</tr>\n"
			));
	}
	if (buf)
		free(buf);

	return nBytesSent;
}

//Carl 
int connectPCList(request *wp, int argc, char **argv)
{
	int nBytesSent=0, i, j, found=0, interface;
	WLAN_STA_INFO_Tp pInfo;
	char *buff;
	int mode, mode2;
	char mac[24];
	unsigned char wlan_iface_name[16];

	buff = calloc(1, sizeof(WLAN_STA_INFO_T) * (MAX_STA_NUM+1));
	if ( buff == 0 ) {
		printf("Allocate buffer failed!\n");
		return 0;
	}
	nBytesSent += req_format_write(wp, "<option>%s</option >", retLS("00lan031"));
	
	for(interface=0;interface<2;interface++){
		
		sprintf(WLAN_IF, "wlan%d", interface);
		if ( getWlStaInfo(WLAN_IF,  (WLAN_STA_INFO_Tp)buff ) < 0 ) {
			free(buff);
			printf("Read wlan sta info failed!\n");
			return 0;
		}

		for (i=1; i<=MAX_STA_NUM; i++) {
			pInfo = (WLAN_STA_INFO_Tp)&buff[i*sizeof(WLAN_STA_INFO_T)];
			if (pInfo->aid && (pInfo->flag & STA_INFO_FLAG_ASOC)) {
				sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", pInfo->addr[0],pInfo->addr[1],pInfo->addr[2],pInfo->addr[3],pInfo->addr[4],pInfo->addr[5]);
				//upper_case(mac);
				for(j=0; j<strlen(mac); j++){
					mac[j]=toupper(mac[j]);
				}
				nBytesSent += req_format_write(wp, "<option >%s</option >", mac);
				found++;
			}
		}
	}

//	apmib_get( MIB_WLAN_MODE, (void *)&mode);
//	if(mode==4)

	apmib_get( MIB_REPEATER_ENABLED1, (void *)&mode);
	apmib_get( MIB_REPEATER_ENABLED2, (void *)&mode2);
	if(mode == 1)
		sprintf(wlan_iface_name, "wlan0-vxd");
	else if(mode2 == 1)
		sprintf(wlan_iface_name, "wlan1-vxd");
	if( mode == 1 || mode2 ==1)
	{
  		if ( getWlStaInfo(wlan_iface_name,  (WLAN_STA_INFO_Tp)buff ) < 0 ) {
    		free(buff);
      		printf("Read wlan sta info failed!\n");
     		return 0;
   	 	}

   		for (i=1; i<=MAX_STA_NUM; i++) {
			pInfo = (WLAN_STA_INFO_Tp)&buff[i*sizeof(WLAN_STA_INFO_T)];
			if (pInfo->aid && (pInfo->flag & STA_INFO_FLAG_ASOC)) {
			sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", pInfo->addr[0],pInfo->addr[1],pInfo->addr[2],pInfo->addr[3],pInfo->addr[4],pInfo->addr[5]);
			//upper_case(mac);
			for(j=0; j<strlen(mac); j++){
				mac[j]=toupper(mac[j]);
			}
			nBytesSent += req_format_write(wp, "<option >%s</option >", mac);
			found++;
			}
		}
	}
	free(buff);

	return nBytesSent;
}//Carl end
/////////////////////////////////////////////////////////////////////////////
void formReflashClientTbl(request *wp, char *path, char *query)
{
	char *submitUrl;

	submitUrl = req_get_cstream_var(wp, "submit-url", "");
	if (submitUrl[0])
		send_redirect_perm(wp, submitUrl);
}


//////////////////////////////////////////////////////////////////////////////
int isDhcpClientExist(char *name)
{
	char tmpBuf[100];
	struct in_addr intaddr;

	if ( getInAddr(name, IP_ADDR, (void *)&intaddr ) ) {
		snprintf(tmpBuf, 100, "%s/%s-%s.pid", _DHCPC_PID_PATH, _DHCPC_PROG_NAME, name);
		if ( getPid(tmpBuf) > 0)
			return 1;
	}
	return 0;
}



