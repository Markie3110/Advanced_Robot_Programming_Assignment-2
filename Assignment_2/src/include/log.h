#ifndef LOG_H_
#define LOG_H_

extern char currenttime[150];

void* current_time(void);

void logopen(char *logpath);

void logdouble(char *logpath, char *variablename, double variablevalue);

void logerror(char *logpath, char *errormessage, int error);

void logmessage(char *logpath, char *message);

void logint(char *logpath, char *variablename, int variablevalue);

#endif