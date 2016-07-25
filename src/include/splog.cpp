#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include "config.h"
#include "splog.h"
#include "const_c.h"

#include <sys/time.h>
#include <time.h>
#include <unistd.h>


pthread_mutex_t __pmt_init = PTHREAD_MUTEX_INITIALIZER;

LoggerClass::LoggerClass(){
	logfile = NULL;
	action_lock = __pmt_init;
}

LoggerClass::~LoggerClass(){
	fclose(logfile);	
}


//delete those logs that don't fit into a set of DEFAULT_LOGSNUM files
void DeleteOldLogs(){
	char * buf1,* c;
	FILE* f1;
	long logiter;

	logiter = 0;
	buf1 = (char*)malloc(MYBUFSZ);
	snprintf(buf1,MYBUFSZ,"ls -1Ar --color=none %s/log/*.log",ConfC("PATH_OWNROOT"));
	if(f1 = popen(buf1,"r")) {
		rep("LOGGER: Start deleting old logs");
		while(fgets(buf1,MYBUFSZ,f1))
		{
			if(++logiter <= ConfI("DEFAULT_LOGSNUM")) 
				continue;
			c = strstr(buf1,"\n");//delete tail
			if(c!=NULL) c[0]=0;
			rep(buf1);
			if(unlink(buf1)) rep("failed");
		}
		rep("LOGGER: Done deleting old logs");
		pclose(f1);
	}
	free(buf1);
}

void LoggerClass::Write(const char* st,unsigned long th_id) {
	tm tv;
	time_t tt;
	struct timeval tmv;
	static char dom = 0;
	char cdom, * fnameb;
	long millisec;
	bool delold = false;

	tt = time(NULL);
	localtime_r(&tt,&tv);
	cdom = tv.tm_mday;

	gettimeofday (&tmv, NULL);
	millisec = tmv.tv_usec / 1000;

	pthread_mutex_lock(&action_lock);

	if(cdom != dom && logfile != NULL) {	//if day has changed, close current log and start new one
		fprintf(logfile,"*END*\n");
		fflush(logfile);
		fclose(logfile);
		logfile=NULL;
		delold = true;
	}else
		delold = false;

	if(logfile==NULL) {
		fnameb = (char*)malloc(MYBUFSZ);
		snprintf(fnameb,300,"%s/log/%d-%.2d-%.2d.log",ConfC("PATH_OWNROOT"),1900+tv.tm_year,tv.tm_mon+1,tv.tm_mday);
		logfile = fopen(fnameb,"a");
		free(fnameb);
		if(logfile == NULL) {
			perror("spanel: opening log: ");
			pthread_mutex_unlock(&action_lock);
			return;
		}
		fprintf(logfile,"\n");
		dom = cdom;
	}
	if(th_id)
		fprintf(logfile,"%.2d:%.2d:%.2d.%.3d [ %.6d ] %s\n", 
			tv.tm_hour,tv.tm_min,tv.tm_sec,millisec, th_id, st);
	else
		fprintf(logfile,"%.2d:%.2d:%.2d.%.3d %s\n", 
			tv.tm_hour,tv.tm_min,tv.tm_sec,millisec, st);
	fflush(logfile);
	pthread_mutex_unlock(&action_lock);

	if(delold) 
		DeleteOldLogs();	
	//do this AFTER reopen. Otherwise action leads to eternal circuit
}

void LoggerClass::Write(const std::string& st,unsigned long th_id)
{	Write(st.c_str(),th_id); 	}

void LoggerClass::Write(const char* st)
{	Write(st,0);	}
void LoggerClass::Write(const std::string& st)
{	Write(st.c_str(),0); 	}

void LoggerClass::Reopen(){
	if(logfile != NULL){
		fclose(logfile);
		logfile = NULL;
	}
	Write("*** Log reopened!",0);
	DeleteOldLogs();
}

//	INSTANCE
static LoggerClass	Log;


void rep(const char* st,unsigned long th_id) 
{	Log.Write(st,th_id);	}

void rep(const char * st) 
{	rep(st, 0);	}

void rep(const std::string& st)
{	Log.Write(st.c_str(),0); 	}

void rep(const std::string& st,unsigned long th_id)
{	Log.Write(st.c_str(),th_id); 	}

void LogReopen()
{	Log.Reopen();	}

