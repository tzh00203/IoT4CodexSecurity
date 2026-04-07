#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "channel_list.h"
#include <fcntl.h>
#include <sys/stat.h>

char get_RegDomain_Area(char *cmo_n , char *reg_domain_5g_buffer , char *area_buffer , char *reg_domain_2g_buffer){
    int i ;
    for(i=0 ; i < sizeof(Bandtable)/sizeof(*Bandtable);i++){
        if( !strcmp(cmo_n,Bandtable[i].CMO_Num) ){
            //printf("Bandtable[i].area = %s\n",Bandtable[i].area);                    //test output
          //  printf("*****%s:%d Bandtable[i].reg_domain_5g = %s\n\n",__FILE__,__LINE__,Bandtable[i].reg_domain_5g);      //test output
          //  printf("*****%s:%d Bandtable[i].reg_domain_2g = %s\n\n",__FILE__,__LINE__,Bandtable[i].reg_domain_2g);      //test output
            sprintf(area_buffer,"%s" , Bandtable[i].area);
            sprintf(reg_domain_5g_buffer,"%s" , Bandtable[i].reg_domain_5g);
            sprintf(reg_domain_2g_buffer,"%s" , Bandtable[i].reg_domain_2g);
            
           

        }
    }
}


char get_cmo_number( char *area, char *reg_domain , char *cmo_n)
{
    int i=0 ;
    for(i=0 ; i < sizeof(Bandtable)/sizeof(*Bandtable);i++){
        if( (!strcmp(area,Bandtable[i].area)) && (!strcmp(reg_domain,Bandtable[i].reg_domain_5g)) ){
             sprintf(cmo_n,"%s" , Bandtable[i].CMO_Num);
        }
    }
}

char channel_DFS_channel( char *country ,char *channel_tmp)
{
  int i=0 ;
  for(i=0 ; i < sizeof(Channel_DFS_ONLY)/sizeof(*Channel_DFS_ONLY) ; i++)
  {
    if( !strcmp(country,Channel_DFS_ONLY[i].country))
    {
      sprintf(channel_tmp,"%s" , Channel_DFS_ONLY[i].channel_DFS_only);
      break;
    } 
  }
}

char channel_list( char *reg_domain_5g ,char *reg_domain_2g ,char *channel_tmp ,char *mode)
{
   int i=0 ,break_flag=0;
   int tbl_size = 0;

   if(!strncmp(mode, "2.4G", 4))
   {
	tbl_size = sizeof(Channel_2g)/sizeof(*Channel_2g);
   }
   else if(!strncmp(mode, "5G", 2))
   {
	tbl_size = sizeof(Channel_5g)/sizeof(*Channel_5g);
   }

   for(i=0 ; i < tbl_size ; i++)
   {
       //select return whitch channel 
           //1.return Channel_2g[i].channel
           //2.return Channel_5g[i].channel_DFS_enable
           //3.return channel_5g[i].channel_DFS_disable
           //4.return channel_5g[i].wds_channel 
      //printf("%s:%d channel list in reg_domain_5g=%s Channel_5g[i].reg_domain_5g=%s\n\n",__FILE__,__LINE__,reg_domain_5g,Channel_5g[i].reg_domain_5g);//eded test printf
       //printf("%s:%d reg_domain_2g=%s Channel_2g[i].reg_domain_2g=%s break_flag=%d\n\n",__FILE__,__LINE__,reg_domain_2g,Channel_2g[i].reg_domain_2g,break_flag);//eded test printf
       if( !strcmp(reg_domain_2g,Channel_2g[i].reg_domain_2g) && !strcmp(mode,"2.4G") ) {//2.4g channel
           //printf("%s:%d channel list in\n\n",__FILE__,__LINE__);//eded test printf
           
           sprintf(channel_tmp,"%s" , Channel_2g[i].channel);
           break_flag=1;
       }
       else if( !strcmp(reg_domain_5g,Channel_5g[i].reg_domain_5g) && !strcmp(mode,"5G_enable") ){ //5g DFS enable channel       
          // printf("%s:%d reg_domain_5g=%s  Channel_5g[i].reg_domain_5g=%s\n\n",__FILE__,__LINE__,reg_domain_5g,Channel_5g[i].reg_domain_5g);//eded test printf
           //printf("%s:%d channel list in\n\n",__FILE__,__LINE__);//eded test printf
           sprintf(channel_tmp,"%s" , Channel_5g[i].channel_DFS_enable);
            //printf("%s:%d i=%d channel_tmp=%s\n\n",__FILE__,__LINE__,i,Channel_5g[i].channel_DFS_enable);//eded test printf
            break_flag=1;
       }
       else if( !strcmp(reg_domain_5g,Channel_5g[i].reg_domain_5g) && !strcmp(mode,"5G_disable") ){ //5g DFS disable channel       
           //printf("%s:%d channel list in\n\n",__FILE__,__LINE__);//eded test printf
           sprintf(channel_tmp,"%s" , Channel_5g[i].channel_DFS_disable);
           break_flag=1;
       }
       else if( !strcmp(reg_domain_5g,Channel_5g[i].reg_domain_5g) && !strcmp(mode,"5G_wds") ){ //5g wds channel  
           //printf("%s:%d channel list in\n\n",__FILE__,__LINE__);//eded test printf
           sprintf(channel_tmp,"%s" , Channel_5g[i].wds_channel);
           break_flag=1;
       }
       if(break_flag==1)
	        break;
   }
}
 /*eded test
 
    function get_RegDomain_Area() :
      get cmo number -> get reg_domain and country
    
    function get_cmo_number() : 
      get reg domain and country -> get cmo number
      
    finction channel_list() :
      get reg_domain -> get channel_5G
                   or
      get area -> get channel_2G  // eded test are not ready
      
    fmgat.c function getInfo() added 3 new option :    
      1.get cmo number -> get reg_domain and country
      2.get reg domain and country -> get cmo number
      3.get reg_domain mib and channel type to slerct channel -> get channel
      
    web page ( whilless.htm ):
      gat channel string ( function_getInfo ) -> string handling -> set channel option  on web page   
 */
    
