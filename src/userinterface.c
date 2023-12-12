#include <stdio.h>
#include <ncurses.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <curses.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <string.h>
#include <errno.h>
#include "include/log.h"
#include "include/parameter.h"


// Declare global variables
int flag = 0; // Flag variable to inform process to clean memory, close links and quit 
int watchdog_pid; // Variable to store the pid of the watchdog
char *logpath = "Assignment_1/log/UI.log"; // Path for the log file
FILE *logfd; // File pointer that contains the file descriptor for the log file


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


int main(int argc, char *argv)
{
    // Print the current working directory
    char cwd[100];
    getcwd(cwd, sizeof(cwd));
    printf("Current working directory: %s\n", cwd);

    
    // Create a log file and open it
    logopen(logpath);


    // Declare a signal handler to handle an INTERRUPT SIGNAL
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = &terminate_signal_handler;
    sigaction(SIGTERM, &act, NULL); 


    // Declare a signal handler to handle the signals received from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Declare variables
    int droneMaxY, droneMaxX; // Variables to hold the max and min positions the drone can move to
    int dronePositionX, dronePositionY; // Variables to gold the X and Y positions of the drone


    // Start ncurses
    initscr(); // Start ncurses
    noecho(); // Disable echoing of characters
    cbreak(); // Disable line buffering
    curs_set(0); // Make the cursor invisible
    start_color(); // Enable color
    logmessage(logpath, "initialised ncurses");


    // Initalize the color pairs 
    init_pair(1, COLOR_BLUE, COLOR_BLACK);


    // Get the windows maximum height and width
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);
    logint(logpath, "initial yMax", yMax);
    logint(logpath, "initial xMax", xMax);


    // Initialize the window variables
    WINDOW *dronewin;
    WINDOW *inspectwin;


    // Set the required window dimensions
    int droneWin_height, droneWin_width, droneWin_startY, droneWin_startX;
    droneWin_height = yMax/2;
    droneWin_width = xMax - 2;
    droneWin_startY = 1;
    droneWin_startX = 1;


    // Determine the dimensions of the inspector window
    int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
    inspWin_height = yMax/6;
    inspWin_width = droneWin_width;
    inspWin_startY = droneWin_height + (yMax/6);
    inspWin_startX = 1;


    // Maximum positions for drone
    droneMaxY = droneWin_height;
    droneMaxX = droneWin_width;
    logint(logpath, "droneMaxY", droneMaxY);
    logint(logpath, "droneMaxX", droneMaxX);


    int dimList[9] = {droneWin_height, droneWin_width, droneWin_startY, droneWin_startX, inspWin_height, inspWin_width, inspWin_startY, inspWin_startX};


    // Initialise the interface
    char *s = "DRONE WINDOW";
    mvprintw(0, 2, "%s", s);
    dronewin = newwin(dimList[0], dimList[1], dimList[2], dimList[3]);
    mvprintw(dimList[0]+1, 2, "List of commands:");
    char *s1 = "Reset: K    ";
    char *s2 = "Quit: L     ";
    char *s3 = "Stop: S     ";
    char *s4 = "Left: A     ";
    char *s5 = "Right: D    ";
    char *s6 = "Top: W      ";
    char *s7 = "Bottom: X   ";
    char *s8 =  "Top-Left: Q    ";
    char *s9=  "Top-Right: E   ";
    char *s10 = "Bottom-Left: Z ";
    char *s11 = "Bottom-Right: C";
    mvprintw(dimList[0]+2, 2, "%s %s", s1, s2);
    mvprintw(dimList[0]+3, 2, "%s %s %s %s %s %s %s %s %s", s3, s4, s5, s6, s7, s8, s9, s10, s11);
    mvprintw(dimList[6] - 1, 2, "INSPECTOR WINDOW");
    inspectwin = newwin(dimList[4], dimList[5], dimList[6], dimList[7]);
    box(dronewin, 0, 0);
    box(inspectwin, 0, 0);
    refresh();
    wrefresh(dronewin);
    wrefresh(inspectwin);


    // Create a FIFO to send the pid to the watchdog
    int UIpid_fd;
    char *UIpidfifo = "Assignment_1/src/tmp/UIpidfifo";
    while(1)
    {
        if(mkfifo(UIpidfifo, 0666) == -1)
        {
            logerror(logpath, "error: UIpidfifo create", errno);
            remove(UIpidfifo);
        }
        else
        {
            logmessage(logpath, "Created UIpidfifo");
            break;
        }
        usleep(10);
    }


    // Open the FIFO to send the pid to the watchdog
    UIpid_fd = open(UIpidfifo, O_WRONLY);
    if (UIpid_fd == -1)
    {
        logerror(logpath, "error: UIpidfifo open", errno);
    }
    else
    {
        logmessage(logpath, "Opened UIpidfifo");
    }


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(UIpid_fd, pid_buf, sizeof(pid_buf));
    logmessage(logpath, "Sent pid to watchdog");


    // Open the semaphore for the watchdog PID shared memory
    sem_t *sem_watchdog = sem_open(WATCHDOG_SEMAPHORE, O_CREAT, S_IRWXU | S_IRWXG, 1);
    while(1)
    {
        if (sem_watchdog == SEM_FAILED)
        {
            logerror(logpath, "error: sem_watchdog open", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_watchdog");
            break;
        }
        usleep(10);
    }


    // Open the shared memory for the watchdog PID
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


    // Create a FIFO to pass the keypressed values to the drone
    int keypressed_fd;
    char *keypressedfifo = "Assignment_1/src/tmp/keypressedfifo";
    while(1)
    {
        if(mkfifo(keypressedfifo, 0666) == -1)
        {
            logerror(logpath, "error: keypressedfifo create", errno);
            remove(keypressedfifo);
        }
        else
        {
            logmessage(logpath, "Created keypressedfifo");
            break;
        }
        usleep(10);
    }


    // Open the FIFO to pass the keypressed values to the drone
    keypressed_fd = open(keypressedfifo, O_WRONLY);
    if (keypressed_fd == -1)
    {
        logerror(logpath, "errorL keypressedfio open", errno);
    }


    // Open the semaphore for the window size shared memory
    sem_t *sem_window;
    while(1)
    {
        sem_window = sem_open(WINDOW_SHM_SEMAPHORE, 0);
        if (sem_window == SEM_FAILED)
        {
            logerror(logpath, "error: open sem_window", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_window");
            break;
        }
    }


    // Open the shared memory for the window size
    int shm_window;
    while(1)
    {
        sem_wait(sem_window);
        shm_window = shm_open(WINDOW_SIZE_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_window == -1)
        {
            logerror(logpath, "error: open shm_window", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_window");
            break;
        }
        sem_post(sem_window);
        usleep(10);
    }
    int *window_ptr = mmap(NULL, WINDOW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_window, 0);
    sem_post(sem_window);
    wrefresh(inspectwin);


    // Open the semaphore for the drone position shared memory
    sem_t *sem_drone;
    while(1)
    {
        sem_drone = sem_open(DRONE_POSITION_SEMAPHORE, 0);
        if (sem_drone == SEM_FAILED)
        {
            logerror(logpath, "error: open sem_drone", errno);
        }
        else
        {
            logmessage(logpath, "Opened sem_drone");
            break;
        }
        usleep(10);
    }


    // Open the shared memory for drone position
    int shm_drone;
    while(1)
    {
        sem_init(sem_drone, 1, 0);
        shm_drone = shm_open(DRONE_POSITION_SHM, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
        if (shm_drone == -1)
        {
            logerror(logpath, "error: open shm_drone", errno);
        }
        else
        {
            logmessage(logpath, "Opened shm_drone");
            break;
        }
        sem_post(sem_drone);
        usleep(10);
    }
    int *drone_ptr = mmap(NULL, DRONE_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_drone, 0);
    sem_post(sem_drone);
    wrefresh(inspectwin);


    // Run a loop with a time interval
    while(1)
    {
        // Declare a timeinterval to refresh the drone window at
        wtimeout(dronewin, DELTA_TIME_MILLISEC);


        // Grab the input for the current timecycle
        int c = wgetch(dronewin);
        logint(logpath, "user input", c);


        // Pass on the user input to the drone file if not a window resize
        if (c != 410) 
        {
            int buf[1] = {c};
            write(keypressed_fd, buf, sizeof(buf));
            logmessage(logpath, "passed input to drone");
        }


        // Pass the current window sizes to the drone
        int list[2] = {droneMaxY, droneMaxX};
        int size = sizeof(list);
        sem_wait(sem_window);
        memcpy(window_ptr, list, size);
        sem_post(sem_window);
        wrefresh(inspectwin);
        logmessage(logpath, "passed window size to drone");


        // Check if the window has been resized
        int yMax_now, xMax_now;
        getmaxyx(stdscr, yMax_now, xMax_now);
        logint(logpath, "yMax_now", yMax_now);
        logint(logpath, "xMax_now", xMax_now);
        if ((yMax_now != yMax) || (xMax_now != xMax))
        {
            // Change the maximum X and Y values
            yMax = yMax_now;
            xMax = xMax_now;


            // Set the required window dimensions
            int droneWin_height, droneWin_width, droneWin_startY, droneWin_startX;
            droneWin_height = yMax/2;
            droneWin_width = xMax - 2;
            droneWin_startY = 1;
            droneWin_startX = 1;


            // Determine the dimensions of the inspector window
            int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
            inspWin_height = yMax/6;
            inspWin_width = droneWin_width;
            inspWin_startY = droneWin_height + (yMax/6);
            inspWin_startX = 1;


            // Maximum positions for drone
            droneMaxY = droneWin_height;
            droneMaxX = droneWin_width;


            int dimList[9] = {droneWin_height, droneWin_width, droneWin_startY, droneWin_startX, inspWin_height, inspWin_width, inspWin_startY, inspWin_startX};


            // Resize the entire interface according to the new dimensions
            clear();
            wresize(dronewin, 1, 1);
            mvwin(dronewin, dimList[2], dimList[3]);
            wresize(dronewin, dimList[0], dimList[1]);
            mvprintw(dimList[0]+1, 2, "List of commands:");
            char *s1 = "Reset: K    ";
            char *s2 = "Quit: L     ";
            char *s3 = "Stop: S     ";
            char *s4 = "Left: A     ";
            char *s5 = "Right: D    ";
            char *s6 = "Top: W      ";
            char *s7 = "Bottom: X   ";
            char *s8 =  "Top-Left: Q    ";
            char *s9=  "Top-Right: E   ";
            char *s10 = "Bottom-Left: Z ";
            char *s11 = "Bottom-Right: C";
            mvprintw(dimList[0]+2, 2, "%s %s", s1, s2);
            mvprintw(dimList[0]+3, 2, "%s %s %s %s %s %s %s %s %s", s3, s4, s5, s6, s7, s8, s9, s10, s11);
            mvprintw(dimList[6] - 1, 2, "INSPECTOR WINDOW");
            wresize(inspectwin, 1, 1);
            mvwin(inspectwin, dimList[6], dimList[7]);
            wresize(inspectwin, dimList[4], dimList[5]);
            box(dronewin, 0, 0);
            box(inspectwin, 0, 0);
            refresh();
            wrefresh(dronewin);
            wrefresh(inspectwin);
            
        }
        

        // Access the drone location from shared memory 
        int dronePosition[2];
        size = sizeof(dronePosition);
        sem_wait(sem_drone);
        memcpy(dronePosition, drone_ptr, size);
        sem_post(sem_drone);
        dronePositionX = dronePosition[1];
        dronePositionY = dronePosition[0];
        wclear(inspectwin);
        box(inspectwin, 0, 0);
        mvwprintw(inspectwin, 1, 1, "Drone Position: %d %d", dronePosition[0], dronePosition[1]);
        wrefresh(inspectwin);
        logint(logpath, "dronePositionY", dronePositionY);
        logint(logpath, "dronePositionX", dronePositionX);


        // Display the drone 
        wclear(dronewin);
        box(dronewin, 0, 0);
        wattron(dronewin, COLOR_PAIR(1));
        mvwprintw(dronewin, dronePosition[0], dronePosition[1], "+");
        wattroff(dronewin, COLOR_PAIR(1));
        wrefresh(dronewin);


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


            // Close the FIFO that passes the keypressed value to the drone
            if((close(keypressed_fd)) == -1)
            {
                logerror(logpath, "error: keypressed_fd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed keypressed_fd");
            }


            // Close the FIFO that sends pid to the watchdog
            if((close(UIpid_fd)) == -1)
            {
                logerror(logpath, "error: UIpid_fd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed UIpid_fd");
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

    // Ncurses end
    endwin();

    return 0;
}