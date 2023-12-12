#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>


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


    // Declare the arguments to be passed for each individual process
    char *server_arg_list[] = {"konsole", "-e", "./src/server", NULL};
    char *userinterface_arg_list[] = {"konsole", "-e", "./src/userinterface", NULL};
    char *drone_arg_list[] = {"konsole", "-e", "./src/drone", NULL};
    char *watchdog_arg_list[] = {"konsole", "-e", "./src/watchdog", NULL};


    // Perform a fork 4 times, executing a different process in each fork
    for (int i = 0; i < 4; i++)
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
                    printf("Created the watchdog with pid %d\n", pid);
                break;
            }
        }
    }


    // Wait for each child process to terminate
    for (int i = 0; i < 4; i++)
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