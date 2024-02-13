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
#include <math.h>
#include "include/log.h"
#include "include/parameter.h"


// Declare global variables
int flag = 0; // Flag variable to inform process to clean memory, close links and quit 
int watchdog_pid; // Variable to store the pid of the watchdog
char *logpath = "src/include/log/drone.log"; // Path for the log file
FILE *logfd; // File pointer that contains the file descriptor for the log file 
int drone2watchfd, watch2dronefd, drone2server_write, server2drone_read, drone_ack, server2droneobstaclesY, server2droneobstaclesX; // Fds associated with the created pipes
int keyvalue = 0; // Variable to hold the value of the current key
int dronePosition[2] = {15, 15}; // Starting drone position
int obstacles_YPos[24], obstacles_XPos[24];  // Variables to hold obstacles positions
double distances[24]; // Variables to hold distances of drone from repulsive objects (obstacles and borders)
double ForceMagn[24], ForceAngle[24]; // Variables to hold the magnitude and direction of the repulsive forces created by each individual repulsive object
double RepForceY[24], RepForceX[24]; // Variables to hold the X and Y components of the repulsive forces created by each individual repulsive object


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


double CalculatePositionX(int forceX, double x1, double x2)
{
    /*
    Calculates the X position of the drone using the positions the drone occupied in the previous two time intervals, x1(one time interval) and 
    (two time intervals) x2 respectively.
    */

    double deltatime = 0.1;
    double num = (forceX*(deltatime*deltatime)) - (MASS * x2) + (2 * MASS * x1) + (VISCOSITY * deltatime * x1);
    double den = MASS + (VISCOSITY * deltatime);
    if (num == 0)
    {
        return 0;
    }
    else
    {
        return (num/den);
    }
    
}


double CalculatePositionY(int forceY, double y1, double y2)
{
    /*
    Calculates the Y position of the drone using the positions the drone occupied in the previous two time intervals, y1(one time interval) and 
    (two time intervals) y2 respectively.
    */

    double deltatime = 0.1;
    double num = (forceY*(deltatime*deltatime)) - (MASS * y2) + (2 * MASS * y1) + (VISCOSITY * deltatime * y1);
    double den = MASS + (VISCOSITY * deltatime);
    if (num == 0)
    {
        return 0;
    }
    else
    {
        return (num/den);
    }

}


double CalculateDistance(int obstacleY, int obstacleX)
{

    /*
    Calculates the absolute distance of an obstacle or boundary from the drone.
    */
    double y = dronePosition[1] - obstacleY;
    double x = dronePosition[0] - obstacleX; 
    double y_2 = pow(y, 2);
    double x_2 = pow(x, 2);
    return sqrt(y_2 + x_2);
}


double CalculateDistanceArray()
{
    /*
    Produces an array of distances of the drone from the various obstacles and boundaries. 
    */
   double dist;
   for (int i = 0; i < 24; i++)
    {
        dist = CalculateDistance(obstacles_YPos[i], obstacles_XPos[i]);
        distances[i] = dist;
    }
}

double CalculateAngle(int obstacleY, int obstacleX)
{
    /*
    Calculates the angle of the drone with respect to the obstacle.
    */
    double y = dronePosition[1] - obstacleY;
    double x = dronePosition[0] - obstacleX;
    logdouble(logpath, "Angle", atan2(x,y)); // CHANGE
    return atan2(x,y);
}


double CalculateMagnitudeAndDirection(double PositionChangeY, double PositionChangeX)
{
    /*
    Calculates the magnitude and direction of the repulsive forces created by the obstacles and boundaries.
    */
    for (int i = 0; i < 24; i++)
    {
        if (distances[i] > THRESHOLD)
        {
            ForceMagn[i] = 0;
            ForceAngle[i] = 0;
            logdouble(logpath, "Magnitude", ForceMagn[i]);
            logdouble(logpath, "Angle", ForceAngle[i]);
        }
        else
        {
            double changeY = pow(PositionChangeY, 2);
            double changeX = pow(PositionChangeX, 2);
            double change = sqrt(changeY + changeX);
            int scale = ((change + 1)/10) * SCALING_FACTOR;
            double num = scale * (THRESHOLD - distances[i]);
            double den = THRESHOLD * pow(distances[i], 3);
            double resultant;
            if (den == 0)
            {
                resultant = 0;
            }
            else
            {
                resultant = (num/den);
            }
            logdouble(logpath, "Magnitude", resultant);
            if (resultant > 15)
            {
                ForceMagn[i] = resultant;
            }
            else
            {
                ForceMagn[i] = resultant;
            }

            ForceAngle[i] = CalculateAngle(obstacles_YPos[i], obstacles_XPos[i]);
        }
    }
}


int main(int argc, char *argv[])
{
    // Parse the needed file descriptors from the arguments list
    sscanf(argv[1], "%d,%d,%d,%d,%d,%d,%d", &drone2watchfd, &watch2dronefd, &drone2server_write, &server2drone_read, &drone_ack, &server2droneobstaclesY, &server2droneobstaclesX);
    printf("%d %d\n", drone2watchfd, watch2dronefd);


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


    // Declare a signal handler to handle a SIGUSR1 signal sent from the watchdog
    struct sigaction act2;
    bzero(&act2, sizeof(act2));
    act2.sa_handler = &watchdog_signal_handler;
    sigaction(SIGUSR1, &act2, NULL);


    // Send the pid to the watchdog
    int pid = getpid();
    int pid_buf[1] = {pid};
    write(drone2watchfd, pid_buf, sizeof(pid_buf));
    logint(logpath, "Drone PID", pid);
    logmessage(logpath, "sent pid to watchdog");


    // Receive the watchdog PID
    int watchdog_list[1] = {0};
    read(watch2dronefd, watchdog_list, sizeof(watchdog_list));
    watchdog_pid = watchdog_list[0];
    logmessage(logpath, "received watchdog pid");
    logint(logpath, "watchdog_pid", watchdog_pid);


    // Declare variables
    int forceX = 0;
    int forceY = 0;
    int KeyforceX = 0;
    int KeyforceY = 0;
    dronePosition[0] = 0; 
    dronePosition[1] = 0; // Current drone position
    double droneStartPosition[2] = {15, 15}; // Drone starting position
    double dronePositionChange[2] = {0, 0}; // Change in drone position per interval
    dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
    dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
    double X[2] = {0, 0}; // Lists to hold drone X positions for the previous two iterations
    double Y[2] = {0, 0}; // Lists to hold drone Y positions for the previous two iterations


    // Run a loop with a time interval
    while(1)
    {
        // Reset the buffers
        int serverread_buf[200];
        int serverwrite_buf[200];
        int serverreadobsy_buf[20], serverreadobsx_buf[20];
        int serverack_buf[2];


        // Reset the forces
        forceX = KeyforceX;
        forceY = KeyforceY;


        // Send the drone position to the server
        serverwrite_buf[0] = dronePosition[0];
        serverwrite_buf[1] = dronePosition[1];
        write(drone2server_write, serverwrite_buf, sizeof(serverwrite_buf));


        // Get the updated keyvalue from the server
        serverack_buf[0] = ACK_INT;
        write(drone_ack, serverack_buf, sizeof(serverack_buf));
        read(server2drone_read, serverread_buf, sizeof(serverread_buf));
        keyvalue = serverread_buf[0];
        logint(logpath, "keyvalue", keyvalue);


        // Get the obstacles position from the server
        read(server2droneobstaclesY, serverreadobsy_buf, sizeof(serverreadobsy_buf));
        for (int i = 0; i < 20; i++)
        {
            obstacles_YPos[i] = serverreadobsy_buf[i];
            //logint(logpath, "obstacles_YPos", obstacles_YPos[i]);
        }
        logmessage(logpath, "Received obstacles ycoords");
        read(server2droneobstaclesX, serverreadobsx_buf, sizeof(serverreadobsx_buf));
        for (int i = 0; i < 20; i++)
        {
            obstacles_XPos[i] = serverreadobsx_buf[i];
            //logint(logpath, "obstacles_XPos", obstacles_XPos[i]);
        }
        logmessage(logpath, "Received obstacles ycoords");


        // Add the borders to the list of obstacles
        for (int i = 20; i < 24; i++)
        {
            switch (i)
            {
                case 20:
                    obstacles_YPos[i] = dronePosition[1];
                    obstacles_XPos[i] = 0;
                break;

                case 21:
                    obstacles_YPos[i] = MAX_Y;
                    obstacles_XPos[i] = dronePosition[0];
                break;

                case 22:
                    obstacles_YPos[i] = dronePosition[1];
                    obstacles_XPos[i] = MAX_X;
                break;

                case 23:
                    obstacles_YPos[i] = 0;
                    obstacles_XPos[i] = dronePosition[0];
                break;
            }
        }
        

        // Calculate the distance of the obstacles and borders from the drone and eject if beyond the threshold
        CalculateDistanceArray();


        // Find the magnitude and direction(theta) of the repulsive forces
        CalculateMagnitudeAndDirection(dronePositionChange[1], dronePositionChange[0]);


        // Break down the forces into their X and Y components
        for (int i = 0; i < 24; i++)
        {
            RepForceY[i] = ForceMagn[i] * (cos(ForceAngle[i]));
            RepForceX[i] = ForceMagn[i] * (sin(ForceAngle[i]));
        }


        // Sum the forces up
        for(int i = 0; i < 24; i++)
        {
            forceY = forceY + RepForceY[i];
            forceX = forceX + RepForceX[i];
        }


        // Calculate the drone position
        dronePositionChange[0] = CalculatePositionX(forceX, X[0], X[1]);
        dronePositionChange[1] = CalculatePositionY(forceY, Y[0], Y[1]);
        dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
        dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
        logint(logpath, "DronePositionX", dronePosition[0]);
        logint(logpath, "DronePositionY", dronePosition[1]);
        logint(logpath, "dronePositionChangeX", dronePositionChange[0]);
        logint(logpath, "dronePositionChangeY", dronePositionChange[1]);
        logint(logpath, "ForceY", forceY);
        logint(logpath, "ForceX", forceX);
        X[1] = X[0]; // Shift the last inteval X position to the one two intervals back
        X[0] = dronePositionChange[0]; // Shift the current drone X position to the last interval
        Y[1] = Y[0]; // Shift the last inteval Y position to the one two intervals back
        Y[0] = dronePositionChange[1]; // Shift the current drone Y position to the last interval


        // Set the force according to the key pressed
        if (keyvalue == 113)
        {
            KeyforceY = KeyforceY - INPUT_FORCE;
            KeyforceX = KeyforceX - INPUT_FORCE;
        }
        else if (keyvalue == 119)
        {
            KeyforceX = KeyforceX - INPUT_FORCE;
        }
        else if (keyvalue == 101)
        {
            KeyforceY = KeyforceY + INPUT_FORCE; 
            KeyforceX = KeyforceX - INPUT_FORCE;
        }
        else if (keyvalue == 97)
        {
            KeyforceY = KeyforceY - INPUT_FORCE;
        }
        else if (keyvalue == 115)
        {
            KeyforceX = 0;
            KeyforceY = 0;
        }
        else if (keyvalue == 100)
        {
            KeyforceY = KeyforceY + INPUT_FORCE;
        }
        else if (keyvalue == 122)
        {
            KeyforceY = KeyforceY - INPUT_FORCE;
            KeyforceX = KeyforceX + INPUT_FORCE;
        }
        else if (keyvalue == 120)
        {
            KeyforceX = KeyforceX + INPUT_FORCE;
        }
        else if (keyvalue == 99)
        {
            KeyforceY = KeyforceY + INPUT_FORCE;
            KeyforceX = KeyforceX + INPUT_FORCE;
        }
        else if (keyvalue == 107) 
        {
            // Reset the drone position to start
            logmessage(logpath, "Resetting Position");
            KeyforceX = 0;
            KeyforceY = 0;
            dronePosition[0] = 0;
            dronePosition[1] = 0;
            dronePositionChange[0] = 0;
            dronePositionChange[1] = 0;
            dronePosition[0] = droneStartPosition[0] + dronePositionChange[0];
            dronePosition[1] = droneStartPosition[1] + dronePositionChange[1];
            X[0] = 0;
            X[1] = 0;
            Y[0] = 0;
            Y[1] = 0;
        }
        else if (keyvalue == 108) // Quit the program
        {
            logmessage(logpath, "Closing system");
            kill(watchdog_pid, SIGUSR1);
        }


        // If the flag is set, close all links and pipes and terminate the process 
        if (flag == 1)
        {
            logmessage(logpath, "closing links and pipes");


            // Close the fd that passes the keypressed value to the drone
            if((close(server2drone_read)) == -1)
            {
                logerror(logpath, "error: server2drone_keypres close", errno);
            }
            else
            {
                logmessage(logpath, "Closed server2drone_keypres");
            }


            // Close the fd that sends pid to the watchdog
            if((close(drone2watchfd)) == -1)
            {
                logerror(logpath, "error: drone2watchfd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone2watchfd");
            }


            // Close the fd that receives pid from the watchdog
            if((close(watch2dronefd)) == -1)
            {
                logerror(logpath, "error: watch2dronefd close", errno);
            }
            else
            {
                logmessage(logpath, "Closed watch2dronefd");
            }


            // Close the fd that passes the drone position from the drone to server
            if((close(drone2server_write)) == -1)
            {
                logerror(logpath, "error: drone2server_write close", errno);
            }
            else
            {
                logmessage(logpath, "Closed drone2server_write");
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


        // Time interval one cycle is to be run at
        usleep(DELTA_TIME_USEC);
    }

    return 0;
}