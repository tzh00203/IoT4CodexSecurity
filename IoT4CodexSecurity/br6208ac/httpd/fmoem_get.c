#include "__codeql_compat.h"
/*-- System inlcude files --*/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <dirent.h>
#include <fcntl.h>

/*-- Local inlcude files --*/
#include "../webs.h"
#include "apmib.h"
#include "utility.h"
#include "apform.h"
#include "oemform.h"


int oem_getInfo(int eid, webs_t wp, char *name)
{
	unsigned char buffer[100];
	int val;
	
	memset (buffer, 0x00, sizeof(buffer));
	if ( !strcmp (name, T("oem_getInfo")) ){
		if ( !apmib_get( MIB_PS_SCH_ENABLED, (void *)&buffer) )
			return -1;
		return websWrite (wp, buffer);
	}

	else if ( !strcmp(name, T("PSSCH0"))) {
		if ( !apmib_get(MIB_PS_SCH_Day0,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH1"))) {
		if ( !apmib_get(MIB_PS_SCH_Day1,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH2"))) {
		if ( !apmib_get(MIB_PS_SCH_Day2,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH3"))) {
		if ( !apmib_get(MIB_PS_SCH_Day3,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH4"))) {
		if ( !apmib_get(MIB_PS_SCH_Day4,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH5"))) {
		if ( !apmib_get(MIB_PS_SCH_Day5,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);

	}else if ( !strcmp(name, T("PSSCH6"))) {
		if ( !apmib_get(MIB_PS_SCH_Day6,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);
	}

	else if ( !strcmp(name, T("NAS_PS_Timer"))) {
		if ( !apmib_get(MIB_NAS_PS_TIMER,  (void *)&val))
			return -1;
		return websWrite(wp, T("%d"), val);
	}
	else if ( !strcmp(name, T("NAS_Folder_Name"))) {
		if ( !apmib_get(MIB_NAS_FOLDER_NAME,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);
	}
	else if ( !strcmp(name, T("NAS_Folder_Memo"))) {
		if ( !apmib_get(MIB_NAS_FOLDER_MEMO,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);
	}
	else if ( !strcmp(name, T("NAS_Folder_Group_Name"))) {
		if ( !apmib_get(MIB_NAS_FOLDER_GROUP_NAME,  (void *)buffer))
			return -1;
		return websWrite(wp, buffer);
	}


	return -1;
}

int oem_getIndex(int eid, webs_t wp, char *name)
{
	unsigned char buffer[100];
	int val;

	if ( !strcmp (name, T("oem_getIndex")) ){
		if ( !apmib_get( MIB_PS_SCH_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}
	else if ( !strcmp(name, T("powersavingEnable")) ) {
		if ( !apmib_get( MIB_PS_SCH_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	
	}else if ( !strcmp(name, T("wiredLans")) ) {
		if ( !apmib_get( MIB_PS_SCH_WIREDLAN, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;

	}else if ( !strcmp(name, T("wirelessLans")) ) {
		if ( !apmib_get( MIB_PS_SCH_WIRELESSLAN, (void *)&val))
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;

	}else if ( !strcmp(name, T("ledswitch")) ) {
		if ( !apmib_get( MIB_PS_SCH_LEDSWITCH, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
        }


	else if ( !strcmp (name, T("NAS_PS_Enabled"))){
		if ( !apmib_get( MIB_NAS_PS_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}
	else if ( !strcmp (name, T("NAS_Folder_Enabled"))){
		if ( !apmib_get (MIB_NAS_FOLDER_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}
	else if ( !strcmp (name, T("NAS_Access_Enabled"))){
		if ( !apmib_get (MIB_NAS_ACCESS_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}
	else if ( !strcmp (name, T("NAS_WAN_Enabled"))){
		if ( !apmib_get (MIB_NAS_WAN_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}
	else if ( !strcmp (name, T("NAS_DLNA_Enabled"))){
		if ( !apmib_get (MIB_NAS_DLNA_ENABLED, (void *)&val) )
			return -1;

		sprintf(buffer, "%d", val);
		ejSetResult(eid, buffer);
		return 0;
	}

	

	return -1;
}

#define NAS_DEVICE_INFO_FILE "/tmp/sxnasplus/sxnas.dir.conf"
#define DF_SYSTEM_CMD "df -h"
#define NAS_DEVICE_SIZE_FILE "/tmp/disk_size"
#define NAS_DEVICE_MOUNT_PATH "/tmp/dev"
#define NAS_DEVICE_PATH "Path"
#define NAS_DEVICE_FILETYPE "File System"

int NASDeviceList(int eid, webs_t wp, int argc, char_t **argv)
{
	char buffer[1024], sys_cmd_buffer[256], *ptr, *ptr1;
	char File_Type[8];
	char Path[64], Used_Size[12], Max_Size[12], Free_Size[12], Percentage[12],  Mount_Path[64];
	int retInt = 0;

	memset (buffer, 0x00, sizeof(buffer));
	if (get_file (NAS_DEVICE_INFO_FILE, buffer, sizeof(buffer)) != 0){

		sprintf (sys_cmd_buffer, "%s > %s", DF_SYSTEM_CMD, NAS_DEVICE_SIZE_FILE);
		system (sys_cmd_buffer);
		ptr = strstr(buffer, NAS_DEVICE_PATH) + sizeof(NAS_DEVICE_PATH);

		if (ptr){
			memset (path, 0x00, sizeof(path));
			ptr1 = strchr (ptr, '\n');
			memcpy (path, ptr, (ptr1 - ptr) >= sizeof(path) ? sizeof(path) - 1 : (ptr1 - ptr));
		}
	
		ptr = strstr(buffer, NAS_DEVICE_FILETYPE) + sizeof(NAS_DEVICE_FILETYPE);
		if (ptr){
			memset (File_Type, 0x00, sizeof(File_Type));
			ptr1 = strchr (ptr, '\n');
			memcpy (File_Type, ptr, (ptr1 - ptr) >= sizeof(File_Type) ? sizeof(File_Type) - 1 : (ptr1 - ptr));
		}
//		printf ("file_type=%s\n", File_Type);

		memset (buffer, 0x00, sizeof(buffer));
		if (get_file (NAS_DEVICE_SIZE_FILE, buffer, sizeof(buffer)) != 0){
	
			ptr = strstr(buffer, NAS_DEVICE_MOUNT_PATH);

//			printf ("ptr = %s\n", ptr);
			if (ptr){
				sscanf (ptr, "%20s %9s %9s %9s %3s%% %s\n", &Path, &Max_Size, &Used_Size, &Free_Size, &Percentage, &Mount_Path);
//				printf ("%s %s %s %s %s %s\n", Path, Max_Size, Used_Size, Free_Size, Percentage, Mount_Path);
				sprintf (buffer, "Logitec USB-001,disk-x_pt-x,%s,%s/%s,1", File_Type, Used_Size, Max_Size);
			}else{
				sprintf (buffer, "Logitec USB-001,disk-x_pt-x,Unknow,Unknow/Unknow,0");
			}
		}

	}else{
		websWrite (wp, "<nasdevice></nasdevice>");
	}
	retInt = websWrite (wp, buffer);

	return retInt;
}

int NASUserList (int eid, webs_t wp, int argc, char_t **argv)
{
	char tmpBuf[1024];
	int entryNum, retInt = 0, num;
	NAS_USER_T nas_user_entry;

	if ( !apmib_get(MIB_NAS_USER_NUM, (void *)&entryNum)){
		return -1;
	}

	if (entryNum == 0)
		return retInt;

	for (num = 1; num <= entryNum; num++){
		*((char *)&nas_user_entry) = (char)num;

		if ( !apmib_get(MIB_NAS_USER, (void *)&nas_user_entry))
			return -1;
		sprintf (tmpBuf, "<tr><td class='table1'><center>%d</center></td>	\
					<td class='table1'><center>%s</center></td>	\
					<td class='table1'><center>%s</center></td>	\
					<td class='table1'><center><input type=submit name='Nas_User_Del' id='Nas_User_Del' value='' onclick='return true'></center></td></tr>",  \
					num, nas_user_entry.name, nas_user_entry.memo, num);
		retInt += websWrite (wp, tmpBuf);
	}
	return retInt;
}



void oem_get_define_form ()
{
	websAspDefine(T("NASDeviceList"), NASDeviceList);
	websAspDefine(T("NASUserList"), NASUserList);
}
