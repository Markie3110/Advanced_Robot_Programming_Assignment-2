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
char *logpath = "src/include/log/obstacles.log"; // Path for the log file
int obstacles2server1_write, obsacles2server2_write, server2obstacles_read, obstacle_ack; // Fds associated with the created pipes
int obstacles2watchfd, watch2obstaclesfd; /// Fds associated with the created pipes
int obstacles_XPos[20], obstacles_YPos[20]; // Variables to hold obstacles positions
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
    sscanf(argv[1], "%d,%d,%d,%d,%d,%d", &obstacles2watchfd, &watch2obstaclesfd, &obstacles2server1_write, &obsacles2server2_write, &server2obstacles_read, &obstacle_ack);


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
    write(obstacles2watchfd, pid_buf, sizeof(pid_buf));
    logint(logpath, "Obstacles PID", pid);
    logmessage(logpath, "sent pid to watchdog");


    // Receive the watchdog PID
    int watchdog_list[1] = {0};
    read(watch2obstaclesfd, watchdog_list, sizeof(watchdog_list));
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Declare a variable to count the number of cycles before the obstacles are refreshed
    int refresh_count = 100;
    int obstacle_count = 0;


    while(1)
    {
        logmessage(logpath, "Completed 1 cycle");


        if (refresh_count == 100)
        {
            // Generate a random number to represent the number of obstacles
            srandom(time(NULL));
            while ((obstacle_count < 5) || (obstacle_count > 20))
            {
                obstacle_count = random() % MAX_COUNT;
            }
            logint(logpath, "obstacle_count", obstacle_count);


            // Generate random x and y positions for each target in a target array
            for (int i = 0; i < obstacle_count; i++)
            {
                int y = random() % MAX_Y;
                int x = random() % MAX_X;
                obstacles_YPos[i] = y;
                obstacles_XPos[i] = x;
            }

            // NOTE: Code that logs obstacle data to the log file. Meant to be used during debugging. Can be commented out to improve performance otherwise.
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


            // Reset the refresh_count
            refresh_count = 0;


            // Check if the obstalce position matches the drone if yes, remove the obstacle from the array
            for (int i = 0; i < obstacle_count; i++)
            {
                if ((obstacles_YPos[i] >= (dronePosition[1]-2)) && (obstacles_YPos[i] <= (dronePosition[1]+2)) && (obstacles_XPos[i] >= (dronePosition[0]-2)) && (obstacles_XPos[i] <= (dronePosition[0]+2)))
                {
                    obstacles_YPos[i] = DISCARD;
                    obstacles_XPos[i] = DISCARD;
                    logmessage(logpath, "Drone and obstacle coincide");
                }
            }

            // NOTE: Code that logs obstacle data to the log file. Meant to be used during debugging. Can be commented out to improve performance otherwise.
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


            // Reset the buffers
            int serverread_buf[200];
            int serverwrite_buf[200];
            int serverack_buf[2];


            // Send the targets to the server
            write(obstacles2server1_write, obstacles_YPos, sizeof(obstacles_YPos));
            write(obsacles2server2_write, obstacles_XPos, sizeof(obstacles_XPos));
            

            // Receive the drone position from the server
            serverack_buf[0] = ACK_INT;
            write(obstacle_ack, serverack_buf, sizeof(serverack_buf));
            read(server2obstacles_read, serverread_buf, sizeof(serverread_buf));
            dronePosition[0] = serverread_buf[0];
            dronePosition[1] = serverread_buf[1];
            logint(logpath, "dronePositionY", dronePosition[0]);
            logint(logpath, "dronePositionX", dronePosition[1]);
        }


        // Increment the refresh count
        refresh_count++;


        if (flag == 1)
        {
            logmessage(logpath, "closing links and pipes");


            // Close the fd that sends obstacles Y position from obstacles to server
            if((close(obstacles2server1_write)) == -1)
            {
                logerror(logpath, "error: obstacles2server1_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obstacles2server1_write");
            }


            // Close the fd that sends obstacles X position from obstacles to server
            if((close(obsacles2server2_write)) == -1)
            {
                logerror(logpath, "error: obsacles2server2_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obsacles2server2_write");
            }


            // Close the fd that sends drone position from server to obstacles
            if((close(server2obstacles_read)) == -1)
            {
                logerror(logpath, "error: server2obstacles_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2obstacles_read");
            }


            // Close the fd that sends a request from the obstacles to the server for an update
            if((close(obstacle_ack)) == -1)
            {
                logerror(logpath, "error: obstacle_ack close", errno);
            }
            else
            {
                logmessage(logpath, "Closed obstacle_ack");
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