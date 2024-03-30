#include "includes/include.h"

pid_t process_pid[2*PLAYERS_PER_TEAM]; 
unsigned player_energy[2*PLAYERS_PER_TEAM]; 

int main() {

    
    for (int i = 0; i < 2*PLAYERS_PER_TEAM; i++) {
        //we will fork a child and execute the child program
        int current_pid;

        if ((current_pid = fork() == -1)) {
            perror("Fork failed.\n");
            exit(1);
        }

        char player_number_arg[4];
        char energy_arg[20];

        sprintf(player_number_arg, "%d", i);
        sprintf(energy_arg, "%u", i);
        


        if (current_pid == 0) {
            execl("./child", energy_arg , player_energy, (char*) NULL);
            perror("execl failed.\n");
            
            // To kill all child processes (from stack-overflow), might need to be tested. 

            /*
               One way to accomplish this is to deliver some signal that can be caught (not
               SIGKILL). Then, install a signal handler that detects if the current process is
               the parent process or not, and calls _exit() if it is not the parent process.(ibid, et al).
            */

            signal(SIGQUIT, SIG_IGN);
            kill(0, SIGQUIT);
            exit(2);


        } else {

            printf("Parent process with PID %d\n",getpid());

            process_pid[i] = current_pid;  
        }
    }

    return 0;
}

void assign_initial_energy() {

    
    for (int i = 0; i < 2*PLAYERS_PER_TEAM; i++) {

        player_energy[i] =  MIN_PLAYER_ENERGY + rand() % (abs(MAX_PLAYER_ENERGY - MIN_PLAYER_ENERGY));
    }
}
