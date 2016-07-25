#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <map>
#include <vector>
#include "base64.h"
#include <mysql.h>

#include "common.h"
#include "config.h"
#include "splog.h"
#include "functions.h"
#include "mydb.h"

#include <pcrecpp.h>

using namespace std;
static bool enc_is_set = false;

static map<ENCODING,string> EncConv;



#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
void daemonize(char* newroot)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir(newroot)) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}

void daemonize(){
	daemonize("/");
}

string domext(const char* dname){
	string rets = "";
	pcrecpp::RE re(".*\\.([\\.a-z0-9-]{1,40})");
	re.FullMatch(dname,&rets);
	return rets;
}
string domextA(const char* dname){
	string rets = "";
	pcrecpp::RE re("[^.]*\\.([\\.a-z0-9-]{1,40})");
	re.FullMatch(dname,&rets);
	return rets;
}
string dombase(const char* dname){
	string rets = "",obsolete = "";
	pcrecpp::RE re("(.*)\\.([a-z0-9-]{1,40})");
	re.FullMatch(dname,&rets,&obsolete);
	return rets;
}


int fd_cmd(string cmd, string inv, string dom, string* fordata)
{
	string act = ConfS("PATH_FDOMCTL");
	string data;
	char buffer[MYBUFSZ];
	
	act += " act="; act +=cmd; act +=" ";
	if(inv !="")
		act += " inv="; act += inv; 
	if(dom !="")
		act += " dom="; act += dom; 

	rep(act.c_str());

	FILE* pp = popen(act.c_str(),"r");
	if(0>pp) 
		return -1;		
	rep("pipe opened");
	while ( fgets(buffer, MYBUFSZ, pp) != NULL )
		data.append(buffer);
	if(data.length()) rep(data);
	int rt = pclose(pp);
	rt /=256;
	switch(rt){
		case 0: rep("FDOM ok."); break;
		case 1: rep("FDOM remote server returned false."); break;
		case 2: rep("FDOM incoice not found"); break;
		case 3: rep("FDOM domain not found"); break;
		case 10: rep("FDOM Wrong command"); break;
		default: 
			snprintf(buffer, MYBUFSZ, "FDOM returned unknown status %d", rt);
			rep(buffer); 
	}
	if(fordata!=NULL) 
		(*fordata) = data;
	return rt;	
}



string makeMessageA(const char *shablonText, shabl_type shablon, MYSQL * SQLconn)

{



	char *messageText;

	MYSQL_ROW randRow;

	DBRES randRes;

	if(shablon["fromName"].size()<1) shablon["fromName"] = ConfS("FROM_NAME");

	if(shablon["fromAddress"].size()<1) shablon["fromAddress"] = ConfS("FROM_ADDR");

	string ret, sendtext, subjectText;

	messageText = (char *) malloc (MYBUFSZ);

	randRes = db_query(SQLconn, "SELECT MD5(RAND())");

	randRow = mysql_fetch_row(randRes); //NextPart_000_0001_01C67736
	
	subjectText = defineMacros(shablon["subject"].c_str(), shablon);
	

	snprintf(messageText, MYBUFSZ, "Reply-To: <support@dnr.kz>\r\n"

			"From: \"%s\" <%s> \r\n"

			"To: <%s>\r\n"

			"Subject: =?windows-1251?B?%s?=\r\n"

			"Organization: SkillTex LLP\r\n"

			"MIME-Version: 1.0\r\n"

			"Content-Type: multipart/mixed; boundary=\"%5$s\"\r\n"

			"X-Priority: 3 (Normal)\r\n"

			"Importance: Normal\r\n"

			"X-MimeOLE: Produced By SkillTex\r\n\r\n"

			"\r\n"

			"--%5$s\r\n"

			"Content-Type: text/plain; charset=\"windows-1251\"\r\n"

			"Content-Transfer-Encoding: base64\r\n\r\n",
			shablon["fromName"].c_str(), 

			shablon["fromAddress"].c_str(), 

			shablon["toAddress"].c_str(), 

			base64_encode(reinterpret_cast<const unsigned char*>(subjectText.c_str()), subjectText.length()).c_str(),
			randRow[0]);
			

	ret = messageText;
	sendtext = defineMacros(shablonText, shablon);
	//ret += transCode(WIN, WIN, defineMacros(shablonText, shablon).c_str(), true);
	ret += base64_encode(reinterpret_cast<const unsigned char*>(sendtext.c_str()), sendtext.length());
	snprintf(messageText, MYBUFSZ, "\r\n\r\n"
			"--%s--\r\n"
			"\r\n.\r\n",randRow[0]
			);
	ret += messageText;
	if(messageText) free(messageText);

	return ret;

}




///////////////////////////////////////////////////////////////////////////////

std::string getParamFromInfo(const char * info, const char * param, int *start)
{
	using namespace std;
	int i0, i1, st=0;
	if(start!=NULL)
		st=*start;
	string inf = info;
	i0=inf.find(param, st);	//Поиск параметра
	if(i0!=string::npos)
	{
		// Найдена строка с искомым параметром, достаем само значение параметра
		i0 = inf.find("=", i0);
		i0+=2;
		i1 = inf.find("\n", i0);
		if(start!=NULL) 
			*start = i1;
		inf = inf.substr(i0, i1-i0);
		return inf;		
	}
	else
	{
		// Строка с параметром не найдена. Облом вышел, возвращаем ""
		return "";
	}
}



std::string getContactParamFromInfo(const char * info, const char * contact, int *start)

{

	using namespace std;

	int i0, i1, st=0;

	if(start!=NULL)

		st=*start;

	string inf = info;

	string fstr;

	fstr = "contact = ("+(string)contact+") ";

	i0=inf.find(fstr.c_str(), st);	//Поиск параметра

	if(i0!=string::npos)

	{
		i0+= fstr.size();

		// Найдена строка с искомым параметром, достаем само значение параметра

		i1 = inf.find("\n", i0);

		if(start!=NULL) 

			*start = i1;

		inf = inf.substr(i0, i1-i0);

		return inf;		

	}

	else

	{

		// Строка с параметром не найдена. Облом вышел, возвращаем ""

		return "";

	}

}

std::string getDateFromInfo_crDate(const char* crDate)
{
	// Преобразовываем из строки типа: 2006-05-07T05:56:30.000Z в дату типа 2006-05-07
	using namespace std;
	string inf = crDate;
	int i1;
	i1 = inf.find("T");
	inf = inf.substr(0, i1);
	return inf;
}

std::string getDateFromInfo_crDateInSec(const char* crDate)
{
	// Преобразовываем из строки типа: 2006-05-07T05:56:30.000Z в секунды
	using namespace std;
	struct tm tv;
	string inf = crDate, a;
	tv.tm_year = atoi((inf.substr(0, 4)).c_str())-1900;
	tv.tm_mon = atoi((inf.substr(5, 2)).c_str())-1;
	tv.tm_mday = atoi((inf.substr(8, 2)).c_str());
	tv.tm_hour = atoi((inf.substr(11, 2)).c_str());
	tv.tm_min = atoi((inf.substr(14, 2)).c_str());
 	tv.tm_sec = atoi((inf.substr(17, 2)).c_str());
	time_t time = mktime(&tv);
	char result[20];
	snprintf(result, 20, "%d", time);
	inf = result;
	return inf;
}

std::string secToDate(long sec, int mod)	// Преобразование из секунд в фату типа 1998-05-28
{
	char *result;
	time_t time = sec;
	tm tv;
	localtime_r(&time,&tv);
	result = (char *) malloc (MYBUFSZ);
	if(mod==0)
		snprintf(result, MYBUFSZ, "%.4d-%.2d-%.2d", tv.tm_year+1900, tv.tm_mon+1, tv.tm_mday);
	if(mod==1)
		snprintf(result, MYBUFSZ, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", tv.tm_year+1900, tv.tm_mon+1, tv.tm_mday, tv.tm_hour, tv.tm_min, tv.tm_sec);
	return (string) result;
}

string getShablonMakeMessageSendMessage(MYSQL * SQLconn, shabl_type shablon)	// Берет из базы шаблон для отправки сообщения. Корректирует макросы. Отправляет письмо
{
	//shablon:	shablonName - имя шаблона
	//		fromName	- имя отправителя
	//		fromAddress	- адрес отправителя
	//		toName	- имя получателя
	//		toAddress	- адрес получателя
	//		subject	- заголовок письма
	char *repString, *shablonQuery;//, *sendString;
	MYSQL_ROW shablonRow, headerRow;
	DBRES shablonTres, headerRes;
	FILE *mailSender;
	string retString="";
	
	char *mailSenderString = (char *) malloc (SMALLBUF);
	repString = (char *) malloc (MYBUFSZ);
	shablonQuery = (char *) malloc (SMALLBUF);
	snprintf(mailSenderString, SMALLBUF, "/usr/sbin/sendmail %s", shablon["toAddress"].c_str());
	rep(mailSenderString);
	mailSender = popen(mailSenderString, "w");
	if(!mailSender)
	{
		// Контакт с посыльным не установлен
		retString = "ERROR: Failed to fork sendmail";
		rep(retString.c_str());
		
	}
	else
	{
		//Запрос на текст шаблона
		snprintf(shablonQuery, SMALLBUF, "SELECT templ_id, templ_name, templ_text, temp_subject FROM dnr_mailtemplates WHERE templ_name='%s'", shablon["shablonName"].c_str());
		shablonTres = db_query(SQLconn, shablonQuery);
		if(!mysql_num_rows(shablonTres))
		{
			retString = "Message template not found (";
			retString += shablon["shablonName"];
			retString += ")";
			rep(retString.c_str());
			retString = "";
		}
		else
		{
			// если заголовок не указан прямо, то он может быть взят из БД
			shablonRow = mysql_fetch_row(shablonTres);
			shablon["subject"] = shablonRow[3];
			retString = makeMessageA(shablonRow[2], shablon,SQLconn);
			fprintf(mailSender, retString.c_str());
			snprintf(repString, MYBUFSZ, "Message '%s' for <%s> send successfully", shablon["shablonName"].c_str(), shablon["toAddress"].c_str(), shablon["subject"].c_str());
			rep(repString);
		}
		pclose(mailSender);
		if(shablonTres) mysql_free_result(shablonTres);
	}
	if(mailSenderString) free(mailSenderString);
	if(repString) free(repString);
	if(shablonQuery) free(shablonQuery);
	return retString;
}

string makeMessage(const char *shablonText, shabl_type shablon)
{
	// Формирование текста сообщения
	char *messageText;
	if(shablon["fromName"]=="") shablon["fromName"] = ConfS("FROM_NAME");

	if(shablon["fromAddress"]=="") shablon["fromAddress"] = ConfS("FROM_ADDR");
	string ret;
	messageText = (char *) malloc (MYBUFSZ);
	snprintf(messageText, MYBUFSZ, "Reply-To: <support@dnr.kz>\r\n"
			"From: \"%s\" <%s> \r\n"
			"To: <%s>\r\n"
			"Subject: %s\r\n"
			"Organization: SkillTex LLP\r\n"
			"MIME-Version: 1.0\r\n"
			"Content-Type: multipart/alternative; boundary=\"----=_NextPart_000_0001_01C67736.SkillTex\"\r\n"
			"X-Priority: 3 (Normal)\r\n"
			//"X-MSMail-Priority: Normal\r\n"
			"Importance: Normal\r\n"
			"X-MimeOLE: Produced By SkillTex\r\n\r\n"
			"This is a multi-part message in MIME format.\r\n\r\n"
			"------=_NextPart_000_0001_01C67736.SkillTex\r\n"
			"Content-Type: text/plain; charset=\"windows-1251\"\r\n"
			"Content-Transfer-Encoding: quoted-printable\r\n\r\n"
			"%s\r\n\r\n"
			"------=_NextPart_000_0001_01C67736.SkillTex--\r\n"
			"\r\n.\r\n", 
			shablon["fromName"].c_str(), shablon["fromAddress"].c_str(), shablon["toAddress"].c_str(), shablon["subject"].c_str(), transCode(WIN, WIN, defineMacros(shablonText, shablon).c_str(), true).c_str());
	ret = messageText;
	if(messageText) free(messageText);
	
	/*rep("Шлю: ");
	rep(ret.c_str());*/
	return ret;
}
string defineMacros(const char *shablonText, shabl_type shablon)
{
	// Макросо подстановщик, который заменяет в shablonText все макросы по карте shablon
	int i0=0, i1=0, len=0;
	string text, ret="";
	
	text = shablonText;
	len = strlen(shablonText);
	while((i1=text.find("<", i0))!=string::npos)
	{
		ret += text.substr(i0, i1-i0);
		i0=i1+1;
		i1 = text.find(">", i0);
		if(i1==string::npos)
		{
			rep("Gluky v stroke shablona. Ne naydeno'>'");
			i0++;
			continue;
		}
		ret += shablon[text.substr(i0, i1-i0)];
		i0 = i1+1;
	}
	ret += text.substr(i0, len-i0);
	ret += "\r\n";
	return ret;
}

static char *encWIN[]   = {"\"", "\xB9", "-",  "=", "\xC0", "\xC1", "\xC2", "\xC3", "\xC4", "\xC5", "\xA8", "\xC6", "\xC7", "\xC8", "\xC9", "\xCA", "\xCB", "\xCC", "\xCD", "\xCE", "\xCF", "\xD0", "\xD1", "\xD2", "\xD3", "\xD4", "\xD5", "\xD6", "\xD7", "\xD8", "\xD9", "\xDA", "\xDB", "\xDC", "\xDD", "\xDE", "\xDF", "\xE0", "\xE1", "\xE2", "\xE3", "\xE4", "\xE5", "\xB8", "\xE6", "\xE7", "\xE8", "\xE9", "\xEA", "\xEB", "\xEC", "\xED", "\xEE", "\xEF", "\xF0", "\xF1", "\xF2", "\xF3", "\xF4", "\xF5", "\xF6", "\xF7", "\xF8", "\xF9", "\xFA", "\xFB", "\xFC", "\xFD", "\xFE", "\xFF", 0};

static char *encDOS[]   = {"\"", "\xEF", "-",  "=", "\x80", "\x81", "\x82", "\x83", "\x84", "\x85", "\xF0", "\x86", "\x87", "\x88", "\x89", "\x8A", "\x8B", "\x8C", "\x8D", "\x8E", "\x8F", "\x90", "\x91", "\x92", "\x93", "\x94", "\x95", "\x96", "\x97", "\x98", "\x99", "\x9A", "\x9B", "\x9C", "\x9D", "\x9E", "\x9F", "\xA0", "\xA1", "\xA2", "\xA3", "\xA4", "\xA5", "\xF1", "\xA6", "\xA7", "\xA8", "\xA9", "\xAA", "\xAB", "\xAC", "\xAD", "\xAE", "\xAF", "\xE0", "\xE1", "\xE2", "\xE3", "\xE4", "\xE5", "\xE6", "\xE7", "\xE8", "\xE9", "\xEA", "\xEB", "\xEC", "\xED", "\xEE", "\xEF",0};
		  
static char *encLAT []  = {"\"",  "#",  "-",    "=", "A",  "B",  "V",  "G",  "D",  "E",  "E", "Zh", "Z", "I", "I", "K", "L", "M", "N", "O", "P", "R", "S", "T", "U", "F", "H", "Ts", "Ch", "Sh", "Sch", "\"", "Y", "\"", "E", "Yu", "Ya", "a", "b", "v", "g", "d", "e", "e", "zh", "z", "i", "i", "k", "l", "m", "n", "o", "p", "r", "s", "t", "u", "f", "h", "ts", "ch", "sh", "sch", "`", "y", "\"", "e", "yu", "ya",0};

static char *mimeKOI[] = {"\"", "#", "-",  "=3D", "=E1", "=E2", "=F7", "=E7", "=E4", "=E5", "=B3", "=F6", "=FA", "=E9", "=EA", "=EB", "=EC", "=ED", "=EE", "=EF", "=F0", "=F2", "=F3", "=F4", "=F5", "=E6", "=E8", "=E3", "=FE", "=FB", "=FD", "=FF", "=F9", "=F8", "=FC", "=E0", "=F1", "=C1", "=C2", "=D7", "=C7", "=C4", "=C5", "=A3", "=D6", "=DA", "=C9", "=CA", "=CB", "=CC", "=CD", "=CE", "=CF", "=D0", "=D2", "=D3", "=D4", "=D5", "=C6", "=C8", "=C3", "=DE", "=DB", "=DD", "=DF", "=D9", "=D8", "=DC", "=C0", "=D1",0};

static char * mimeWIN[] = {"\"", "#", "-", "=3D", "=C0", "=C1", "=C2", "=C3", "=C4", "=C5", "=A8", "=C6", "=C7", "=C8", "=C9", "=CA", "=CB", "=CC", "=CD", "=CE", "=CF", "=D0", "=D1", "=D2", "=D3", "=D4", "=D5", "=D6", "=D7", "=D8", "=D9", "=DA", "=DB", "=DC", "=DD", "=DE", "=DF", "=E0", "=E1", "=E2", "=E3", "=E4", "=E5", "=B8", "=E6", "=E7", "=E8", "=E9", "=EA", "=EB", "=EC", "=ED", "=EE", "=EF", "=F0", "=F1", "=F2", "=F3", "=F4", "=F5", "=F6", "=F7", "=F8", "=F9", "=FA", "=FB", "=FC", "=FD", "=FE", "=FF",0};


void EncConvSet(){
	if(enc_is_set) return;
	enc_is_set = true;
	EncConv[KOI]="koi8-r";
	EncConv[WIN]="windows-1251";
	EncConv[LAT]="iso-8859-15";	
}

string str_replace(const char *from, const char* to, const char* source)
{		
	// Макросо подстановщик, который заменяет в shablonText все макросы по карте shablon
	int i,i0=0, i1=0, len=0;
	string text, ret="";
	text = source;
	//char * x;
	//x = (char *) malloc (MYBUFSZ);
// 	rep("str_replace << ");
// 	rep(source);
	while((i1=text.find(from, i0))!=string::npos){
		ret += text.substr(i0, i1-i0);
// // 		rep("found");
// 		rep(from);
		i0=i1+strlen(from);
		ret += to;
// 		rep(ret);			
	}	

	ret += text.substr(i0, text.size()-i0);
// 	rep(">>");
// 	rep(ret.c_str());
	
	return ret;
}

string transCode(ENCODING target,  
		 ENCODING source,  
		 const char *subj,  
		 bool mimetarget , 
		 bool ishdr )
{
	EncConvSet();
	char** t = NULL, **f = NULL;
	string x = "";
	/*rep("HERE GOES INPUT:");
	rep (subj);*/

	if(ishdr && (target==KOI || target==WIN)){
		x= "=?"+EncConv[target]+"?Q?"+
				str_replace(" ", "_", transCode(target,  source,  subj,  true).c_str())+"?=";
		return x;			

	}
// 	rep(EncConv[target]);
	switch(target){
		case KOI: t =(mimetarget)?mimeKOI: NULL; /*encKOI;*/ break;
		case LAT: t = encLAT; break;
		case DOS: t = encDOS;break;
		case WIN: t =(mimetarget)?mimeWIN: encWIN;break;
	}

// 	rep(EncConv[source]);
	switch(source){
		//case KOI: t = encKOI; break;
		case LAT: f = encLAT; break;
		case DOS: f = encDOS; break;
		case WIN: f = encWIN; break;
	}
	
	if(f && t) 
		x = myReplace(f,  t,  subj);		
	else
		x = subj;
	
	/*rep("HERE GOES RESULT:");
	rep(x);*/

	return x;
}

string myReplace( char** from,   char**to,  const char* subj) {
	long ln;
	string ss , ss1 = "";
	ss = subj;
	ln  = strlen(subj);
	for(long p = 0; from[p] && to[p]; p++) {
		//rep("p");
		//rep(from[p]);
		ss1 = str_replace(from[p],  to[p],  ss.c_str());
		ss = ss1;
	}

	return ss;
}
