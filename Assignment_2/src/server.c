#include <stdio.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include "include/log.h"
#include "include/parameter.h"


// Declare global variables
int flag = 0; // Flag variable to inform process to clean memory, close links and quit 
int watchdog_pid; // Variable to store the pid of the watchdog
FILE *logfd; // File pointer that contains the file descriptor for the log file
char *logpath = "src/include/log/server.log"; // Path for the log file


// Declare FDs
int server2watchfd, watch2serverfd, UI2server_read, server2UI_write, drone2server_read, server2drone_write, UI_ack, drone_ack;
int targets2server1_read, targets2server2_read, server2targets_write, target_ack, server2UItargetY, server2UItargetX;
int obstacles2server1_read, obstacles2server2_read, server2obstacles_write, obstacles_ack, server2UIobstaclesY, server2UIobstaclesX, server2droneobstaclesY, server2droneobstaclesX;


// Declare data variables 
int dronePositionX, dronePositionY; // Variale to hold drone position
int keypressedValue; // Variable to hold keypressedvalue
int targets_YPos[20], targets_XPos[20]; // Variables to hold targets positon
int obstacles_YPos[20], obstacles_XPos[20]; // Variable to hold obstacles position


void terminate_signal_handler(int signum)
{
    /*
    A signal handler that sets the flag variable to 1 when receiving a terminate signal.
    */

    logmessage(logpath, "Received SIGTERM");
    flag = 1;

}


void watchdog_signal_handler(int signum)
{
    /*
    A signal handler that sends a response back to the watchdog after 
    receiving a SIGUSR1 signal from it.
    */

    kill(watchdog_pid, SIGUSR2);

}


int main(int argc, char *argv[])
{
    // Parse the needed file descriptors from the arguments list
    int targets2server1_read, targets2server2_read, server2targets_write, target_ack, server2UItargetY, server2UItargetX;
    sscanf(argv[1], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &server2watchfd, &watch2serverfd, &UI2server_read, &server2UI_write, &drone2server_read, &server2drone_write, &UI_ack, &drone_ack, &targets2server1_read, &targets2server2_read, &server2targets_write, &target_ack, &server2UItargetY, &server2UItargetX, &obstacles2server1_read, &obstacles2server2_read, &server2obstacles_write, &obstacles_ack, &server2UIobstaclesY, &server2UIobstaclesX, &server2droneobstaclesY, &server2droneobstaclesX);


    // Print the current working directory
    char cwd[100];
    getcwd(cwd, sizeof(cwd));
    printf("Current working directory: %s\n", cwd);


    // Create a log file and open it
    logopen(logpath);


    // Declare a signal handler to enable the flag when the SIGTERM signal is received
    struct sigaction act1;
    bzero(&act1, sizeof(act1));
    act1.sa_handler = &terminate_signal_handler;
    sigaction(SIGTERM, &act1, NULL);


    // Declare a signal handler to handle the signals received from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(server2watchfd, pid_buf, sizeof(pid_buf));
    logmessage(logpath, "sent pid to watchdog");

    // Receive the watchdog PID
    int watchdog_list[1] = {0};
    read(watch2serverfd, watchdog_list, sizeof(watchdog_list));
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Find the maxfd from the current set of fds
    int maxfd1 = -1;
    int fd1[4] = {UI2server_read, drone2server_read, targets2server1_read, obstacles2server1_read};
    for (int i = 0; i < 4; i++)
    {
        if (fd1[i] > maxfd1)
        {
            maxfd1 = fd1[i];
        }
    }


    // Find the maxfd from the current set of fds
    int maxfd2 = -1;
    int fd2[4] = {UI_ack, drone_ack, target_ack, obstacles_ack};
    for (int i = 0; i < 4; i++)
    {
        if (fd2[i] > maxfd2)
        {
            maxfd2 = fd2[i];
        }
    }


    // Run a loop with a time interval
    while (1)
    {
        // Add the wait interval for one cycle
        usleep(DELTA_TIME_USEC);
        logmessage(logpath, "Server performed 1 cycle");


        // Reset the buffers
        int UIread_buf[200];
        int droneread_buf[200];
        int UIwrite_buf[200];
        int dronewrite_buf[200];
        int targetsread1_buf[20];
        int targetsread2_buf[20];
        int targetswrite_buf[20];
        int obstaclesread1_buf[20];
        int obstaclesread2_buf[20];
        int obstacleswrite_buf[20];
        int UIack_buf[2];
        int droneack_buf[2];
        int targetack_buf[2];
        int obstaclesack_buf[2];


        // Receive the latest value of the variables
        fd_set rfds1;
        struct timeval tv1;
        FD_ZERO(&rfds1);
        FD_SET(UI2server_read, &rfds1);
        FD_SET(drone2server_read, &rfds1);
        FD_SET(targets2server1_read, &rfds1);
        FD_SET(obstacles2server1_read, &rfds1);
        tv1.tv_usec = 10000;
        tv1.tv_sec = 0;
        int retval1 = select((maxfd1+1), &rfds1, NULL, NULL, &tv1);
        if (retval1 == -1)
        {
            logerror(logpath, "error: select", errno);
        }
        for (int i = 0; i < 4; i++)
        {
            if(FD_ISSET(fd1[i], &rfds1))
            {
                if (fd1[i] == UI2server_read)
                {
                    read(UI2server_read, UIread_buf, sizeof(UIread_buf));
                    keypressedValue = UIread_buf[0];
                }
                else if (fd1[i] == drone2server_read)
                {
                    read(drone2server_read, droneread_buf, sizeof(droneread_buf));
                    dronePositionX = droneread_buf[0]; 
                    dronePositionY = droneread_buf[1];
                }
                else if (fd1[i] == targets2server1_read)
                {
                    read(targets2server1_read, targetsread1_buf, sizeof(targetsread1_buf));
                    read(targets2server2_read, targetsread2_buf, sizeof(targetsread2_buf));
                    for (int i = 0; i < (sizeof(targets_YPos)/4); i++)
                    {
                        targets_YPos[i] = targetsread1_buf[i];
                    }
                    for (int i = 0; i < (sizeof(targets_XPos)/4); i++)
                    {
                        targets_XPos[i] = targetsread2_buf[i];
                    }
                }
                else if (fd1[i] == obstacles2server1_read)
                {
                    read(obstacles2server1_read, obstaclesread1_buf, sizeof(obstaclesread1_buf));
                    read(obstacles2server2_read, obstaclesread2_buf, sizeof(obstaclesread2_buf));
                    for (int i = 0; i < (sizeof(obstacles_YPos)/4); i++)
                    {
                        obstacles_YPos[i] = obstaclesread1_buf[i];
                    }
                    for (int i = 0; i < (sizeof(obstacles_XPos)/4); i++)
                    {
                        obstacles_XPos[i] = obstaclesread2_buf[i];
                    }
                }
            }
        }
        logint(logpath, "keypressedValue", keypressedValue);
        logint(logpath, "dronePositionY", dronePositionY);
        logint(logpath, "dronePositionX", dronePositionX);

        // NOTE: Code that logs target and obstacle data to the log file. Meant to be used during debugging. Can be commented out to improve performance otherwise.
        // for (int i = 0; i < (sizeof(targets_YPos)/4); i++)
        // {
        //     logint(logpath, "targets_YPos", targets_YPos[i]);
        // }
        // logmessage(logpath, "Finished target ycoords");
        // for (int i = 0; i < (sizeof(targets_XPos)/4); i++)
        // {
        //     logint(logpath, "targets_XPos", targets_XPos[i]);
        // }
        // logmessage(logpath, "Finished target xcoords");
        // for (int i = 0; i < (sizeof(obstacles_YPos)/4); i++)
        // {
        //     logint(logpath, "obstacles_YPos", obstacles_YPos[i]);
        // }
        // logmessage(logpath, "Finished obstacles ycoords");
        // for (int i = 0; i < (sizeof(obstacles_XPos)/4); i++)
        // {
        //     logint(logpath, "obstacles_XPos", obstacles_XPos[i]);
        // }
        // logmessage(logpath, "Finished obstacles xcoords");


        // Send the updated values to the processes
        fd_set rfds2;
        struct timeval tv2;
        FD_ZERO(&rfds2);
        FD_SET(UI_ack, &rfds2);
        FD_SET(drone_ack, &rfds2);
        FD_SET(target_ack, &rfds2);
        FD_SET(obstacles_ack, &rfds2);
        tv2.tv_usec = 10000;
        tv2.tv_sec = 0;
        int retval2 = select((maxfd2+1), &rfds2, NULL, NULL, &tv2);
        if (retval2 == -1)
        {
            logerror(logpath, "error: select", errno);
        }
        for (int i = 0; i < 4; i++)
        {
            if(FD_ISSET(fd2[i], &rfds2))
            {
                if (fd2[i] == UI_ack)
                {
                    read(UI_ack, UIack_buf, sizeof(UIack_buf));
                    if(UIack_buf[0] == ACK_INT)
                    {
                        UIwrite_buf[0] = dronePositionX; 
                        UIwrite_buf[1] = dronePositionY;
                        write(server2UI_write, UIwrite_buf, sizeof(UIwrite_buf));    
                        write(server2UItargetY, targets_YPos, sizeof(targets_YPos));        
                        write(server2UItargetX, targets_XPos, sizeof(targets_XPos));
                        write(server2UIobstaclesY, obstacles_YPos, sizeof(obstacles_YPos));        
                        write(server2UIobstaclesX, obstacles_XPos, sizeof(obstacles_XPos));
                    }
                }
                else if (fd2[i] == drone_ack)
                {
                    read(drone_ack, droneack_buf, sizeof(droneack_buf));
                    if(droneack_buf[0] == ACK_INT)
                    {
                        dronewrite_buf[0] = keypressedValue;
                        write(server2drone_write, dronewrite_buf, sizeof(dronewrite_buf));
                        write(server2droneobstaclesY, obstacles_YPos, sizeof(obstacles_YPos));        
                        write(server2droneobstaclesX, obstacles_XPos, sizeof(obstacles_XPos));
                    }
                }
                else if (fd2[i] == target_ack)
                {
                    read(target_ack, targetack_buf, sizeof(targetack_buf));
                    if(targetack_buf[0] == ACK_INT)
                    {
                        targetswrite_buf[0] = dronePositionX; 
                        targetswrite_buf[1] = dronePositionY;
                        write(server2targets_write, targetswrite_buf, sizeof(targetswrite_buf));
                    }
                }
                else if (fd2[i] == obstacles_ack)
                {
                    read(obstacles_ack, obstaclesack_buf, sizeof(obstaclesack_buf));
                    if(obstaclesack_buf[0] == ACK_INT)
                    {
                        obstacleswrite_buf[0] = dronePositionX; 
                        obstacleswrite_buf[1] = dronePositionY;
                        write(server2obstacles_write, obstacleswrite_buf, sizeof(obstacleswrite_buf));
                    }
                }
            }
        }


        // If the flag is set, close all links and pipes and terminate the process 
        if (flag == 1)
        {
            logmessage(logpath, "Closing all links and pipes");


            // Close the fd that receives pid from the watchdog
            if((close(watch2serverfd)) == -1)
            {
                logerror(logpath, "error: watch2serverfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watch2serverfd");
            }

            // Close the fd that sends pid to the watchdog
            if((close(server2watchfd)) == -1)
            {
                logerror(logpath, "error: server2watchfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2watchfd");
            }


            // Close the fd that sends data from the UI to the server
            if((close(UI2server_read)) == -1)
            {
                logerror(logpath, "error: UI2server_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed UI2server_read");
            }


            // Close the fd that sends data from the server to the UI
            if((close(server2UI_write)) == -1)
            {
                logerror(logpath, "error: server2UI_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UI_write");
            }


            // Close the fd that sendsdata from the drone to the server
            if((close(drone2server_read)) == -1)
            {
                logerror(logpath, "error: drone2server_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone2server_read");
            }


            // Close the fd that sends data from the server to the drone
            if((close(server2drone_write)) == -1)
            {
                logerror(logpath, "error: server2drone_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2drone_write");
            }


            // Close the fd that sends a request from the UI to the server for an update
            if((close(UI_ack)) == -1)
            {
                logerror(logpath, "error: UI_ack close", errno);
            }
            else
            {
                logmessage(logpath, "Closed UI_ack");
            }


            // Close the fd that sends a request from the drone to the server for an update
            if((close(drone_ack)) == -1)
            {
                logerror(logpath, "error: drone_ack close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone_ack");
            }


            // Close the fd that sends targets Y position from targets to server
            if((close(targets2server1_read)) == -1)
            {
                logerror(logpath, "error: targets2server1_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed targets2server1_read");
            }


            // Close the fd that sends targets X position from targets to server
            if((close(targets2server2_read)) == -1)
            {
                logerror(logpath, "error: targets2server2_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed targets2server2_read");
            }


            // Close the fd that sends drone position from server to targets
            if((close(server2targets_write)) == -1)
            {
                logerror(logpath, "error: server2targets_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2targets_write");
            }


            // Close the fd that sends a request from the targets to the server for an update
            if((close(target_ack)) == -1)
            {
                logerror(logpath, "error: target_ack close", errno);
            }
            else
            {
                logmessage(logpath, "Closed target_ack");
            }


            // Close the fd that sends targets Y position from server to UI
            if((close(server2UItargetY)) == -1)
            {
                logerror(logpath, "error: server2UItargetY close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UItargetY");
            }


            // Close the fd that sends targets X position from server to UI
            if((close(server2UItargetX)) == -1)
            {
                logerror(logpath, "error: server2UItargetX close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UItargetX");
            }


            // Close the fd that sends obstacles Y position from obstacles to server
            if((close(obstacles2server1_read)) == -1)
            {
                logerror(logpath, "error: obstacles2server1_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obstacles2server1_read");
            }


            // Close the fd that sends obstacles X position from obstacles to server
            if((close(obstacles2server2_read)) == -1)
            {
                logerror(logpath, "error: obstacles2server2_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obstacles2server2_read");
            }


            // Close the fd that sends drone position from server to obstacles
            if((close(server2obstacles_write)) == -1)
            {
                logerror(logpath, "error: server2obstacles_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2obstacles_write");
            }


            // Close the fd that sends a request from the obstacles to the server for an update
            if((close(obstacles_ack)) == -1)
            {
                logerror(logpath, "error: obstacles_ack close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obstacles_ack");
            }


            // Close the fd that sends obstacles Y position from server to UI
            if((close(server2UIobstaclesY)) == -1)
            {
                logerror(logpath, "error: server2UIobstaclesY close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UIobstaclesY");
            }


            // Close the fd that sends obstacles X position from server to UI
            if((close(server2UIobstaclesX)) == -1)
            {
                logerror(logpath, "error: server2UIobstaclesX close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UIobstaclesX");
            }


            // Close the fd that sends obstacles Y position from server to drone
            if((close(server2droneobstaclesY)) == -1)
            {
                logerror(logpath, "error: server2droneobstaclesY close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2droneobstaclesY");
            }


            // Close the fd that sends obstacles X position from server to drone
            if((close(server2droneobstaclesX)) == -1)
            {
                logerror(logpath, "error: server2droneobstaclesX close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2droneobstaclesX");
            }


            // Kill the konsole and then self
            logmessage(logpath, "Killing process");
            kill(getppid(), SIGINT);
            struct sigaction act;
            bzero(&act, sizeof(act));
            act.sa_handler = SIG_DFL;
            sigaction(SIGTERM, &act, NULL);
            raise(SIGTERM);

            return -1;

        }
    }
    
    return 0;
}