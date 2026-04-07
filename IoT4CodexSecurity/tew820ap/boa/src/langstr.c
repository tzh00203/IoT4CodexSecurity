//Brent Create for multiple lang 
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
//#include "httpd.h"
#include "boa.h"
#include "apform.h"

extern char *modelName;	// defined in version.c
extern char *hwVersion; //version.c

char	buffer[1024];
char	prefn[6] = {0};
char	fullVersion[20]={0};
char  	langCategory[24]={0};
char  	modelName_hwVersion[16]={0}; //ed_hsiao add it
FILE	*langfp = NULL;
unsigned int	lang_path_sel = 0; //0:langpack 1:default lang
unsigned int	have_langpack = 0;
char			*LangPath;
unsigned char 	langVersion[16];
unsigned char 	inlangVersion[16];
unsigned char 	langDate[11];
unsigned char	langTag[4];


int CheckLangId(char *Lpath)
{
	FILE	*fp = NULL;
	char	langid[30] = {0};
	char	fpath[32] = {0};
	
	sprintf(fpath, "%s%s", Lpath, "idtag");
	
	if((fp = fopen(fpath, "r")) != NULL){
		fseek ( fp , 0 , SEEK_SET );
		fgets(langid, 20, fp);
		printf("lang id %s\n", langid);
		//ed_hsiao change to match DAP1360-C1 => match modelName_hwVersion   2012/10/23
		sprintf( modelName_hwVersion , "%s-%s" , modelName , hwVersion ); //ed_hsiao add it
		if(strncmp(langid,modelName_hwVersion, 11)){ 
			fclose(fp);
			return 0;
		}
		else{
			fgets(langid, 20, fp);			
			fgets(langid, 30, fp);//catagory	
			memset(langid, '\0', 20);
			if ((fgets(langid, 20, fp)==NULL)){//langVersion
				fclose(fp);
				return 1;
			}
			
			memset(langVersion, '\0', sizeof(langVersion));
			memcpy(langVersion, langid, sizeof(langVersion)-1);			
			memset(langid, '\0', 20);
			fgets(langid, 20, fp);//inlangVersion
			memset(inlangVersion, '\0', sizeof(inlangVersion));
			memcpy(inlangVersion, langid, sizeof(inlangVersion)-1);		
			memset(langid, '\0', 20);
			fgets(langid, 20, fp);//langDate
			memset(langDate, '\0', sizeof(langDate));
			memcpy(langDate, langid, sizeof(langDate)-1);				
			fclose(fp);
			return 1;
		}
	}
	return 0;
}

void CheckLangPack(void)
{
	have_langpack = 0;//Reset it
	
	if(CheckLangId(LANG_PATH_TW))
		have_langpack |= LANG_SEL_TW;
	if(CheckLangId(LANG_PATH_CN))
		have_langpack |= LANG_SEL_CN;
	if(CheckLangId(LANG_PATH_EN))
		have_langpack |= LANG_SEL_EN;
	if(CheckLangId(LANG_PATH_FR))
		have_langpack |= LANG_SEL_FR;
	if(CheckLangId(LANG_PATH_GE))
		have_langpack |= LANG_SEL_GE;
	if(CheckLangId(LANG_PATH_IT))
		have_langpack |= LANG_SEL_IT;
	if(CheckLangId(LANG_PATH_JP))
		have_langpack |= LANG_SEL_JP;
	if(CheckLangId(LANG_PATH_SP))
		have_langpack |= LANG_SEL_SP;
	if(CheckLangId(LANG_PATH_KR))
		have_langpack |= LANG_SEL_KR;

	printf("Lang support 0x%x, select %x\n", have_langpack, lang_path_sel);
	
	//sprintf(fullVersion,"%s", fwVersion);
	if(lang_path_sel == 0)
	{	
		lang_path_sel = 0;
		LangPath = LANG_PATH_DEF;
		//sprintf(fullVersion,"%sEN", fwVersion);
		strcpy(langCategory, "DEFAULT");
	} 
	else 
	{
		switch(have_langpack)
		{
			case LANG_SEL_TW:
				LangPath = LANG_PATH_TW;
				//sprintf(fullVersion,"%sTW", fwVersion);
				strcpy(langTag,"TW");
				strcpy(langCategory,"Traditional Chinese");
				break;
			case LANG_SEL_CN:
				LangPath = LANG_PATH_CN;
				//sprintf(fullVersion,"%sCN", fwVersion);
				strcpy(langTag,"CN");
				strcpy(langCategory, "Simplified Chinese");
				break;
			case LANG_SEL_EN:
				LangPath = LANG_PATH_EN;
				//sprintf(fullVersion,"%sEN", fwVersion);
				strcpy(langTag,"EN");
				strcpy(langCategory,"English");
				break;
			case LANG_SEL_FR:
				LangPath = LANG_PATH_FR;
				//sprintf(fullVersion,"%sFR", fwVersion);
				strcpy(langTag,"FR");
				strcpy(langCategory, "French");
				break;
			case LANG_SEL_GE:
				LangPath = LANG_PATH_GE;
				//sprintf(fullVersion,"%sGE", fwVersion);
				strcpy(langTag,"GE");
				strcpy(langCategory, "German");
				break;
			case LANG_SEL_IT:
				LangPath = LANG_PATH_IT;
				//sprintf(fullVersion,"%sIT", fwVersion);
				strcpy(langTag,"IT");
				strcpy(langCategory, "Italian");
				break;
			case LANG_SEL_JP:
				LangPath = LANG_PATH_JP;
				//sprintf(fullVersion,"%sJP", fwVersion);
				strcpy(langTag,"JP");
				strcpy(langCategory, "Japanese");
				break;
			case LANG_SEL_SP:
				LangPath = LANG_PATH_SP;
				//sprintf(fullVersion,"%sSP", fwVersion);
				strcpy(langTag,"SP");
				strcpy(langCategory, "Spanish");
				break;
			case LANG_SEL_KR:
				LangPath = LANG_PATH_KR;
				//sprintf(fullVersion,"%sKR", fwVersion);
				strcpy(langTag,"KR");
				strcpy(langCategory, "Korean");
				break;
			default:
				lang_path_sel = 0;
				LangPath = LANG_PATH_DEF;
				//sprintf(fullVersion,"%sEN", fwVersion);
				strcpy(langCategory, "DEFAULT");
				break;
		}
	}
			
}

int findstr(char *path, char *fn, char *tag, char *str)
{
	char	*pbuf;
	int	len;
	char	key[]=";";
	char	fpath[32] = {0};
	int	line = 0;
	int	idx = 0;
	
	if(strncmp(fn, prefn, 5))
	{
		if(langfp != NULL)
		{
			//printf("fclose %s\n",prefn);
			fclose(langfp);
		}
		strcpy(prefn, fn);
		
		sprintf(fpath, "%s%s", path, fn);
		//printf("fopen %s\n", fpath);
		langfp = fopen(fpath,"r");

	}

	line = atoi(tag);
	//printf("Get Line %d string ", line);
	
	if(langfp == NULL){
		printf("open file failed\n");
		return 0;
	}
	
	fseek ( langfp , 0 , SEEK_SET );
	while(idx++ < line)
	{
		fgets(str, 1024, langfp);
		
		if(feof(langfp))
			return 0;
	}
	
	len = strlen(str);
	str[len-2] = '\0';

	pbuf = strpbrk(str, key);
	//strcpy(str, pbuf+1);
	
	return 1;
}

//Carl: Modified for langstr.c 20130411
int getLS(request *wp, int argc, char **argv)
{
	//char	buffer[512] = {0};
	char	fn[6] = {0};
	char	tag[4] = {0};
	char	*name;
   /*   
	if (ejArgs(argc, argv, T("%s"), &name) < 1) {
   		websError(wp, 400, T("Insufficient args\n"));
   		return -1;
   	}
	*/
	name = argv[0];
	//LangPath = LANG_PATH_DEF; //temp
	strncpy(fn, name, 5);
	strncpy(tag, name + 5, 3);
	//printf("====LangPath = %s, fn = %s, tag = %s\n", LangPath, fn, tag);
	memset(buffer, 0 , 1024);
	if(!findstr( LangPath, fn, tag, buffer))
		strcpy(buffer, "    Empty String");
	return req_format_write(wp, "%s", (char *)(buffer+4)); 
}

char *retLS(char *fntag){
	char fn[6] = {0};
	char tag[4] = {0};
	
	strncpy(fn, fntag, 5);
	strncpy(tag, fntag+5 , 3);

	//printf("http call fn %s tag %s\n", fn, tag);
	//LangPath = LANG_PATH_DEF; //temp
	memset(buffer, 0, 1024);
	if(!findstr( LangPath, fn, tag, buffer))
		strcpy(buffer, "    Empty String");

	return (char *)(buffer+4);
}

char MulitLan[25][8]= {"mlten",
									"mlten","mltes","mltde","mltfr","mltit","mltru","mltpt","mltja",
									"mlttw","mltcn","mltko","mltcs","mltda","mltel","mltfi","mlthr",
									"mlthu","mltnl","mltno","mltpl","mlten","mltro","mltsl","mltsv"};
						
#ifdef MYDLINK_ENABLE
//modify for multi-language
int getMLS(request *wp, int argc, char **argv) 
{
	//char	buffer[512] = {0};
	char	fn[6] = {0};
	char	tag[4] = {0};
	//char_t	*name;
	char *name;
	
	     /*
	if (ejArgs(argc, argv, T("%s"), &name) < 1) {
   		websError(wp, 400, T("Insufficient args\n"));
   		return -1;
   	}
	*/
	name = argv[0];//add by Ed
	strncpy(fn, name, 5);
	
	strncpy(fn, name, 5);
	if( strcmp( fn, "multi" ) == 0 )
	{	
		//printf("MulitLan=%s\n",MulitLan[MultiLangType]);
		sprintf(fn, "%s", MulitLan[MultiLangType]);
	}
	strncpy(tag, name + 5, 3);
	//printf("fn = %s tag = %s",fn, tag);
	memset(buffer, 0 , 1024);
	if(!findstr( LANG_MULTI_LANG_PATH_DEF, fn, tag, buffer))
		strcpy(buffer, "    Empty String");
	return req_format_write(wp, "%s", (char *)(buffer+4));
}

//modify for multi-language
char *retMLS(char *fntag){
	char fn[6] = {0};
	char tag[4] = {0};
	
	strncpy(fn, fntag, 5);
	if( strcmp( fn, "multi" ) == 0 )
	{	
		//printf("MulitLan=%s\n",MulitLan[MultiLangType]);
		sprintf(fn, "%s", MulitLan[MultiLangType]);
	}
	strncpy(fn, MulitLan[MultiLangType], 5);
	strncpy(tag, fntag+5 , 3);

	//printf("http call fn %s tag %s\n", fn, tag);
	
	memset(buffer, 0, 1024);
	if(!findstr( LANG_MULTI_LANG_PATH_DEF, fn, tag, buffer))
		strcpy(buffer, "    Empty String");

	return (char *)(buffer+4);
}
#endif

