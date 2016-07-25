#include <string>
#include <pthread.h>
#include <stdio.h>


#ifndef __LOGGER_DEF
#define __LOGGER_DEF


class LoggerClass{
	FILE* logfile;
	pthread_mutex_t	action_lock;
public:
	LoggerClass();
	~LoggerClass();

	void Write(const char* ,unsigned long );
	void Write(const std::string& ,unsigned long );
	void Write(const char* );
	void Write(const std::string& );
	void Reopen();
};

//define logger instance

#endif

void rep(const char* ,unsigned long );
void rep(const char * );
void rep(const std::string& );
void rep(const std::string&, unsigned long );
void LogReopen();



