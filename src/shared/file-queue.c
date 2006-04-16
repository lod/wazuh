/*   $OSSEC, file-queue.c, v0.1, 2006/04/13, Daniel B. Cid$   */

/* Copyright (C) 2003-2006 Daniel B. Cid <dcid@ossec.net>
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */


/* File monitoring functions */

#include "shared.h"
#include "file-queue.h"

/* To translante between month (int) to month (char) */
char *(s_month[])={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug",
                "Sept","Oct","Nov","Dec"};


/** void file_sleep();
 * file_sleep
 */
void file_sleep()
{
    #ifndef WIN32
    struct timeval fp_timeout;
        
    fp_timeout.tv_sec = FQ_TIMEOUT;
    fp_timeout.tv_usec = 0;

    /* Waiting for the select timeout */
    select(0, NULL, NULL, NULL, &fp_timeout);
    
    #else
    /* Windows don't like select that way */
    Sleep((FQ_TIMEOUT + 2) * 1000);
    #endif

    return;
}


/** void GetFile_Queue(file_queue *fileq)
 * Get the file queue for that specific hour
 */
void GetFile_Queue(file_queue *fileq)
{
    /* Creating the logfile name */
    fileq->file_name[0] = '\0';
    fileq->file_name[MAX_FQUEUE] = '\0';

    snprintf(fileq->file_name, MAX_FQUEUE,
                               "%s/%d/%s/ossec-alerts-%02d.log",
                               ALERTS,
                               fileq->year,
                               fileq->mon,
                               fileq->day);  
}


/** int Handle_Queue(file_queue *fileq)
 * Re Handle the file queue.
 */
int Handle_Queue(file_queue *fileq, u_int8_t seek_end) 
{
    /* Closing if it is open */
    if(fileq->fp)
    {
        fclose(fileq->fp);
        fileq->fp = NULL;
    }
    
    /* We must be able to open the file, fseek and get the
     * time of change from it.
     */
    fileq->fp = fopen(fileq->file_name, "r");
    if(!fileq->fp)
    {
        /* Queue not available */
        return(0);
    }

    /* Seeking the end of file */
    if(seek_end)
    {
        if(fseek(fileq->fp, 0, SEEK_END) < 0)
        {
            merror(FSEEK_ERROR, ARGV0, fileq->file_name);
            fclose(fileq->fp);
            fileq->fp = NULL;
            return(-1);
        }
    }
   
    /* File change time */
    if(fstat(fileno(fileq->fp), &fileq->f_status) < 0)
    {
        merror(FILE_ERROR,ARGV0, fileq->file_name);
        fclose(fileq->fp);
        fileq->fp = NULL;
        return(-1);
    }
    
    fileq->last_change = fileq->f_status.st_mtime;
    
    return(1);
}


/** int Init_FileQueue(file_queue *fileq)
 * Initiates the file monitoring.
 */
int Init_FileQueue(file_queue *fileq, struct tm *p)
{
    fileq->fp = NULL;
    fileq->last_change = 0;
    memset(fileq->file_name, '\0',MAX_FQUEUE + 1);
    fileq->day = p->tm_mday;
    fileq->year = p->tm_year+1900;
    strncpy(fileq->mon, s_month[p->tm_mon], 4);

    /* Getting latest file */
    GetFile_Queue(fileq);
    
    /* Always seek end when starting the queue */
    if(Handle_Queue(fileq, 1) < 0)
        return(-1);

    return(0);    
}


/** int Read_FileMon(file_queue *file, char *buffer, int buffer_size)
 * Reads from the monitored file.
 */
alert_data *Read_FileMon(file_queue *fileq, struct tm *p, int timeout)
{
    int i = 0;
    alert_data *al_data;
    
    /* Getting currently file */
    if(p->tm_mday != fileq->day)
    {
        fileq->day = p->tm_mday;
        fileq->year = p->tm_year+1900;
        strncpy(fileq->mon, s_month[p->tm_mon], 4);

        /* Getting latest file */
        GetFile_Queue(fileq);

        if(Handle_Queue(fileq, 0) == -1)
        {
            file_sleep();
            return(NULL);
        }
    }

    
    /* If the file queue is not available, try to access it */
    if(!fileq->fp)
    {
        if(Handle_Queue(fileq, 0) != 1)
        {
            file_sleep();
            return(NULL);
        }
    }

    /* Try up to timeout times to get an event */
    while(i < timeout)
    {
        al_data = GetAlertData(CRALERT_MAIL_SET, fileq->fp);
        if(al_data)
            return(al_data);
            
        i++;    
        file_sleep();
    }

    return(NULL);
}


/* EOF */
