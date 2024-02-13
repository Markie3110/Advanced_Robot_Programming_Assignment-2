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
char *logpath = "src/include/log/UI.log"; // Path for the log file
FILE *logfd; // File pointer that contains the file descriptor for the log file
int UI2watchfd, watch2UIfd, UI2server_write, server2UI_read, UI_ack, server2UItargetY, server2UItargetX, server2UIobstaclesY, server2UIobstaclesX; // Fds associated with the created pipes
int dronePosition[2] = {15, 15}; // Variable to hold drone position
int c = -1; // Variable to hold user input
int targetcount = 0; // Variable to keep track of number of targets remaining
int firstrun = 4; // Variable that determines the number of cycles to be run before the targetcount becomes the determining variable to end the program


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
    sscanf(argv[1], "%d,%d,%d,%d,%d,%d,%d,%d,%d", &UI2watchfd, &watch2UIfd, &UI2server_write, &server2UI_read, &UI_ack, &server2UItargetY, &server2UItargetX, &server2UIobstaclesY, &server2UIobstaclesX);


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


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(UI2watchfd, pid_buf, sizeof(pid_buf));
    logint(logpath, "UI_PID", pid);
    logmessage(logpath, "sent pid to watchdog");


    // Receive the watchdog PID
    int watchdog_list[1] = {0};
    read(watch2UIfd, watchdog_list, sizeof(watchdog_list));
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Declare variables
    float droneMaxY, droneMaxX; // Variables to hold the max and min positions the drone can move to
    int dronePositionX, dronePositionY; // Variables to hold the X and Y positions of the drone
    int targets_YPos[20], targets_XPos[20]; // Variables to hold the targets positions
    int obstacles_YPos[20], obstacles_XPos[20]; // Variables to hold the obstacles position


    // Start ncurses
    initscr(); // Start ncurses
    noecho(); // Disable echoing of characters
    cbreak(); // Disable line buffering
    curs_set(0); // Make the cursor invisible
    start_color(); // Enable color
    logmessage(logpath, "initialised ncurses");


    // Initalize the color pairs 
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_color(COLOR_RED, 255, 255, 0);
    init_pair(3, COLOR_RED, COLOR_BLACK);


    // Get the windows maximum height and width
    int yMax, xMax;
    getmaxyx(stdscr, xMax, yMax); // Change
    logint(logpath, "initial xMax", xMax);
    logint(logpath, "initial yMax", yMax);


    // Initialize the window variables
    WINDOW *dronewin;
    WINDOW *inspectwin;


    // Set the required window dimensions
    float droneWin_height, droneWin_width; 
    int droneWin_startY, droneWin_startX;
    droneWin_height = xMax/2; // Change
    droneWin_width = yMax - 2;
    droneWin_startY = 1;
    droneWin_startX = 1;


    // Determine the dimensions of the inspector window
    int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
    inspWin_height = xMax/6;
    inspWin_width = droneWin_width;
    inspWin_startX = droneWin_height + (xMax/6);
    inspWin_startY = 1;


    // Maximum positions for drone
    droneMaxX = droneWin_height;
    droneMaxY = droneWin_width;
    logint(logpath, "droneMaxY", droneMaxY); 
    logint(logpath, "droneMaxX", droneMaxX);


    int dimList[9] = {droneWin_height, droneWin_width, droneWin_startX, droneWin_startY, inspWin_height, inspWin_width, inspWin_startX, inspWin_startY};


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


    // Run a loop with a time interval
    while(1)
    {
        // Declare a timeinterval to refresh the drone window at
        wtimeout(dronewin, DELTA_TIME_MILLISEC);


        // Reset the buffers
        int serverreadpos_buf[200];
        int serverreadtary_buf[200];
        int serverreadtarx_buf[200];
        int serverreadobsy_buf[200];
        int serverreadobsx_buf[200];
        int serverwrite_buf[200];
        int serverack_buf[2];


        // Grab the input for the current timecycle
        c = wgetch(dronewin);
        logint(logpath, "user input", c);


        // Send the keypressed value to the server
        serverwrite_buf[0] = c;
        write(UI2server_write, serverwrite_buf, sizeof(serverwrite_buf));


        // Check if the window has been resized
        int yMax_now, xMax_now;
        getmaxyx(stdscr, xMax_now, yMax_now);
        logint(logpath, "yMax_now", yMax_now);
        logint(logpath, "xMax_now", xMax_now); // CHANGE 
        if ((yMax_now != yMax) || (xMax_now != xMax))
        {
            // Change the maximum X and Y values
            yMax = yMax_now;
            xMax = xMax_now;


            // Set the required window dimensions
            float droneWin_height, droneWin_width;
            int droneWin_startY, droneWin_startX;
            droneWin_height = xMax/2;
            droneWin_width = yMax - 2;
            droneWin_startY = 1;
            droneWin_startX = 1;


            // Determine the dimensions of the inspector window
            int inspWin_height, inspWin_width, inspWin_startY, inspWin_startX;
            inspWin_height = xMax/6;
            inspWin_width = droneWin_width;
            inspWin_startX = droneWin_height + (xMax/6);
            inspWin_startY = 1;


            // Maximum positions for drone
            droneMaxX = droneWin_height;
            droneMaxY = droneWin_width; // CHANGE


            int dimList[9] = {droneWin_height, droneWin_width, droneWin_startX, droneWin_startY, inspWin_height, inspWin_width, inspWin_startX, inspWin_startY};


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


        // Get the updated drone, target and obstacle variables from the server
        serverack_buf[0] = ACK_INT;
        write(UI_ack, serverack_buf, sizeof(serverack_buf));
        read(server2UI_read, serverreadpos_buf, sizeof(serverreadpos_buf));
        dronePosition[0] = serverreadpos_buf[0];
        dronePosition[1] = serverreadpos_buf[1];
        logint(logpath, "Pipe_dronePositionY", dronePosition[0]);
        logint(logpath, "Pipe_dronePositionX", dronePosition[1]); 
        read(server2UItargetY, serverreadtary_buf, sizeof(serverreadtary_buf));
        for (int i = 0; i < (sizeof(targets_YPos)/4); i++)
        {
            targets_YPos[i] = serverreadtary_buf[i];
            //logint(logpath, "original_targets_YPos", targets_YPos[i]);
            targets_YPos[i] = targets_YPos[i] * (droneMaxY/MAX_Y);
            //logint(logpath, "targets_YPos", targets_YPos[i]);
        }
        logmessage(logpath, "Received target ycoords");
        read(server2UItargetX, serverreadtarx_buf, sizeof(serverreadtarx_buf));
        for (int i = 0; i < (sizeof(targets_XPos)/4); i++)
        {
            targets_XPos[i] = serverreadtarx_buf[i];
            //logint(logpath, "original_targets_XPos", targets_XPos[i]);
            targets_XPos[i] = targets_XPos[i] * (droneMaxX/MAX_X);
            //logint(logpath, "targets_XPos", targets_XPos[i]);
        }
        logmessage(logpath, "Received target xcoords");
        read(server2UIobstaclesY, serverreadobsy_buf, sizeof(serverreadobsy_buf));
        for (int i = 0; i < (sizeof(obstacles_YPos)/4); i++)
        {
            obstacles_YPos[i] = serverreadobsy_buf[i];
            //logint(logpath, "original_obstacles_YPos", obstacles_YPos[i]);
            obstacles_YPos[i] = obstacles_YPos[i] * (droneMaxY/MAX_Y);
            //logint(logpath, "obstacles_YPos", obstacles_YPos[i]);
        }
        logmessage(logpath, "Received obstacles ycoords");
        read(server2UIobstaclesX, serverreadobsx_buf, sizeof(serverreadobsx_buf));
        for (int i = 0; i < (sizeof(obstacles_XPos)/4); i++)
        {
            obstacles_XPos[i] = serverreadobsx_buf[i];
            //logint(logpath, "original_obstacles_XPos", obstacles_XPos[i]);
            obstacles_XPos[i] = obstacles_XPos[i] * (droneMaxX/MAX_X);
            //logint(logpath, "obstacles_XPos", obstacles_XPos[i]);
        }
        logmessage(logpath, "Received obstacles xcoords");


        // Display the drone 
        wclear(dronewin);
        box(dronewin, 0, 0);
        if ((targetcount > 0) || (firstrun > 0))
        {
            if (firstrun > 0)
            {
                firstrun--;
            }
            float scale;
            scale = dronePosition[0] * (droneMaxX/MAX_X); 
            logdouble(logpath, "ScaledY", scale);
            dronePosition[0] = scale; 
            scale = dronePosition[1] * (droneMaxY/MAX_Y); 
            logdouble(logpath, "ScaledX", scale);
            dronePosition[1] = scale; 
            wattron(dronewin, COLOR_PAIR(1));
            mvwprintw(dronewin, dronePosition[0], dronePosition[1], "+");
            wattroff(dronewin, COLOR_PAIR(1));
            logint(logpath, "dronePositionY", dronePosition[0]); 
            logint(logpath, "dronePositionX", dronePosition[1]);
            logint(logpath, "droneMaxY", droneMaxY);
            logint(logpath, "droneMaxX", droneMaxX);
            wclear(inspectwin);
            box(inspectwin, 0, 0);
            mvwprintw(inspectwin, 1, 1, "Drone Position: %d %d", dronePosition[0], dronePosition[1]); 
            mvwprintw(inspectwin, 2, 1, "Obstacles Remaining: %d", targetcount);
            wrefresh(inspectwin);


            // Display the targets
            int count = 1;
            targetcount = 0;
            for (int i = 0; i < (sizeof(targets_YPos)/4); i++)
            {
                if((targets_YPos[i] == DISCARD) || (targets_XPos[i] == DISCARD) || (targets_YPos[i] <= 0) || (targets_XPos[i] <= 0) || (targets_YPos[i] >= droneMaxY) || (targets_XPos[i] >= droneMaxX))
                {
                    logmessage(logpath, "Skipped target");
                    continue;
                }
                else
                {
                    logmessage(logpath, "Displayed target");
                    wattron(dronewin, COLOR_PAIR(2));
                    mvwprintw(dronewin, targets_XPos[i], targets_YPos[i], "%d", count);
                    wattroff(dronewin, COLOR_PAIR(2));
                    count++;
                    targetcount++;
                }
            }
            wrefresh(dronewin);


            // Display the objects
            for (int i = 0; i < (sizeof(obstacles_YPos)/4); i++)
            {
                if((obstacles_YPos[i] == DISCARD) || (obstacles_XPos[i] == DISCARD) || (obstacles_YPos[i] <= 0) || (obstacles_XPos[i] <= 0) || (obstacles_YPos[i] >= droneMaxY) || (obstacles_XPos[i] >= droneMaxX))
                {
                    logmessage(logpath, "Skipped obstacle");
                    continue;
                }
                else
                {
                    logmessage(logpath, "Displayed obstacle");
                    wattron(dronewin, COLOR_PAIR(3));
                    mvwprintw(dronewin, obstacles_XPos[i], obstacles_YPos[i], "0");
                    wattroff(dronewin, COLOR_PAIR(3));
                }
            }
            wrefresh(dronewin);
        }
        else
        {
            mvwprintw(dronewin, droneMaxX/2, droneMaxY/2, "||GAME OVER||");
            wrefresh(dronewin);
            wclear(inspectwin);
            box(inspectwin, 0, 0);
            mvwprintw(inspectwin, 1, 1, "Collected All Obstacles!");
        }


        // If the flag is set, close all links and pipes and terminate the process 
        if (flag == 1)
        {
            logmessage(logpath, "Closing all links and pipes");


            // Close the FIFO that passes the keypressed value to the drone
            if((close(UI2server_write)) == -1)
            {
                logerror(logpath, "error: UI2server_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed UI2server_write");
            }


            // Close the fd that sends pid to the watchdog
            if((close(UI2watchfd)) == -1)
            {
                logerror(logpath, "error: UI2watchfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed UI2watchfd");
            }


            // Close the fd that receives pid from the watchdog
            if((close(watch2UIfd)) == -1)
            {
                logerror(logpath, "error: watch2UIfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watch2UIfd");
            }


            // Close the fd that passes the drone position from server to UI
            if((close(server2UI_read)) == -1)
            {
                logerror(logpath, "error: server2UI_read close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2UI_read");
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


            // Ncurses end
            endwin();


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