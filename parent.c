#include "includes/include.h"
#include "includes/std.h"

pid_t process_pid[2*PLAYERS_PER_TEAM]; 
unsigned player_energy[2*PLAYERS_PER_TEAM]; 
unsigned next_player; /* each process will have a different next player pid,
                       this is used to index the pids array and send the pid to the process when we use execlp. arg[3] */ 

int status, i;

int number_of_processes_waiting_for_read = 0;
//create structs 
team team1;
team team2;
//init the two structs to 0 balls and 0 score


// FUNCTION PROTOTYPES

void create_FIFOs();
void assign_initial_energy();
void fork_children();
void fork_gui();
void init_teams();
void read_parameters(int argc,char* argv[]);
void create_shared_mem(); 
void doOneRound();

int gui_pid = -1;

// GLOBAL VARIABLES

unsigned int round_duration;
unsigned int number_of_rounds;
int current_round_number = 0;

struct sigaction sa_chld, sa_io, sa_alarm, sa_usr1, sa_usr2;;

int fd_shm;
// Define a structure to hold the flag
struct shared_data {
    int ignore_signals; // 0 means not ignore, 1 means ignore
};

struct shared_data *shared_mem;


// SIGNAL HANDLERS


void signal_handler(int signum) {

    if (signum == SIGUSR1) {

        team1.number_of_balls++;
        team2.number_of_balls--;

        // if team2 has no balls, send a signal to the team2 leader to get a ball. 
        if (team2.number_of_balls == 0) {
            kill(process_pid[11], SIGUSR1);
        }

    }

    if (signum == SIGUSR2) {

        team2.number_of_balls++;
        team1.number_of_balls--;

        // if team1 has no balls, send a signal to the team1 leader to get a ball.
        if (team1.number_of_balls == 0) {
            kill(process_pid[5], SIGUSR1);
        }
    }


    if (signum == SIGIO) {
        
        printf("entrered SIGIO from parent\n");
    }

    fflush(stdout);
}

void alarm_handler(int signum) {

    
    shared_mem->ignore_signals = 1;


        my_pause(4);

    if (team1.number_of_balls < team2.number_of_balls) {
        team1.total_score++;
    }

    else if (team1.number_of_balls > team2.number_of_balls) {
        team2.total_score++;
    }


    cyan_stdout();
    printf("Team 1 score: %d, and number of balls: %d\n", team1.total_score, team1.number_of_balls);
    printf("Team 2 score: %d, and number of balls: %d\n", team2.total_score, team2.number_of_balls);
    yellow_stdout();
    printf("Round %d finished\n", current_round_number);
    reset_stdout();
    my_pause(4);

    if (current_round_number < number_of_rounds) {
        doOneRound();
    }

    else {
        // print the final scores and the winner
        if (team1.total_score > team2.total_score) {
            green_stdout();
            printf("Team 1 wins\n");
            reset_stdout();
        }

        else if (team1.total_score < team2.total_score) {
            red_stdout();
            printf("Team 2 wins\n");
            reset_stdout();
        }

        else {
            blue_stdout();
            printf("It's a tie\n");
            reset_stdout();
        }

        // remove the FIFOs
        unlink(FIFO1);
        unlink(FIFO2);
        
        for (int i = 0; i < 2*PLAYERS_PER_TEAM; i++) {
            kill(process_pid[i], SIGKILL);
        }

        exit(0);
    }


}

int main(int argc, char *argv[]) {

#ifdef __GUI__

    gui_pid = fork();

    if (gui_pid == 0) {

        execlp("./ballpass", "ballpass.o", (const char*)NULL);
    }

    else if (gui_pid == -1) {
        perror("GUI failed.\n");
    }
    
    else {
        sleep(1);
    }


#endif 

    read_parameters(argc, argv);
    create_shared_mem();
    
    //create the FIFOs
    create_FIFOs();
    fork_children();
    init_teams();

    // Set up SIGCHLD handler
    sa_chld.sa_handler = signal_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("sigaction for SIGCHLD");
        exit(EXIT_FAILURE);
    }

    // Set up SIGIO handler
    sa_io.sa_handler = signal_handler;
    sigemptyset(&sa_io.sa_mask);
    sa_io.sa_flags = 0;
    if (sigaction(SIGIO, &sa_io, NULL) == -1) {
        perror("sigaction for SIGIO");
        exit(EXIT_FAILURE);
    }


    // add the alarm handler
    sa_alarm.sa_handler = alarm_handler;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;

    if (sigaction(SIGALRM, &sa_alarm, NULL) == -1) {
        perror("sigaction for SIGALRM");
        exit(EXIT_FAILURE);
    }


    sa_usr1.sa_handler = signal_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("sigaction for SIGUSR1");
        exit(EXIT_FAILURE);
    }

    sa_usr2.sa_handler = signal_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;

    if (sigaction(SIGUSR2, &sa_usr2, NULL) == -1) {
        perror("sigaction for SIGUSR2");
        exit(EXIT_FAILURE);
    }


    int fd = open(FIFO1, O_WRONLY);

    if (fd == -1) {
        perror("Error opening FIFO1");
        exit(-1);
    }
    //pasing 5 and 6 as edge cases
    int arr[2]  = {process_pid[5], process_pid[6]};


    if (write(fd, arr, sizeof(int) * 2) == -1) {
        perror("Error writing to FIFOR1");
        exit(-1);
    }

    close(fd);


    fd = open(FIFO2, O_WRONLY);

    if (fd == -1) {
        red_stderr();
        perror("Error opening FIFO2");
        reset_stderr();
        exit(-1);
    }

    if (write(fd, &process_pid[0], sizeof(pid_t)) == -1) {
        red_stderr();
        perror("Error writing to FIFO2");
        reset_stderr();
        exit(-1);
    }

    close(fd);

    //sending the signal to both team leads

   doOneRound();
    
    // wait for all children to finish
    for (int i = 0; i < 2*PLAYERS_PER_TEAM; i++) {
        waitpid(process_pid[i], &status, 0);
    }
    
    munmap(shared_mem, sizeof(struct shared_data));
    close(fd_shm);

    return 0;
}



/* FUNCTIONS */
void assign_initial_energy() {

    for (int i = 0; i < 2*PLAYERS_PER_TEAM; i++) {
        player_energy[i] =  MIN_PLAYER_ENERGY + (rand() % (abs(MAX_PLAYER_ENERGY - MIN_PLAYER_ENERGY)));
    }
}

void init_teams() {

    team1.number_of_balls = 0;
    team1.total_score = 0;
    team2.number_of_balls = 0;
    team2.total_score = 0;
}

void create_FIFOs()
{

    // CAUTION:
    // If keeping the FIFOs causes problems, we might need to remove the FIFOS if they exist
    // 
    //
    //
    // if the FIFO exists, no problem.
    if ((mkfifo(FIFO1, S_IFIFO | 0777)) == -1 && errno != EEXIST)
    {
        perror("Error Creating Fifo");
        exit(-1);
    }

    // if the FIFO exists, no problem.
    if ((mkfifo(FIFO2, S_IFIFO | 0777)) == -1 && errno != EEXIST)
    {
        perror("Error Creating Fifo");
        exit(-1);
    }
#ifdef __GUI__
    if ((mkfifo(GUI_FIFO, S_IFIFO | 0777)) == -1 && errno != EEXIST)
    {
        perror("Error Creating Fifo");
        exit(-1);
    }
#endif
}


void doOneRound() {
    
    shared_mem->ignore_signals = 0;

    // Start new round
    magenta_stdout();
    printf("Starting new round...\n");
    reset_stdout();

    team1.number_of_balls = 0;
    team2.number_of_balls = 0;

    current_round_number++;
    alarm(round_duration); // set the alarm for the round duration.

    // send a ball to team1 leader (send a signal to the team1 leader)
    team1.number_of_balls++;
    kill(process_pid[5], SIGUSR1);
    #ifdef __GUI__
    kill(gui_pid, SIGUSR1);
    int fd_1 = open(GUI_FIFO, O_WRONLY);
    
    if (fd_1 == -1) {
        perror("Error opening GUI FIFO");
        exit(-1);
    }

    if (write(fd_1, "-1#5", sizeof(char) * 5) == -1) {
        perror("Error writing to GUI FIFO");
        exit(-1);
    }
    
    close(fd_1);
    #endif

    /*
    kill(gui_pid, SIGUSR1);

    sleep(2);

    fd_1 = open(GUI_FIFO, O_WRONLY);
    if (fd_1 == -1) {
        perror("Error opening GUI FIFO");
        exit(-1);
    }

    if (write(fd_1, "5#0", sizeof(char) * 6) == -1) {
        perror("Error writing to GUI FIFO");
        exit(-1);
    }

    close(fd_1);
   */



    // send a ball to team2 leader (send a signal to the team2 leader)
    team2.number_of_balls++;
    kill(process_pid[11], SIGUSR1);

}

void fork_children(){

    pid_t team1_lead_pid;
    pid_t team2_lead_pid;
    pid_t next_player_pid;


    char team1_leader_pid_arg[10]="-1";
    char team2_leader_pid_arg[10]="-1";

    char next_player_pid_arg[10]="-1";

    char next_pid_arg[10] = "-1";


    for (i = 2*PLAYERS_PER_TEAM-1; i >= 0; i--) {

        //we will fork a child and execute the child program
        int current_pid;

        if ((current_pid = fork()) == -1) {
            perror("Fork failed.\n");
            exit(1);
        }

        char player_number_arg[4];
        char energy_arg[20];
        char next_player_arg[20]; 
        char gui_pid_arg[20];

        srand(time(NULL)); 
        assign_initial_energy();

        if (i == 5) {

            team1_lead_pid = current_pid;
            sprintf(team1_leader_pid_arg, "%d", team1_lead_pid);
        }

        else if (i == 11) { 

            team2_lead_pid = current_pid;
            sprintf(team2_leader_pid_arg, "%d", team2_lead_pid);
        }


        if (current_pid == 0) {
            sprintf(player_number_arg, "%d", i);

            sprintf(energy_arg, "%d", player_energy[i]);
            // this is unlikely to be incorrect, but feel free to triple check it, since it can cause serious damage.
            next_player = (i < PLAYERS_PER_TEAM) ? ((i+1) % PLAYERS_PER_TEAM) : 6 + ((i+1) % PLAYERS_PER_TEAM);
            
            sprintf(next_player_arg, "%d",next_player);
            sprintf(next_player_pid_arg, "%d", process_pid[next_player]);
        
            sprintf(gui_pid_arg, "%d", gui_pid);

            execlp("./child" ,"child.o",player_number_arg, energy_arg, next_player_arg, team1_leader_pid_arg, team2_leader_pid_arg,next_player_pid_arg, gui_pid_arg ,(const char*)NULL);

            perror("execvp failed.\n");
            
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
            process_pid[i] = current_pid;  
        }
    }    
}


void create_shared_mem() {

    // Create a shared memory segment
    fd_shm = shm_open("/my_shared_memory", O_CREAT | O_RDWR, 0666);
    if (fd_shm == -1) {
        red_stderr();
        perror("shm_open");
        reset_stderr();
        exit(EXIT_FAILURE);
    }

    // Set the size of the shared memory segment
    if (ftruncate(fd_shm, sizeof(struct shared_data)) == -1) {
        red_stderr();
        perror("ftruncate");
        reset_stderr();
        exit(EXIT_FAILURE);
    }

    // Map the shared memory segment into the address space
    shared_mem = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (shared_mem == MAP_FAILED) {
        red_stderr();
        perror("mmap");
        reset_stderr();
        exit(EXIT_FAILURE);
    }

    close(fd_shm);

}


void read_parameters(int argc,char* argv[]) {

    FILE *file;
    char line[100]; // Assuming the line won't exceed 100 characters

    // Open the file for reading
    file = fopen("settings.txt", "r");
    if (file == NULL) {
        red_stderr();
        perror("Error opening file");
        reset_stderr();
        return ;
    }

    // Read the line from the file
    fgets(line, sizeof(line), file);

    // Close the file
    fclose(file);

    // Use sscanf to extract the numbers
    sscanf(line, "round_duration=%d, number_of_rounds=%d", &round_duration, &number_of_rounds);

}


void fork_gui(){

    pid_t gui_pid;

    if ((gui_pid = fork()) == -1) {
        perror("Fork failed.\n");
        exit(1);
    }

    if (gui_pid == 0) {
        execlp("./ballPass", "ballPass.o", (const char*)NULL);
        perror("execvp failed.\n");
        exit(2);
    }
}
