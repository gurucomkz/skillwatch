#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/types.h>
#include <mysql.h>
#include "const_c.h"

using namespace std;

typedef vector<string> Command;
typedef void(*timerAction)(void*);


#define PMUTINIT PTHREAD_MUTEX_INITIALIZER

#ifndef __DUMMY_DEF
#define __DUMMY_DEF 1
struct TimerEntry{
	timerAction Act;
	void* Params;
	unsigned long Freq;
	unsigned long Moment;
} ;


struct ClientConnection{
	unsigned long id;
	struct sockaddr *peer;
	int peer_sz;
	int sock;
};

const pthread_mutex_t inited_mutex = PMUTINIT;

#endif


int execute(char* );
char* mallocncopy(const char*);

int make_server_socket(int );
int make_server_socket_q(int , int );

char** exec_prepare(char*);
int newprocess(char*,char **);
void exec_args_clean(char** );

Command* SplitCommand(char *);
string * PrintDetailed(Command*);
string * PrintDetailed(char** );

double getDirSize(char* path);
void updClientUsage_r(void* );
int updClientUsage(MYSQL*,unsigned long);


int timerInit(bool);
void timerEnvoke(int);
void timerSetAction(unsigned long ,timerAction, void*, unsigned long);
void *timerLoop(void * );


//eof

