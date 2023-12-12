#ifndef PARAMETER_H
# define PARAMETER

#define DELTA_TIME_MILLISEC 100
#define DELTA_TIME_USEC 100000

#define WINDOW_SIZE_SHM "/window_server" 
#define USER_INPUT_SHM "/input_server"
#define DRONE_POSITION_SHM "/position_server"
#define WATCHDOG_SHM "/watchdog_server"
#define WINDOW_SHM_SEMAPHORE "/sem_window"
#define USER_INPUT_SEMAPHORE "/sem_input"
#define DRONE_POSITION_SEMAPHORE "/sem_drone"
#define WATCHDOG_SEMAPHORE "/sem_watchdog"

#define WINDOW_SHM_SIZE 3
#define USER_SHM_SIZE 3
#define DRONE_SHM_SIZE 5
#define WATCHDOG_SHM_SIZE 10

#define MASS 1
#define VISCOSITY 1
#define INPUT_FORCE 1


#endif