#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <errno.h>
#include "include/log.h"
#include "include/parameter.h"


// Declare global variables
FILE *logfd; // File pointer that contains the file descriptor for the log file
char *logpath = "src/include/log/main.log"; // Path for the log file


void interrupt_signal_handler(int signum)
{
    /*
    A signal handler that kills all child processes when CTRL+C is pressed 
    in the terminal before killing the main process itself.
    */

    kill(-2, SIGINT);
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = SIG_DFL;
    sigaction(SIGINT, &act, NULL);
    raise(SIGINT);

}


int main(int argc, char *argv[])
{
    // Declare a signal handler to handle SIGINT
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &interrupt_signal_handler;
    sigaction(SIGINT, &act, NULL);


    // Create a log file and open it
    logopen(logpath);


    // Declare the fds for the PID pipes
    int server2watchfd[2], UI2watchfd[2], drone2watchfd[2], targets2watchfd[2], obstacles2watchfd[2];
    int watch2serverfd[2], watch2UIfd[2], watch2dronefd[2], watch2targetsfd[2], watch2obstaclesfd[2];
    

    // Declare the fds for the data pipes
    int UI2server[2], server2UI[2], server2UItargetY[2], server2UItargetX[2], server2UIobstaclesY[2], server2UIobstaclesX[2];
    int drone2server[2], server2drone[2], server2droneobstaclesY[2], server2droneobstaclesX[2];
    int targets2server_1[2], targets2server_2[2], server2targets[2];
    int obstacles2server_1[2], obstacles2server_2[2], server2obstacles[2];


    //Declare the fds for the acknowledge pipes
    int UI_ack[2];
    int drone_ack[2];
    int target_ack[2];
    int obstacles_ack[2];


    // Create the PID pipes
    // Pipe for server PID
    if(pipe(server2watchfd) == -1) 
    {
        logerror(logpath, "error: server2watch create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2watch");
    }

    // Pipe for UI PID
    if(pipe(UI2watchfd) == -1)
    {
        logerror(logpath, "error: UI2watch create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe UI2watch");
    }

    // Pipe for drone PID
    if(pipe(drone2watchfd) == -1)
    {
        logerror(logpath, "error: drone2watch create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe drone2watch");
    }

    // Pipe for sending watchdog PID to server
    if(pipe(watch2serverfd) == -1)
    {
        logerror(logpath, "error: watch2server create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe watch2server");
    }

    // Pipe for sending watchdog PID to UI
    if(pipe(watch2UIfd) == -1)
    {
        logerror(logpath, "error: watch2UI create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe watch2UI");
    }

    // Pipe for sending watchdog PID to drone
    if(pipe(watch2dronefd) == -1)
    {
        logerror(logpath, "error: watch2drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe watch2drone");
    }

    // Pipe for sending keyvalues from UI to server
    if(pipe(UI2server) == -1)
    {
        logerror(logpath, "error: UI2server create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe UI2server");
    }

    // Pipe for sending droneposition from server to UI
    if(pipe(server2UI) == -1)
    {
        logerror(logpath, "error: server2UI create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2UI");
    }

    // Pipe for sending droneposition from drone to server
    if(pipe(drone2server) == -1)
    {
        logerror(logpath, "error: drone2server create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe drone2server");
    }

    // Pipe for sending keypressed value from server to drone
    if(pipe(server2drone) == -1)
    {
        logerror(logpath, "error: server2drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2drone");
    }

    // Pipe for UI to request for data from server
    if(pipe(UI_ack) == -1)
    {
        logerror(logpath, "error: UI_receive create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe UI_receive");
    }

    // Pipe for drone to request for data from server
    if(pipe(drone_ack) == -1)
    {
        logerror(logpath, "error: drone_receive create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe drone_receive");
    }

    // Pipe for targets PID
    if(pipe(targets2watchfd) == -1)
    {
        logerror(logpath, "error: targets2watchfd create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe targets2watchfd");
    }

    // Pipe for sending watchdog PID from watchdog to targets
    if(pipe(watch2targetsfd) == -1)
    {
        logerror(logpath, "error: watch2targetsfd create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe watch2targetsfd");
    }

    // Pipe for sending targets Y position from targets to server
    if(pipe(targets2server_1) == -1)
    {
        logerror(logpath, "error: targets2server_1 create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe targets2server_1");
    }

    // Pipe for sending targets X position from targets to server
    if(pipe(targets2server_2) == -1)
    {
        logerror(logpath, "error: targets2server_2 create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe targets2server_2");
    }

    // Pipe for sending drone position from server to targets
    if(pipe(server2targets) == -1)
    {
        logerror(logpath, "error: server2targets create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2targets");
    }

    // Pipe for targets to request for data from server
    if(pipe(target_ack) == -1)
    {
        logerror(logpath, "error: target_ack create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe target_ack");
    }

    // Pipe to send targets Y position from server to UI
    if(pipe(server2UItargetY) == -1)
    {
        logerror(logpath, "error: server2UItargetY create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2UItargetY");
    }

    // Pipe to send targets X position from server to UI
    if(pipe(server2UItargetX) == -1)
    {
        logerror(logpath, "error: server2UItargetX create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2UItargetX");
    }

    // Pipe for obstacles PID
    if(pipe(obstacles2watchfd) == -1)
    {
        logerror(logpath, "error: obstacles2watchfd create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe obstacles2watchfd");
    }

    // Pipe to send watchdog PID from watchdog to obstacles
    if(pipe(watch2obstaclesfd) == -1)
    {
        logerror(logpath, "error: watch2obstaclesfd create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe watch2obstaclesfd");
    }

    // Pipe to to send obstacles Y position from obstacles to server
    if(pipe(obstacles2server_1) == -1)
    {
        logerror(logpath, "error: obstacles2server_1 create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe obstacles2server_1");
    }

    // Pipe to send obstacles X position from obstacles to server
    if(pipe(obstacles2server_2) == -1)
    {
        logerror(logpath, "error: obstacles2server_2 create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe obstacles2server_2");
    }

    // Pipe to send drone position from server to obstacles
    if(pipe(server2obstacles) == -1)
    {
        logerror(logpath, "error: server2obstacles create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2obstacles");
    }

    // Pipe for obstacle to request for data from server
    if(pipe(obstacles_ack) == -1)
    {
        logerror(logpath, "error: obstacles_ack create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe obstacles_ack");
    }
    
    // Pipe for sending obstacles Y position from server to UI
    if(pipe(server2UIobstaclesY) == -1)
    {
        logerror(logpath, "error: server2UIobstaclesY create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2UIobstaclesY");
    }

    // Pipe for sending obstacle X position from server to UI
    if(pipe(server2UIobstaclesX) == -1)
    {
        logerror(logpath, "error: server2UIobstaclesX create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2UIobstaclesX");
    }

    // Pipe for sending obstacles Y position from server to drone
    if(pipe(server2droneobstaclesY) == -1)
    {
        logerror(logpath, "error: server2droneobstaclesY create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2droneobstaclesY");
    }

    // Pipe for sending obstacles X position from server to drone
    if(pipe(server2droneobstaclesX) == -1)
    {
        logerror(logpath, "error: server2droneobstaclesX create", errno);
    }
    else
    {
        logmessage(logpath, "Created pipe server2droneobstaclesX");
    }


    // Format the fds into argument strings
    char serverfds[200];
    char UIfds[200];
    char dronefds[200];
    char targetsfds[200];
    char obstaclesfds[200];
    char watchdogfds[200];
    sprintf(serverfds, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", server2watchfd[1], watch2serverfd[0], UI2server[0], server2UI[1], drone2server[0], server2drone[1], UI_ack[0], drone_ack[0], targets2server_1[0], targets2server_2[0], server2targets[1], target_ack[0], server2UItargetY[1], server2UItargetX[1], obstacles2server_1[0], obstacles2server_2[0], server2obstacles[1], obstacles_ack[0], server2UIobstaclesY[1], server2UIobstaclesX[1], server2droneobstaclesY[1], server2droneobstaclesX[1]);
    sprintf(UIfds, "%d,%d,%d,%d,%d,%d,%d,%d,%d", UI2watchfd[1], watch2UIfd[0], UI2server[1], server2UI[0], UI_ack[1], server2UItargetY[0], server2UItargetX[0], server2UIobstaclesY[0], server2UIobstaclesX[0]);
    sprintf(dronefds, "%d,%d,%d,%d,%d,%d,%d", drone2watchfd[1], watch2dronefd[0], drone2server[1], server2drone[0], drone_ack[1], server2droneobstaclesY[0], server2droneobstaclesX[0]);
    sprintf(targetsfds, "%d,%d,%d,%d,%d,%d", targets2watchfd[1], watch2targetsfd[0], targets2server_1[1], targets2server_2[1], server2targets[0], target_ack[1]);
    sprintf(obstaclesfds, "%d,%d,%d,%d,%d,%d", obstacles2watchfd[1], watch2obstaclesfd[0], obstacles2server_1[1], obstacles2server_2[1], server2obstacles[0], obstacles_ack[1]);
    sprintf(watchdogfds, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", server2watchfd[0], UI2watchfd[0], drone2watchfd[0], watch2serverfd[1], watch2UIfd[1], watch2dronefd[1], targets2watchfd[0], watch2targetsfd[1], obstacles2watchfd[0], watch2obstaclesfd[1]);


    // Declare the arguments to be passed for each individual process
    char *server_arg_list[] = {"konsole", "-e", "./src/server", serverfds, NULL};
    char *userinterface_arg_list[] = {"konsole", "-e", "./src/userinterface", UIfds, NULL};
    char *drone_arg_list[] = {"konsole", "-e", "./src/drone", dronefds, NULL};
    char *targets_arg_list[] = {"konsole", "-e", "./src/targets", targetsfds, NULL};
    char *obstacles_arg_list[] = {"konsole", "-e", "./src/obstacles", obstaclesfds, NULL};
    char *watchdog_arg_list[] = {"konsole", "-e", "./src/watchdog", watchdogfds, NULL};


    // Perform a fork 6 times, executing a different process in each fork
    for (int i = 0; i < 6; i++)
    {
        pid_t pid = fork(); 
        if (pid < 0) // If fork failed
        {
            perror("Failed to fork");
            exit(1);
        }
        else if (pid == 0) // If the child
        {
            if (i == 0)
            {
                // Execute the server
                execvp(server_arg_list[0], server_arg_list);
                perror("Failed to create server");
                return -1;
            }
            else if (i == 1)
            {
                // Execute the UI
                execvp(userinterface_arg_list[0], userinterface_arg_list);
                perror("Failed to create UI");
                return -1;
            }
            else if (i == 2)
            {
                // Execute the drone
                execvp(drone_arg_list[0], drone_arg_list);
                perror("Failed to create drone process");
                return -1;
            }
            else if (i ==3)
            {
                // Execute the targets
                execvp(targets_arg_list[0], targets_arg_list);
                perror("Failed to create targets process");
                return -1;
            }
            else if (i ==4)
            {
                // Execute the obstacles
                execvp(obstacles_arg_list[0], obstacles_arg_list);
                perror("Failed to create obstacles process");
                return -1;
            }
            else if (i ==5)
            {
                // Execute the watchdog
                execvp(watchdog_arg_list[0], watchdog_arg_list);
                perror("Failed to create watchdog process");
                return -1;
            }
        }
        else // If the parent
        {
            switch (i)
            {
                case 0:
                    printf("Created the server with pid %d\n", pid);
                break;

                case 1:
                    printf("Created the UI with pid %d\n", pid);
                break;

                case 2:
                    printf("Created the drone with pid %d\n", pid);
                break;

                case 3:
                    printf("Created the targets with pid %d\n", pid);
                break;

                case 4:
                    printf("Created the obstacles with pid %d\n", pid);
                break;

                case 5:
                    printf("Created the watchdog with pid %d\n", pid);
                break;
            }
        }
    }


    // Wait for each child process to terminate
    for (int i = 0; i < 6; i++)
    {
        int status;
        pid_t pid = wait(&status);
        if (WIFEXITED(status))
        {
            printf("Child %d exited normally\n", pid);
        }
        else
        {
            printf("Child %d exited abnormally\n", pid);
        }
    }


    return 0;
}