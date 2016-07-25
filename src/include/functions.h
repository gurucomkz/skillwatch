#include <string.h>
#include <string>
#include <map>
#include <mysql.h>

using namespace std;

typedef map<string, string> shabl_type;

enum ENCODING{
	KOI,
	WIN,
	ISO,
	DOS,
	LAT	
};

void daemonize();
void daemonize(char*);

string transCode(ENCODING ,  
		 ENCODING ,  
		 const char *,  
		 bool mimetarget = false, 
		 bool ishdr = false);

string myReplace( char** ,  char**, const char* );
string str_replace(const char *, const char* , const char* );
string domext(const char*);
string domextA(const char*);
string dombase(const char*);

int fd_cmd(string cmd, string inv, string dom = "", string* fordata = NULL);

std::string getParamFromInfo(const char * info, const char * param, int *start=NULL); // Получения значения параметра из Info
std::string getContactParamFromInfo(const char * info, const char * contact, int *start=NULL);
std::string getDateFromInfo_crDate(const char*);	// Преобразование параметра crDate в формат, где только дата
std::string getDateFromInfo_crDateInSec(const char*);	//Преобразовываем из строки типа: 2006-05-07T05:56:30.000Z в секунды
std::string secToDate(long sec, int mod=0);	// Преобразование из секунд в фату типа 1998-05-28

//void dnsUpdate(const char* dnsAtReg, const char * dom_id); 	// Обработка DNS серверов

string makeMessage(const char *shablonText, shabl_type shablon);
string makeMessageA(const char *shablonText, shabl_type shablon,MYSQL *);
string defineMacros(const char *shablonText, shabl_type shablon);
string getShablonMakeMessageSendMessage(MYSQL *, shabl_type shablon);

