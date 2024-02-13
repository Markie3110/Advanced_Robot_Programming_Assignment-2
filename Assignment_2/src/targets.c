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
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include "include/log.h"
#include "include/parameter.h"


// Declare global variables
int flag = 0; // Flag variable to inform process to clean memory, close links and quit 
int watchdog_pid; // Variable to store the pid of the watchdog
FILE *logfd; // File pointer that contains the file descriptor for the log file
char *logpath = "src/include/log/targets.log"; // Path for the log file
int targets2watchfd, watch2targetsfd, targets2server1_write, targets2server2_write, server2targets_read, target_ack; // Fds associated with the created pipes
int targets_XPos[20], targets_YPos[20]; // Variables to hold targets positions
int dronePosition[2] = {15, 15}; // Variable to hold drone position


void terminate_signal_handler(int signum)
{
    /*
    A signal handler that sets the flag variable to 1 when receiving a terminate signal.
    */

    logmessage(logpath ,"Received SIGTERM\n");
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
    sscanf(argv[1], "%d,%d,%d,%d,%d,%d", &targets2watchfd, &watch2targetsfd, &targets2server1_write, &targets2server2_write, &server2targets_read, &target_ack);


    // Create a log file and open it
    logopen(logpath);


    // Declare a signal handler to handle an INTERRUPT signal
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &terminate_signal_handler;
    sigaction(SIGTERM, &act, NULL); 


    // Declare a signal handle to handle the signals received from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(targets2watchfd, pid_buf, sizeof(pid_buf));
    logint(logpath, "Targets PID", pid);
    logmessage(logpath, "sent pid to watchdog");


    // Receive the watchdog PID
    int watchdog_list[1] = {0};
    read(watch2targetsfd, watchdog_list, sizeof(watchdog_list));
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);
    

    // Generate a random number to represent the number of targets
    int target_count = 0;
    srandom(time(NULL)*1000);
    while ((target_count < 5) || (target_count > 20))
    {
        target_count = random() % MAX_COUNT;
    }
    logint(logpath, "target_count", target_count);

    // Generate random x and y positions for each target in a target array
    for (int i = 0; i < target_count; i++)
    {
        int y = random() % MAX_Y;
        int x = random() % MAX_X;
        targets_YPos[i] = y;
        targets_XPos[i] = x;
    }


    // NOTE: Code that logs targets data to the log file. Meant to be used during debugging. Can be commented out to improve performance otherwise.
    // for (int i = 0; i < (sizeof(targets_YPos)/4); i++)
    // {
    //     logint(logpath, "targets_YPos_intial", targets_YPos[i]);
    // }
    // logmessage(logpath, "Finished target ycoords");
    // for (int i = 0; i < (sizeof(targets_XPos)/4); i++)
    // {
    //     logint(logpath, "targets_XPos_initial", targets_XPos[i]);
    // }
    // logmessage(logpath, "Finished target xcoords");


    while(1)
    {
        // Reset the buffers
        int serverread_buf[200];
        int serverwrite_buf[200];
        int serverack_buf[2];


        // Check if the target position matches the drone if yes, remove the target from the array
        for (int i = 0; i < target_count; i++)
        {
            if (((targets_YPos[i] == dronePosition[1]) && (targets_XPos[i] == dronePosition[0])) || (targets_YPos[i] <= 10) || (targets_XPos[i] <= 5) || (targets_YPos[i] >= (MAX_Y - 10)) || (targets_XPos[i] >= (MAX_X - 5)))
            {
                targets_YPos[i] = DISCARD;
                targets_XPos[i] = DISCARD;
                logmessage(logpath, "Drone and target coincide");
            }
        }


        // NOTE: Code that logs targets data to the log file. Meant to be used during debugging. Can be commented out to improve performance otherwise.
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


        // Send the targets to the server
        write(targets2server1_write, targets_YPos, sizeof(targets_YPos));
        write(targets2server2_write, targets_XPos, sizeof(targets_XPos));
        

        // Receive the drone position from the server
        serverack_buf[0] = ACK_INT;
        write(target_ack, serverack_buf, sizeof(serverack_buf));
        read(server2targets_read, serverread_buf, sizeof(serverread_buf));
        dronePosition[0] = serverread_buf[0];
        dronePosition[1] = serverread_buf[1];
        logint(logpath, "dronePositionY", dronePosition[0]);
        logint(logpath, "dronePositionX", dronePosition[1]);


        if (flag == 1)
        {
            logmessage(logpath, "closing links and pipes");


            // Close the fd that sends the pid to the watchdog
            if((close(targets2watchfd)) == -1)
            {
                logerror(logpath, "error: targets2watchfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed targets2watchfd");
            }


            // Close the fd that receives the pid from watchdog
            if((close(watch2targetsfd)) == -1)
            {
                logerror(logpath, "error: watch2targetsfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watch2targetsfd");
            }


            // Close the fd that sends targets Y position from targets to server
            if((close(targets2server1_write)) == -1)
            {
                logerror(logpath, "error: targets2server1_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed targets2server1_write");
            }


            // Close the fd that sends targets X position from targets to server
            if((close(targets2server2_write)) == -1)
            {
                logerror(logpath, "error: targets2server2_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed targets2server2_write");
            }


            // Close the fd that sends drone position from server to targets
            if((close(server2targets_read)) == -1)
            {
                logerror(logpath, "error: server2targets_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2targets_read");
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

        // Time interval one cycle is to be run at
        usleep(DELTA_TIME_USEC);
    }
}