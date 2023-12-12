#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "log.h"


char currenttime[150]; // Global variable to hold the string depicting the current timestamp.


void* current_time()
{
    /*
    Returns a string depicting the current timestamp.
    */

    int day, month, year, hours, minutes, seconds, milliseconds, microseconds;

    time_t t_now;

    time(&t_now);

    struct tm *local = localtime(&t_now);

    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    local = localtime(&time_now.tv_sec);

    day = local -> tm_mday;
    month = local -> tm_mon + 1;
    year = local -> tm_year + 1900;
    hours = local -> tm_hour;
    minutes = local -> tm_min;
    seconds = local -> tm_sec;
    milliseconds = time_now.tv_usec/1000;
    microseconds = time_now.tv_usec%1000;

    sprintf(currenttime, "%02d-%02d-%02d %02d:%02d:%02d:%d:%d ", day, month, year, hours, minutes, seconds, milliseconds, microseconds);
    return &currenttime;
}


void logopen(char *logpath)
{
    /*
    Opens the logfile specified by logpath.
    */

    FILE *logfd = fopen(logpath, "w");
    if (logfd == NULL)
    {
        perror("Failed to create log");
    }
    else
    {
        printf("Created log file\n");
    }
    char *str1 = "DATE/TIME";
    char *str2 = "MESSAGE/VARIABLE";
    char *str3 = "DATA";
    fprintf(logfd, "%30s%35s%35s\n", str1, str2, str3);
    fclose(logfd);
}


void logdouble(char *logpath, char *variablename, double variablevalue)
{
    /*
    Logs a variable of type double to the logfile.
    */
   
    FILE *fd = fopen(logpath, "a");
    char *now = current_time();
    fprintf(fd, "%30s%35s%35.2f\n", now, variablename, variablevalue);
    fclose(fd);
}


void logerror(char *logpath, char *errormessage, int error)
{
    /*
    Takes the errno variable and logs its associated message into the logfile.
    */

    FILE *fd = fopen(logpath, "a");
    char *now = current_time();
    fprintf(fd, "%30s%35s%35s\n", now, errormessage, strerror(error));
    fclose(fd);
}


void logmessage(char *logpath, char *message)
{
    /*
    Logs a message specified by the programmer to the logfile.
    */

    FILE *fd = fopen(logpath, "a");
    char *now = current_time();
    char *str = "-\n";
    fprintf(fd, "%30s%35s%35s", now, message, str);
    fclose(fd);
}


void logint(char *logpath, char *variablename, int variablevalue)
{
    /*
    Logs a variable of type integer to the logfile.
    */

    FILE *fd = fopen(logpath, "a");
    char *now = current_time();
    fprintf(fd, "%30s%35s%35d\n", now, variablename, variablevalue);
    fclose(fd);
}