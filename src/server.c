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
char *logpath = "Assignment_1/log/server.log"; // Path for the log file


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


    // Create a FIFO to send the pid to the watchdog
    int serverpid_fd;
    char *serverpidfifo = "Assignment_1/src/tmp/serverpidfifo";
    while(1)
    {
        if(mkfifo(serverpidfifo, 0666) == -1)
        {
            if (errno != EEXIST)
            {
                logerror(logpath, "error: serverpidfifo create", errno);
                remove(serverpidfifo);
            }
        }
        else
        {
            logmessage(logpath, "Created serverpidfifo");
            break;  
        }
        usleep(10);
    }


    // Open the FIFO to send the pid to the watchdog
    serverpid_fd = open(serverpidfifo, O_WRONLY);
    if (serverpid_fd == -1)
    {
        logerror(logpath, "error: serverpidfifo open", errno);
    }
    else
    {
        logmessage(logpath, "Opened serverpidfifo");
    }


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(serverpid_fd, pid_buf, sizeof(pid_buf));
    logmessage(logpath, "sent pid to watchdog");


    // Create the semaphore for the watchdog PID shared memory
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_watchdog == SEM_FAILED)
    {
        logerror(logpath, "error: sem_watchdog open", errno);
    }
    else
    {
        logmessage(logpath, "Opened sem_watchdog");
    }


    // Create the shared memory for the watchdog PID
    int shm_watchdog;
    while(1)
    {
        sem_init(sem_watchdog, 1, 0);
        shm_watchdog = shm_open(WATCHDOG_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_watchdog == -1)
        {
            logerror(logpath, "error: shm_watchdog open", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_watchdog");
            break;
        }
        sem_post(sem_watchdog);
        usleep(10);
    }
    ftruncate(shm_watchdog, DRONE_SHM_SIZE);
    int *watchdog_ptr = mmap(NULL, WATCHDOG_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_watchdog, 0);
    int buf[1] = {0};
    memcpy(watchdog_ptr, buf, sizeof(buf)); // Initialize the shared memory with a dummy value
    sem_post(sem_watchdog);


    // Read the watchdog PID from shared memory
    int watchdog_list[1] = {0};;
    int size = sizeof(watchdog_list);
    while (watchdog_list[0] == 0) // Keep reading until the shared memory is updated from its initial state
    {
        sem_wait(sem_watchdog);
        memcpy(watchdog_list, watchdog_ptr, size);
        sem_post(sem_watchdog);
        usleep(10);
    }
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Create the semaphore for the window size shared memory
    sem_t *sem_window = sem_open(WINDOW_SHM_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_window == SEM_FAILED)
    {
        logerror(logpath, "error: sem_window create", errno);
    }
    else
    {
        logmessage(logpath, "Created sem_window");
    }


    // Create the shared memory for the window size shared memory
    sem_init(sem_window, 1, 0);
    int shm_window = shm_open(WINDOW_SIZE_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_window == -1)
    {
        logerror(logpath, "error: shm_window create", errno);
    }
    else
    {
        logmessage(logpath, "Created shm_window");
    }
    ftruncate(shm_window, WINDOW_SHM_SIZE);
    void *window_ptr = mmap(NULL, WINDOW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_window, 0);
    sem_post(sem_window);


    // Create the semaphore for the drone position shared memory
    sem_t *sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    if (sem_drone == SEM_FAILED)
    {
        logerror(logpath, "error: sem_drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created sem_drone");
    }


    // Create the shared memory for the drone position
    sem_init(sem_drone, 1, 0);
    int shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shm_drone == -1)
    {
        logerror(logpath, "error: shm_drone create", errno);
    }
    else
    {
        logmessage(logpath, "Created shm_drone");
    }
    ftruncate(shm_drone, DRONE_SHM_SIZE);
    int *drone_ptr = mmap(NULL, DRONE_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_drone, 0);
    sem_post(sem_drone);


    // Run a loop with a time interval
    while (1)
    {
        usleep(DELTA_TIME_USEC);
        logmessage(logpath, "Server performed 1 cycle");
        

        // If the flag is set, close all links and pipes and terminate the process 
        if (flag == 1)
        {
            logmessage(logpath, "Closing all links and pipes");


            // Close and unlink the window size shared memory
            if((sem_close(sem_window)) == -1)
            {
                logerror(logpath, "error: sem_window close", errno);
            }
            else
            {
                logmessage(logpath, "Closed sem_window");
            }

            if((sem_unlink(WINDOW_SHM_SEMAPHORE)) == -1)
            {
                logerror(logpath, "error: sem_window unlink", errno);
            }
            else
            {
                logmessage(logpath, "Unlinked sem_window");
            }

            if((munmap(window_ptr, WINDOW_SHM_SIZE)) == -1)
            {
                logerror(logpath, "error: window_ptr close", errno);
            }
            else
            {
                logmessage(logpath, "Closed window_ptr");
            }

            if (shm_unlink(WINDOW_SIZE_SHM) == -1)
            {
                logerror(logpath, "error: window_shm close", errno);
            }
            else
            {
                logmessage(logpath, "Closed window_shm");
            }


            // Close and unlinke the drone positon shared memory
            if((sem_close(sem_drone)) == -1)
            {
                logerror(logpath, "error: sem_drone close", errno);
            }
            else
            {
                logmessage(logpath, "Closed sem_drone");
            }

            if((sem_unlink(DRONE_POSITION_SEMAPHORE)) == -1)
            {
                logerror(logpath, "error: sem_drone unlink", errno);
            }
            else
            {
                logmessage(logpath, "Unlinked sem_drone");
            }

            if((munmap(drone_ptr, DRONE_SHM_SIZE)) == -1)
            {
                logerror(logpath, "error: drone_ptr close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone_ptr");
            }

            if (shm_unlink(DRONE_POSITION_SHM) == -1)
            {
                logerror(logpath, "error: drone_shm close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone_shm");
            }


            // Close and unlink the watchdog pid shared memory
            if((sem_close(sem_watchdog)) == -1)
            {
                logerror(logpath, "error: sem_watchdog close", errno);
            }
            else
            {
                logmessage(logpath, "Closed sem_watchdog");
            }

            if((sem_unlink(WATCHDOG_SEMAPHORE)) == -1)
            {
                logerror(logpath, "error: sem_watchdog unlink", errno);
            }
            else
            {
                logmessage(logpath, "Unlinked sem_watchdog");
            }

            if((munmap(watchdog_ptr, WATCHDOG_SHM_SIZE)) == -1)
            {
                logerror(logpath, "error: watchdog_ptr close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watchdog_ptr");
            }

            if (shm_unlink(WATCHDOG_SHM) == -1)
            {
                logerror(logpath, "error: watchdog_shm close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watchdog_shm");
            }


            // Close the FIFO that sends pid to the watchdog
            if((close(serverpid_fd)) == -1)
            {
                logerror(logpath, "error: serverpid_fd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed serverpid_fd");
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