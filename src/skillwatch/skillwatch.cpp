//#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <map>
#include <netdb.h>
#include <math.h>

#include <mysql.h>
#include <pthread.h>

#include "common.h"
#include "config.h"
#include "functions.h"
#include <signal.h>
#include "splog.h"

#include <fstream>
#include <iostream>
#include <regex.h>


using namespace	std;
typedef  map<string,int> reqmap;
typedef  map<string,reqmap> statmap;
typedef  vector<string> svec;
typedef  map<string, svec> connmap;

//class 
void sig_act(int);
void finalize(int);

MYSQL * SQLconn;
MYSQL ** mspt = NULL;


statmap zoneCstats;
statmap zoneCstatsold;

connmap conns;
reqmap ipstat;
svec localAddrs;

void* watch(void*);
void printStats();
void compareStats(statmap*,statmap*);
void init();
int SWexec(string);
int IPTexec(string);
int IPTblock(string);
string hex2ipv4(string);
svec explode(const char*, char delim = ' ');
svec getLocalAddr();
bool isLocal(string);

unsigned long genWatchers(string);
void setup(pthread_attr_t * );

static unsigned long 	logwatchcount = 0;
static bool			 	logwatchrun = true;
static pthread_mutex_t 	logwatchcountlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t 	mainlock		  = PTHREAD_MUTEX_INITIALIZER;

unsigned long maxhit;
unsigned long maxChit;
unsigned long freq;
unsigned long maxatonce;

char *repString = NULL;

int		pidf = 0;

string regexp (string str, char *patrn, int *begin = NULL, int *end = NULL) {     
        int i, w=0, len, b, e;
		string word;
        regex_t rgT;
        regmatch_t match;
        regcomp(&rgT, patrn, REG_EXTENDED);
        if ((regexec(&rgT, str.c_str(), 1, &match, 0)) == 0) {
                b = (int)match.rm_so;
                e = (int)match.rm_eo;
                len = e-b;
                for (i=b; i<e; i++) {
                        word += str[i];
                }
        }
		if(begin!=NULL) *begin = b;
		if(end!=NULL) *end = e;
        regfree(&rgT);
        return word;
}


int main(int argc, char* argv[])
{
	
	if(ConfLoad("/etc/skillwatch.conf")!=1)
	{
		perror("ERROR! Failed to open config file. Maybe I'm not root?\n");
		finalize(1);
	}
	if(argc>1){
		if(((string)argv[1]) == "stop"){
			SWexec(((string)"cat ") + ConfC("PATH_PIDFILE") + " |xargs kill");
			exit(0);
		}
	}
	
	//daemonize();
	
	char pidbuf[10];
	
	pidf = open(ConfC("PATH_PIDFILE"),O_CREAT|O_WRONLY,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if(flock(pidf,LOCK_EX|LOCK_NB)){
		rep("ERROR: Pidfile is busy");
		exit(1);
	}
	sprintf(pidbuf,"%d",getpid());
	if(-1==write(pidf,pidbuf,strlen(pidbuf))) perror("write (pidf)");
	fsync(pidf);
	
	init();	
	
	pthread_mutex_lock(&mainlock);
	
	genWatchers(ConfS("PATH_WATCHLOG"));
	
	pthread_mutex_lock(&mainlock);
	pthread_mutex_unlock(&mainlock);
	return 0;
}

unsigned long genWatchers(string wpattern){
	char * buf1,* c;
	FILE* f1;
	unsigned long logiter;
	pthread_t		worker;
	pthread_attr_t	attr;

	setup(&attr);
	logiter = 0;
	buf1 = (char*)malloc(MYBUFSZ);
	snprintf(buf1,MYBUFSZ,"ls -1Ar --color=none %s",wpattern.c_str());
	if(f1 = popen(buf1,"r")) {
		rep("Launching watchers");
		while(fgets(buf1,MYBUFSZ,f1))
		{
			++logiter;
			c = strstr(buf1,"\n");//delete tail
			if(c!=NULL) c[0]=0;
			rep(buf1);
			rep("Will malloc()");
			c = (char*)malloc(strlen(buf1));
			rep("Will strcpy()");
			strcpy(c, buf1);
			rep("Want to create thread");
			pthread_create(&worker, &attr, watch, (void*)c);
			rep("Want to create thread");
		}
		rep("All Watchers Launched");
		pclose(f1);
	}
	free(buf1);
	
	return logiter;
}

void setup(pthread_attr_t * attrp)
{
	pthread_attr_init(attrp);
	pthread_attr_setdetachstate(attrp, PTHREAD_CREATE_DETACHED);
}

void init()
{
	maxhit = 	ConfL("SW_MAXHIT");
	maxChit = 	ConfL("SW_MAXCHIT");
	freq = 		ConfL("SW_FREQ");
	maxatonce = ConfL("SW_MAXATONCE");
	
	repString = (char *) malloc (MYBUFSZ);
	
	signal(SIGINT, sig_act);
	signal(SIGHUP, sig_act);
	signal(SIGKILL, sig_act);
	signal(SIGTERM, sig_act);
	
	if( ConfS("IPT_CHAIN")!="" ){
		IPTexec(((string)"-N ") + ConfS("IPT_CHAIN"));
		//resetting chain redirection
		IPTexec(((string)"-D INPUT -j ") + ConfS("IPT_CHAIN"));
		IPTexec(((string)"-I INPUT -j ") + ConfS("IPT_CHAIN"));
	}
}

void* watch(void* fn){

	statmap stats;
	statmap statsold;
	unsigned long lastpos = 0;
	unsigned long lastread = 0;
	unsigned long iters = 0;
	unsigned long atonce;
	char charbuf[500];
	string i, ip, req;
	ifstream logfile;
	char* filename = (char*)fn;
	
	time_t tt, ttold;
	
	pthread_mutex_lock(&logwatchcountlock);
	logwatchcount++;
	pthread_mutex_unlock(&logwatchcountlock);
	
	tt = ttold = time(NULL);
	
	logfile.open(filename, ios::in);
	logfile.seekg(0,ios::end);
	lastpos = logfile.tellg();
	logfile.close();
	while(logwatchrun){
		//snprintf(repString, MYBUFSZ, "Lastpos = %d",  lastpos); rep(repString);
		logfile.open(filename, ios::in);
		if(!logfile.is_open()){
			rep("File vanished. Waiting 5 sec..."); sleep(5); lastpos = 0; continue;
		}
		if(lastread<1){
			logfile.seekg(0, ios::end);
			if(logfile.tellg() < lastpos){
				rep("Detected Log Rotation.");
				logfile.seekg(0, ios::beg);
				lastpos = 0;
			}
		}
		
		logfile.seekg(lastpos, ios::beg);
		
		atonce = 0;
		while(!logfile.eof() && !logfile.fail() && !logfile.bad()){
			logfile.getline(charbuf, 500); 
			charbuf[500-1] = 0; //just in case :) 
			lastread = logfile.gcount();
			if(lastread < 1) break;
			lastpos += lastread;
			i = charbuf;
			ip = regexp(i, "^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
			if(ip == "") continue;
			req = regexp(i, "(GET|POST|OPTIONS) .* HTTP\\/[0-9\\.]+");
			if(req == "") {  continue; } 
			//cout<<endl;
			if(stats[ip][req]) stats[ip][req]++;
			else stats[ip][req] = 1;
			
			//Class C address stats -- sometimes needed
			ip = regexp(ip, "^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
			if(zoneCstats[ip][req]) zoneCstats[ip][req]++;
			else zoneCstats[ip][req] = 1;
			
			if(++atonce >= maxatonce) break; //do not overload system
		}
		logfile.close();
		
		tt = time(NULL);
		if(tt > ttold+freq){
			compareStats(&stats, &statsold); //TODO: compare statmaps
			ttold = tt; //reset interval
			statsold = stats; //save new checkpoint
			zoneCstatsold = zoneCstats; //save new checkpoint for zone C
		}
		sleep(2); 
	}
	//free(charbuf);
	logfile.close();
	free(filename);
	
	pthread_mutex_lock(&logwatchcountlock);
	logwatchcount--;
	pthread_mutex_unlock(&logwatchcountlock);
	return NULL;
}

void compareStats(statmap* stats, statmap* statsold){
	statmap::iterator it;
	reqmap::iterator it1;
	int oldv, diff;
	char* repString = (char*)malloc(MYBUFSZ);
	
	for ( it=(*stats).begin() ; it != (*stats).end(); it++ ){
		for ( it1 = (*it).second.begin() ; it1 != (*it).second.end(); it1++ ){
			oldv = (*statsold)[(*it).first][(*it1).first];
			diff = (*stats)[(*it).first][(*it1).first] - oldv;
			if(diff > maxhit){
				if((*it).second.size() < ConfL("SW_MINOTHERREQS")){
					if(ConfI("IPT_REALBLOCK")){
						snprintf(repString, MYBUFSZ, "Blocked %s for %d requests of '%s' for %d s.", 
									(*it).first.c_str(), diff, (*it1).first.c_str(), freq);
						rep(repString);
						IPTblock((*it).first);
					}else{
						snprintf(repString, MYBUFSZ, "ALERT!! Should block %s for %d requests of '%s' for %d s.", 
									(*it).first.c_str(), diff, (*it1).first.c_str(), freq);
						rep(repString);
					}
				}else{
					snprintf(repString, MYBUFSZ, "WARNING: %s made %d requests of '%s' for %d s. but has %d other requests. Ignoring.", 
								(*it).first.c_str(), diff, (*it1).first.c_str(), freq, (*it).second.size()-1);
					rep(repString);
				}
				break;
			}
		}
	}
	free(repString);
/*	
	for ( it=zoneCstats.begin() ; it != zoneCstats.end(); it++ ){
		for ( it1 = (*it).second.begin() ; it1 != (*it).second.end(); it1++ ){
			oldv = zoneCstatsold[(*it).first][(*it1).first];
			diff = zoneCstats[(*it).first][(*it1).first] - oldv;
			if(diff > maxChit){
				snprintf(repString, MYBUFSZ, "ALERT!! ZONE %s.0 made %d requests of '%s' for last %d seconds", 
							(*it).first.c_str(), diff, (*it1).first.c_str(), freq);
				rep(repString);
			}
		}
	}
*/
}

int SWexec(string act){
	char buffer[MYBUFSZ];
	string data;
	
	FILE* pp = popen(act.c_str(),"r");
	if(0>pp) 
		return -1;		
	while ( fgets(buffer, MYBUFSZ, pp) != NULL )
		data.append(buffer);
	return pclose(pp);
}

string SWexecR(string act){
	char buffer[MYBUFSZ];
	string data;
	
	FILE* pp = popen(act.c_str(),"r");
	if(0>pp) 
		return "";		
	while ( fgets(buffer, MYBUFSZ, pp) != NULL )
		data.append(buffer);
	pclose(pp);
	return data;
}

int IPTexec(string act){
	if(act=="") return 0;
	act = ConfS("IPT_PATH") + " " + act;
	return SWexec(act);
}

int IPTblock(string ip){
	string act = "";

	if(ConfS("IPT_CHAIN")=="")
		act += "-I INPUT -s ";
	else{
		act += "-I ";
		act += ConfS("IPT_CHAIN");
		act += " -s ";
	}
	act += ip;
	act += " -j ";
	act += ConfS("IPT_BLOCK_TARGET");

	//rep(act);

	return IPTexec(act);	
}

void printStats(){
/*
	statmap::iterator it;
	reqmap::iterator it1;
	for ( it=stats.begin() ; it != stats.end(); it++ ){
	//	if((*it).second > 100)
		//cout << (*it).first << endl;
		for ( it1 = (*it).second.begin() ; it1 != (*it).second.end(); it1++ ){
			//if((*it1).second > 10)
			cout << (*it).first << " : " << (*it1).first << " => " << (*it1).second << endl;
		}
	}
*/
}

bool isLocal(string ip){
	for(int j = 0; j < localAddrs.size(); j++) 
		if(localAddrs[j].compare(ip) == 0) 
			return true;
	return false;
}

void scanNetTcp(){
	ifstream netf;
	svec nline;
	string ip;
	unsigned long cline = 0;
	connmap::iterator i;
	reqmap::iterator ri;
	char* charbuf = (char*)malloc(500);
	
	netf.open("/proc/net/tcp", ios::in);
	
	while(!netf.eof()){
		netf.getline(charbuf, 500); 
		if(!cline++) continue;
		charbuf[500-1] = 0; //just in case :) 
		if(netf.gcount() <1) break;
		nline = explode(charbuf);
		if(nline[3]=="0A") continue; //listen sockets
		//cout<< charbuf<<endl;
		//cout << hex2ipv4(nline[1]) << endl;
		//cout << hex2ipv4(nline[2]) << endl;
		//if(!isLocal(hex2ipv4(nline[1])))
			conns[nline[1]].push_back(nline[2]);
		//else
			conns[nline[2]].push_back(nline[1]);
	}
	
	free(charbuf);
	netf.close();
	
	for( i = conns.begin(); i != conns.end(); i++){
		int sz = (*i).second.size();
		if(isLocal(hex2ipv4((*i).first))){
			if(sz <= 2 ) {conns.erase(i); continue;}
		}else if(sz > 1 ) {conns.erase(i); continue;}
		//cout<< (*i).first << " : " << sz << endl;
	}
	for( i = conns.begin(); i != conns.end(); i++){
		int sz = (*i).second.size();
		ip = hex2ipv4((*i).first);
		if(!isLocal(ip)){
		if(ipstat[ip]) ipstat[ip]++; else ipstat[ip]=1;
		}
		//cout<< (*i).first << " : " << sz << endl;
	}
	
	for( ri = ipstat.begin(); ri != ipstat.end(); ri++){
		cout<< (*ri).first << " : " << (*ri).second << endl;
	}
}

svec explode(const char* str, char delim )
{
	svec ret(1);
	ret[0] = "";
	
	long s = 0, p = 0;
	for(s = 0; str[s]; s++){		
		if(str[s] == delim){
			if(ret[p].length() < 2) continue;
			ret.resize(++p+1);
			ret[p] = "";	
		}else
		ret[p] += str[s];
	}
	//cleanup
	if(ret[p].length() < 2) ret.resize(p);
	return ret;
}

string hs2is(string h){
	char obuf[3];
	unsigned char c = 0, b;
	while(h.length()<2) h = "0" + h;
	for(char x = 0; x < 2; x++){
		if(h[x] >= '0' && h[x] <= '9') b = '0'; else b = 'A';
		c += (h[x] - b + (b == '0'?0:10) ) * (x ? 1:16);
	}
	sprintf(obuf, "%d", c); 
	return obuf;
}

string hex2ipv4(string h){
	string ret = "";
	char o=1;
	for(char x = 0; x < h.length();x+=2){
		ret = hs2is(h.substr(x,2)) + ret; 
		if(o++ < 4) ret = '.' + ret; else break; 
	}
	return ret;
}

svec getLocalAddr(){
	string ifc;
	svec ret(1), iflines;
	int j,k,f = 0; bool dups;
	ifc = SWexecR("/sbin/ifconfig");
	iflines = explode(ifc.c_str(),'\n');
	for(int i = 0; i < iflines.size()-1; i++){
		if(iflines[i][0]<=' ') continue;
		i++;
		j = k = iflines[i].find("addr:");
		if(j == string::npos) continue;
		
		for( j += 5; j<iflines[i].size() && iflines[i][j]>' '; j++){}
		ifc = iflines[i].substr(k+5,j-k-5);
		dups = false;
		for(j = 0; j < f; j++) {
			if(ret[j].compare(ifc) == 0) { 
				dups = true;
				break;
			}
		}
		if(dups) continue;
		ret.resize(f+1);
		ret[f++] = ifc;
	}
	return ret;
}

void sig_act(int signum)
{
	if(signum !=SIGHUP){
		rep("Caucht interrupt signal!");
		finalize(0);
	}else{
		rep("Caught SIGHUP! Reloading config and reopen logs...");
		if(ConfLoad("/etc/skillwatch.conf")!=1){
			perror("ERROR! Failed to reread config file.\n");
			rep("ERROR! Failed to reread config file.\n");
		}
		LogReopen();
	}
}

void finalize(int errcode=0){
	rep ("Started shutdown sequence..");
	timerInit(1);
	logwatchrun = false;
	rep("Waiting for all Watchers to die");
	while(logwatchcount>0)
		sleep(1);
	rep("All Watchers are dead");
	
	pthread_mutex_unlock(&mainlock);
	
	if( ConfI("IPT_CLEARONEXIT") && ConfS("IPT_CHAIN")!="" ){
		rep("Clearing own chain");
		IPTexec(((string)"-F ") + ConfS("IPT_CHAIN"));
	}
	if(repString!=NULL)		free(repString);
	
	if(SQLconn)				mysql_close(SQLconn);

	if(pidf)				close(pidf);
	unlink(ConfC("PATH_PIDFILE"));
	rep("SkillWATCH server turned off");
	exit(errcode);
}

//newline 
