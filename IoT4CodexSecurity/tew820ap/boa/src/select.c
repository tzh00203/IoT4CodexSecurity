/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
 *  Copyright (C) 1996-1999 Larry Doolittle <ldoolitt@boa.org>
 *  Copyright (C) 1996-2003 Jon Nelson <jnelson@boa.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* $Id: select.c,v 1.1.2.17 2005/02/22 14:11:29 jnelson Exp $*/

/* algorithm:
 * handle any signals
 * if we still want to accept new connections, add the server to the
 * list.
 * if there are any blocked requests or the we are still accepting new
 * connections, determine appropriate timeout and select, then move
 * blocked requests back into the active list.
 * handle active connections
 * repeat
 */



#include "boa.h"

extern int firmware_len;
extern char *firmware_data;
static void fdset_update(void);
fd_set block_read_fdset;
fd_set block_write_fdset;
int max_fd = 0;
extern int isFWUPGRADE;
extern int isREBOOTASP;
extern int isCFGUPGRADE;
extern int isRestoreDefSw; // joe add for restore default-sw 
int req_after_upgrade=0;
int last_req_after_upgrade=0;
int confirm_last_req=0;
extern int reboot_time;

void loop(int server_s)
{
    FD_ZERO(BOA_READ);
    FD_ZERO(BOA_WRITE);

    max_fd = -1;
		
		AUTH_check_time();
		
    while (1) {
        /* handle signals here */
#ifdef CONFIG_APP_BULK_DHCP       
        if (bulk_upgrade_flag)
        {		 
        		bulk_FWUPGRADE(NULL,NULL,NULL);
        }	
#endif	
#ifdef HNAP_ENABLE 
		if(hnap_fw_download_flag)
		{
			FILE *fp;	
			char buf[256];
			char wget_image[256];
			char fwDownload_url[256];
			unsigned char *strtmp;
			
			if ((fp = fopen("/tmp/fw_check_result.txt", "r")) != NULL){  
				memset(buf, 0, sizeof(buf));
	   			fgets(buf, sizeof(buf), fp);    
	   		      	    		
				if ((strncmp(buf, "ERROR", 5))!=0 && (strncmp(buf, "LATEST", 6)!=0)){
					strtmp = gettoken(buf, 3, '+');         	   			
	   				strcpy(fwDownload_url, strtmp);
					sprintf(wget_image,"wget -O /tmp/download_fwImage -c %s >/tmp/download_percent 2>&1 &",fwDownload_url);
					system(wget_image);
					sleep(6);
				}
			}
			system("rm /tmp/fw_check_result.txt");
			hnap_fw_download_flag = 0;
		}
		if (hnap_upgrade_flag)
        {		
        	int fileLen;
        	char *fildData=NULL;
        	char buffer[200]; 
        	FILE *fp = fopen("/tmp/download_fwImage", "rb");

			fseek(fp, 0, SEEK_END); 
			fileLen = ftell(fp); 
			fseek(fp, 0, SEEK_SET);
			printf("fileLen=%d\n",fileLen);
			fildData = (char *)malloc(fileLen);
			fread(fildData,fileLen,1,fp);
			fclose(fp);
			
        	BULK_FirmwareUpgrade(fildData, fileLen, 0, buffer);
        }	
        if(hnap_sitesurvey_flag)
        {
        	hnap_sitesurvey();
        	hnap_sitesurvey_flag = 0;
        }
#endif
        if (sighup_flag)
            sighup_run();
        if (sigchld_flag)
            sigchld_run();
        if (sigalrm_flag)
            sigalrm_run();

        if (sigterm_flag) {
            /* sigterm_flag:
             * 1. caught, unprocessed.
             * 2. caught, stage 1 processed
             */
            if (sigterm_flag == 1) {
                sigterm_stage1_run();
                BOA_FD_CLR(req, server_s, BOA_READ);
                close(server_s);
                /* make sure the server isn't in the block list */
                server_s = -1;
            }
            if (sigterm_flag == 2 && !request_ready && !request_block) {
                sigterm_stage2_run(); /* terminal */
            }
        } else {
            if (total_connections > max_connections) {
                /* FIXME: for poll we don't subtract 20. why? */
                BOA_FD_CLR(req, server_s, BOA_READ);
            } else {
                BOA_FD_SET(req, server_s, BOA_READ); /* server always set */
            }
        }
        
      	if (isREBOOTASP == 1) {
      			if(last_req_after_upgrade != req_after_upgrade){
      					last_req_after_upgrade = req_after_upgrade;
      			}
      	}
	
        pending_requests = 0;
        /* max_fd is > 0 when something is blocked */
        if (max_fd) {
            struct timeval req_timeout; /* timeval for select */

            req_timeout.tv_sec = (request_ready ? 0 : default_timeout);
            req_timeout.tv_usec = 0l; /* reset timeout */

            if (select(max_fd + 1, BOA_READ,
                       BOA_WRITE, NULL,
                       (request_ready || request_block ?
                        &req_timeout : NULL)) == -1) {
                /* what is the appropriate thing to do here on EBADF */
                if (errno == EINTR) {
			//fprintf(stderr,"####%s:%d isFWUPGRADE=%d isREBOOTASP=%d###\n",  __FILE__, __LINE__ ,isFWUPGRADE , isREBOOTASP);
			//fprintf(stderr,"####%s:%d last_req_after_upgrade=%d req_after_upgrade=%d confirm_last_req=%d###\n",  __FILE__, __LINE__ ,last_req_after_upgrade , req_after_upgrade, confirm_last_req);
          //     printf("[%s-%u] isFWUPGRADE = %d, isREBOOTASP = %d isCFGUPGRADE = %d\n",__FILE__,__LINE__,isFWUPGRADE,isREBOOTASP,isCFGUPGRADE); //joe DEBUG
          //     printf("[%s-%u] , confirm_last_req = %d\n",__FILE__,__LINE__,confirm_last_req); //joe DEBUG
                	if (isFWUPGRADE !=0 && isREBOOTASP == 1 ) 
                	{	  
                	  
                		if (last_req_after_upgrade == req_after_upgrade)
              				confirm_last_req++;
                		if (confirm_last_req >1) // joe change to 1 from 3 to make firmware upgrade faster
                			goto ToUpgrade;
                	} else if(isCFGUPGRADE ==2  && isREBOOTASP == 1 ) {
                		goto ToReboot;
                	}
                	else if (isFWUPGRADE ==0 && isREBOOTASP == 1) {
                		if (last_req_after_upgrade == req_after_upgrade)
              				confirm_last_req++;
                		if (confirm_last_req >3) {
                			isFWUPGRADE = 0;
                			isREBOOTASP = 0;
                			//isFAKEREBOOT = 0;
                			confirm_last_req=0;
              			}
                	}
                	else if ( (isREBOOTASP == 2) || (isREBOOTASP == 3) )
                	{
                	  goto ToReInit;
                	} 
                    continue;       /* while(1) */                
                }
                else if (errno != EBADF) {
                    DIE("select");
                }
            }
            /* FIXME: optimize for when select returns 0 (timeout).
             * Thus avoiding many operations in fdset_update
             * and others.
             */
            if (!sigterm_flag && FD_ISSET(server_s, BOA_READ)) {
                pending_requests = 1;
            }
            time(&current_time); /* for "new" requests if we've been in
            * select too long */
            /* if we skip this section (for example, if max_fd == 0),
             * then we aren't listening anyway, so we can't accept
             * new conns.  Don't worry about it.
             */
        }

        /* reset max_fd */
        max_fd = -1;

        if (request_block) {
            /* move selected req's from request_block to request_ready */
            fdset_update();
        }

        /* any blocked req's move from request_ready to request_block */
        if (pending_requests || request_ready) {
        	if (isFWUPGRADE !=0 && isREBOOTASP == 1 ){
        		req_after_upgrade++;
        	}else if(isFWUPGRADE ==0 && isREBOOTASP == 1){
        		req_after_upgrade++;
        	}
            process_requests(server_s);
            continue;
        }

ToUpgrade:
	 if (isFWUPGRADE !=0 && isREBOOTASP == 1 ) {
		char buffer[200];
		//fprintf(stderr,"\r\n [%s-%u] FirmwareUpgrade start",__FILE__,__LINE__);
		FirmwareUpgrade(firmware_data, firmware_len, 0, buffer);
		//fprintf(stderr,"\r\n [%s-%u] FirmwareUpgrade end",__FILE__,__LINE__);
		//system("echo 7 > /proc/gpio"); // disable system LED
		isFWUPGRADE=0;
		isREBOOTASP=0;
		//reboot_time = 5;
     		break;
     	}

ToReboot:
       if( isRestoreDefSw ==1) {
       // printf("[%s-%u] To Restore default sw \n",__FILE__,__LINE__); //joe DEBUG
         isRestoreDefSw = 0;
         system("flash default-sw");
       }
	 if(isCFGUPGRADE == 2 && isREBOOTASP ==1) {
	 	isCFGUPGRADE=0;
		isREBOOTASP=0;
		system("reboot");
		for(;;);
	 }
	 
ToReInit:
	  //printf("\n\n[%s-%u]$$$ ToReInit $$$\n\n",__FILE__,__LINE__); //joe DEBUG 
	  if( isREBOOTASP == 2 )
	  {
	    isCFGUPGRADE=0;
    	isREBOOTASP=0;
    	req_login_p->auth_login = 0;
    	run_init_script("all");
	  }
    if( isREBOOTASP == 3 ) // for repeater reinit
	  {
	    printf("[%s-%d] Repeater reinit test!!\n\n",__FILE__,__LINE__);
	    isCFGUPGRADE=0;
	    isREBOOTASP=0;
	    system("sysconf init ap wlan_app");
	  }
  }
}

/*
 * Name: fdset_update
 *
 * Description: iterate through the blocked requests, checking whether
 * that file descriptor has been set by select.  Update the fd_set to
 * reflect current status.
 *
 * Here, we need to do some things:
 *  - keepalive timeouts simply close
 *    (this is special:: a keepalive timeout is a timeout where
       keepalive is active but nothing has been read yet)
 *  - regular timeouts close + error
 *  - stuff in buffer and fd ready?  write it out
 *  - fd ready for other actions?  do them
 */

static void fdset_update(void)
{
    request *current, *next;

    time(&current_time);
    for (current = request_block; current; current = next) {
        time_t time_since = current_time - current->time_last;
        next = current->next;

        /* hmm, what if we are in "the middle" of a request and not
         * just waiting for a new one... perhaps check to see if anything
         * has been read via header position, etc... */
        if (current->kacount < ka_max && /* we *are* in a keepalive */
            (time_since >= ka_timeout) && /* ka timeout */
            !current->logline) { /* haven't read anything yet */
            log_error_doc(current);
            fputs("connection timed out\n", stderr);
            current->status = TIMED_OUT; /* connection timed out */
        } else if (time_since > REQUEST_TIMEOUT) {
            log_error_doc(current);
            fputs("connection timed out\n", stderr);
            current->status = TIMED_OUT; /* connection timed out */
        }
        if (current->buffer_end && /* there is data to write */
            current->status < DONE) {
            if (FD_ISSET(current->fd, BOA_WRITE))
                ready_request(current);
            else {
                BOA_FD_SET(current, current->fd, BOA_WRITE);
            }
        } else {
            switch (current->status) {
            case IOSHUFFLE:
#ifndef HAVE_SENDFILE
                if (current->buffer_end - current->buffer_start == 0) {
                    if (FD_ISSET(current->data_fd, BOA_READ))
                        ready_request(current);
                    break;
                }
#endif
            case WRITE:
            case PIPE_WRITE:
                if (FD_ISSET(current->fd, BOA_WRITE))
                    ready_request(current);
                else {
                    BOA_FD_SET(current, current->fd, BOA_WRITE);
                }
                break;
            case BODY_WRITE:
// davidhsu ------------------------------
#ifndef NEW_POST
                if (FD_ISSET(current->post_data_fd, BOA_WRITE))
                    ready_request(current);
                else {
                    BOA_FD_SET(current, current->post_data_fd,
                               BOA_WRITE);
                }
#else
		ready_request(current);
#endif
//--------------------------------------				
                break;
            case PIPE_READ:
                if (FD_ISSET(current->data_fd, BOA_READ))
                    ready_request(current);
                else {
                    BOA_FD_SET(current, current->data_fd,
                               BOA_READ);
                }
                break;
            case DONE:
                if (FD_ISSET(current->fd, BOA_WRITE))
                    ready_request(current);
                else {
                    BOA_FD_SET(current, current->fd, BOA_WRITE);
                }
                break;
            case TIMED_OUT:
            case DEAD:
                ready_request(current);
                break;
            default:
                if (FD_ISSET(current->fd, BOA_READ))
                    ready_request(current);
                else {
                    BOA_FD_SET(current, current->fd, BOA_READ);
                }
                break;
            }
        }
        current = next;
    }
}
