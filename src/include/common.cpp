#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <string>
#include <vector>

#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <mysql.h>

#include "mydb.h"

#include "common.h"
#include "config.h"
#include "splog.h"

#include "const_c.h"


static pthread_mutex_t timerLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t timerListLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_attr_t	timerThreadAttr;
static pthread_t		timerThread;

static bool TimerActive = false;

static std::vector<TimerEntry> TimerTODO;

unsigned long uptimeGet();
void uptimeStart();


void updClientUsage_r(void* conn){
	//rep("updClientUsage_r ENTERED");
	MYSQL** arg = (MYSQL**)conn;
	updClientUsage(*arg,0);
	//rep("updClientUsage_r EXITED");
}

int updClientUsage(MYSQL* conn,unsigned long cl_id = 0){
	DBRES res1;
	MYSQL_ROW	row1, row2;
	FILE* qpf;
	unsigned long csz,dbsz,emlsz, mstorage,tot;
	long lim;
//	rep("updClientUsage");
	char* buf = (char*)malloc(MYBUFSZ);
	char* buf1 = (char*)malloc(MYBUFSZ);

	if(buf == NULL) return 0;
	if(buf1 == NULL) { free(buf); return 0; }

//	rep("buffers ok");
/*	using namespace Connector;
	CGate cgserv; */

//get client list
	res1 = db_query(conn,"SELECT c.cl_id, c.cl_sysname, c.cl_name, c.cl_sysid, c.domain, p.`max_storage` FROM sk_clients c "
						 " LEFT JOIN sk_packages p ON p.`pack_id`=c.`pack_id` WHERE c.`expires`>UNIX_TIMESTAMP()");
	
	if(mysql_num_rows(res1)){
	rep("foreach clients");
		while(row1 = mysql_fetch_row(res1)){
			mstorage = atol(row1[5]);
			csz = dbsz = emlsz = 0;
//		rep("scan home directory");
			snprintf(buf, MYBUFSZ, "%s/%s",ConfC("PATH_VHOST_FILES"),row1[1]);
			csz = (long)getDirSize(buf);
//		rep("scan dbases");
			snprintf(buf, MYBUFSZ, "%s/%s_*",ConfC("MYSQL_DATA"),row1[1]);
			dbsz = (long)getDirSize(buf);
//		rep("get mail accounts sizes");
			snprintf(buf, MYBUFSZ, "%s/%s/ -wholename '*%s/*'",ConfC("MAIL_DATA_PRE"),row1[4],ConfC("MAIL_DATA_SUF"));
			emlsz = (long)getDirSize(buf);
						/*if(cgserv.Connect()){if(cgserv.Auth()){
								snprintf(buf, MYBUFSZ, "ListAccounts \"%s\"\r\n",row1[4]);if(cgserv.WR(buf,buf1,MYBUFSZ)){}					
							}}else rep("FAIL: launch CGATE");*/
		//update db
			tot = emlsz+dbsz+csz;
			snprintf(buf, MYBUFSZ, "UPDATE sk_clients SET cur_dbsize='%d', `cur_storage`='%d' WHERE cl_id='%s' LIMIT 1",
					dbsz,tot, row1[0]);

			db_query(conn,buf);

		//update user quota
			lim = mstorage-tot+csz;	//current quota = available storage - mailboxes - databases
			if(lim<0) lim = 0;
			if(mstorage>0)
			{
				rep("updating quota");
				snprintf(buf,MYBUFSZ,"/usr/sbin/setquota -u %s %d %d 0 0 -a",
					row1[1], BLKINMEG*lim,BLKINMEG*lim);
				//rep(buf);
				qpf = popen(buf,"r");
				if(qpf!=NULL) {
					if(pclose(qpf)) rep("setquota FAIL");					
				}else
					rep("popen for setquota = NULL");
			}else rep("No disk quota in this package. Skipping setquota");
			
		}
		mysql_free_result(res1);
	}//else rep("No rows in result");
	
	free(buf);
	free(buf1);
	return 1;
}

int timerInit(bool notimer){
	if(!notimer){
		if(TimerActive) return 0;
		rep("Activating TIMER Thread");
		//create timer thread
		pthread_attr_init(&timerThreadAttr);
		pthread_attr_setdetachstate(&timerThreadAttr, PTHREAD_CREATE_DETACHED);
		//setup timer
		TimerActive = true;
		pthread_create(&timerThread,&timerThreadAttr,timerLoop,NULL);
		
		//signal(SIGALRM,timerEnvoke);
		//alarm(10);
		rep("TIMER activated");
	}else{
		rep("Stopping TIMER...");
		//signal(SIGALRM,SIG_DFL);
		//alarm(0);
		TimerActive = false;				//setting termination flag
		pthread_mutex_unlock(&timerLock);	// to allow thread to read flag
		rep("TIMER stopped");
	}
	return 1;
}
void timerEnvoke(int sec){
	if(!TimerActive) return;
	//pthread_mutex_unlock(&timerLock);
	//alarm(10);
}
void *timerLoop(void * a){
	while(1){		
		//pthread_mutex_lock(&timerLock);
		//rep("TIMER: entered iteration");
		if(!TimerActive) {
			rep("TIMER: Not active -> terminating");
			return NULL;
		}
		pthread_mutex_lock(&timerLock);
		//do our job...
		for(long i = TimerTODO.size()-1; i>=0; i--){
			if(TimerTODO[i].Moment <= time(NULL)){
				//exec function
				//rep("TIMER: exec function");
				TimerTODO[i].Act(TimerTODO[i].Params);
				//rep("TIMER: i'm alive!!");
				if(TimerTODO[i].Freq)
					TimerTODO[i].Moment = time(NULL)+TimerTODO[i].Freq;
				else{
					if(TimerTODO[i].Params != NULL)
						free(TimerTODO[i].Params);
					TimerTODO.erase(TimerTODO.begin( ) + i);
				}
				
			}//else rep("TIMER: this one not ready yet");
		}
		//rep("TIMER: exiting iteration");
		pthread_mutex_unlock(&timerLock);	
		sleep(10);
	}
}
void timerSetAction(unsigned long offset,timerAction act, void* params=NULL, unsigned long freq=0){
	time_t tt;
	tt = time(NULL);
	pthread_mutex_lock(&timerLock);
	//do our job...	
	TimerEntry te;
	te.Act = act;
	te.Freq = freq;
	te.Moment = tt+offset;
	te.Params = params;
	TimerTODO.push_back(te);
	pthread_mutex_unlock(&timerLock);
}


double getDirSize(char* path){ //returns size in megs
	FILE * pid;
	double total = 0.0f;
	unsigned long one = 0;
	char * buf;
	buf = (char*)malloc(MYBUFSZ);
	if(buf == NULL) {
		rep("getDirSize: NOMEM");
		return total;
	}
	snprintf(buf,MYBUFSZ,"/usr/bin/find %s -type f -print0 | xargs -0 --no-run-if-empty -n 1 stat --format=%%s ",path);
//	rep(buf);
	pid = popen(buf,"r");
	if(pid != NULL) {
		while(fgets(buf,MYBUFSZ,pid)){
			one = atol(buf);
			total += one;
		}
		pclose(pid);
		total /= 1024*1024;
				
	}
	sprintf(buf,"scanned: %s found: %f", path,total);
	rep(buf);	
	return total;
}


char* mallocncopy(const char*st)
{
	char * ret;
	ret = (char*)malloc(strlen(st)+1);
	if(ret ==NULL) return NULL;
	strcpy(ret,st);
	return ret;
}

Command* SplitCommand(char *str){	

	Command * cmdp = new Command;
	char ch, pc=0;
	std::string	buf ="";
	bool snew = true, inq = false, linq = false;
	int len = strlen(str);
	for(int i=0; i<len; i++){
		snew = true;
		ch=str[i];
		if(ch>32 || inq){
			if(ch=='"'){
				linq = true;
				if(inq){
					if(pc !='\\') 					
						inq = snew = false;						
				}else{
					snew = false;
					inq = true;
				}
			}else
				if(ch=='\\' && pc !='\\') snew = false; else pc = 0;
			if (snew) buf+=ch;
		}else if(buf.size()>0 || linq){
			cmdp->push_back(buf);
			buf = "";
			linq = false;
		}
		pc = ch;
	}
	if(buf.size()>0 || linq)
		cmdp->push_back(buf);
	return cmdp;
}


int make_server_socket(int portnum)
{
	return make_server_socket_q(portnum, BACKLOG);
}

int make_server_socket_q(int portnum, int backlog)
{
	struct sockaddr_in	saddr;
	struct hostent		*hp;
	char hostname[HOSTLEN];
	char * buf;
	int sock_id, bret=1,i=0;
	if((sock_id = socket(PF_INET, SOCK_STREAM,0))==-1) return -1;

	bzero((void*)&saddr,sizeof(saddr));
	gethostname(hostname,HOSTLEN);
	hp = gethostbyname(hostname);
	
	do{
		if(i) 
		{ 
			buf = (char*)malloc(SMALLBUF);
			snprintf(buf,SMALLBUF, "Retrying in %d seconds...", ConfI("SOCK_RETRY_INTERVAL"));
			rep(buf);	
			free(buf);
			sleep(ConfI("SOCK_RETRY_INTERVAL"));
		}
		bcopy((void*)hp->h_addr,(void*)&saddr.sin_addr,hp->h_length);
		saddr.sin_port = htons(portnum);
		saddr.sin_family	=	AF_INET;
		bret=bind(sock_id, (struct sockaddr *)&saddr,sizeof(saddr));
		i++;	
	}while (bret && i<ConfL("SOCK_RETRY"));
	if (bret)return -1;

	if(listen(sock_id,backlog)) return -1;
	return sock_id;
}

///////////////////////////////////////////////////////////////////////

char** exec_prepare(char* str)
{
	Command* cm;
	cm = SplitCommand(str);
	char ** args;
	args = (char**)malloc(sizeof(char*)*((cm->size()>0?cm->size()+2:1)));	
	if(cm->size())
	{
		args[cm->size()+1] = NULL;
		args[0] = mallocncopy((*cm)[0].c_str());
		args[1] = mallocncopy((*cm)[0].c_str());
	//	char* lo = strrchr(args[0],'/');
	//	args[1] = mallocncopy(lo!=NULL?lo+1:args[0]);
	
		for(int i = 1; i<cm->size(); i++)	
			args[i+1] = mallocncopy((*cm)[i].c_str());		
		
	}else
		args[0] = NULL;
	return args;
}


std::string * PrintDetailed(Command* pcmd){
	std::string * ret = new std::string;
	*ret = "";
	for(int i=0; i<pcmd->size(); i++)
	{
		(*ret) +="[";
		(*ret)+=i;
		(*ret)+="] => ";
		(*ret)+= (*pcmd)[i];
	}
	return ret;
}

std::string * PrintDetailed(char** pcmd){
	std::string * ret = new std::string;
	*ret = "";
	for(int i=0; pcmd[i]!=NULL; i++)
	{
		fprintf(stderr," [ %d ] => %s\r\n",i, pcmd[i]);
	}
	return ret;
}

void exec_args_clean(char** list)
{
	for(int i = 0; list[i]!=NULL; i++)
		free(list[i]);
	free(list);
}

int newprocess(char* path,char *arglist[])
{
	int pid, exitstatus;
	pid = fork();
	
	switch(pid)
	{
	case -1: return -1;
	case 0:
		execvp(arglist[0],
			 arglist);
		rep("exec failed");
		switch(errno )
		{
		case EPERM:rep("EPERM"); break;
		case EACCES:rep("EACCES"); break;

		case E2BIG:rep("E2BIG"); break;
		case ENOEXEC:rep("ENOEXEC"); break;
		case EFAULT:rep("EFAULT"); break;
		case ENAMETOOLONG:rep("ENAMETOOLONG"); break;

		case ENOENT:rep("ENOENT"); break;
		case ENOMEM:rep("ENOMEM"); break;
		case ENOTDIR:rep("ENOTDIR"); break;
		case ETXTBSY:rep("ETXTBSY"); break;
		case EIO:rep("EIO"); break;
		default: rep("MYSTERY!"); 
		}
		
		exit(-1);
	default:
		while(wait(&exitstatus)!=pid);
		fprintf(stderr,"myserv: child exited with %d r\n",exitstatus);
		return WEXITSTATUS(exitstatus);
	}
}

int execute(char* cmd)
{
	char** args;
	args = exec_prepare(cmd);
	PrintDetailed(args);
	int ret = newprocess(args[0], (char**) args[1]);
	exec_args_clean(args);
	return ret;
}


//end

