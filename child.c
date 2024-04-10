#include "includes/include.h"
#include "includes/std.h"

unsigned energy;
unsigned player_number;
unsigned next_player_number;
#define A 5
#define K 2
pid_t pid_of_team1_leader;
pid_t pid_of_team2_leader;
pid_t next_player_pid;
double short_pause_duration();
bool player_drops_ball();
int gui_pid;

void send_ball_to_next_player();

int set_handler(struct sigaction *sa, void (*sa_handler1)(int), void(*sa_sigaction1)(int, siginfo_t*, void* )  , int signum, int mode);

struct sigaction sa_usr1, sa_usr2, sa_chld, sa_bus, sa_int;

int sig_bus_num = 0;

//bool next_round_started = true;

union sigval value;

int fd_shm;
// Define a structure to hold the flag
struct shared_data {
    int ignore_signals; // 0 means not ignore, 1 means ignore
};

struct shared_data *shared_mem;

void open_shared_mem();
void send_ball(int next_player_pid, int signum, int next_player_number); 

void signal_handler_usr1(int signum) {


    // only team leaders can receive SIGUSR1, this way they know they have to send the ball to the next player in the team
    // next team a team leader receives the ball, it receives the signal on SIGUSR2 and sends the ball to the other team leader
    if (signum == SIGUSR1) {

        if (shared_mem->ignore_signals == 1) {
            return;
        }

        if (player_number != 10 && player_number != 4) { //all players except player 10 and 4 


            energy = (energy > 20) ? energy - 5 : 20;
            send_ball(next_player_pid, SIGUSR1, next_player_number);
        }

        else {
            energy = (energy > 20) ? energy - 5 : 20;
            send_ball(next_player_pid, SIGUSR2, next_player_number);
        }

    }

    fflush(stdout);

}


void signal_handler_sigint () {

    // clean up
    munmap(shared_mem, sizeof(struct shared_data));
    close(fd_shm);

    // send sigint to the gui and child processes to terminate them
    usleep(20);
    exit(EXIT_SUCCESS);
}


void signal_handler_usr2 (int signum) {

    if (signum == SIGUSR2) {

        if (shared_mem->ignore_signals == 1) {
            return;
        }

        // only players 11 ,5  receive this signal (for now, testing purposes)
        if(player_number == 11){
            energy = (energy > 20) ? energy - 5 : 20;
            send_ball(pid_of_team1_leader, SIGUSR1, TEAM1_LEADER);
            kill(getppid(), SIGUSR1); // send a signal to the parent process to change count of balls
        }

        else { //team 1 leader
            energy = (energy > 20) ? energy - 5 : 20;
            send_ball(pid_of_team2_leader, SIGUSR1, TEAM2_LEADER);
            kill(getppid(), SIGUSR2); // send a signal to the parent process to change count of balls
        }
    }

    fflush(stdout);
}


void handler_bus(int signum, siginfo_t *info, void *context) {

    if (signum == SIGBUS) {
       
        if (sig_bus_num == 0) {
            sig_bus_num++;
            next_player_pid = info->si_value.sival_int;
        }

        else {
        // team1 leader pid 
        
            pid_of_team1_leader = info->si_value.sival_int;
        }
    }
}


int main(int argc, char* argv[]) {
    
    open_shared_mem();

    // TODO: if argc != numberOfArgs.. 

    srand(getpid());

    char* arguments=malloc(50 * sizeof(char));
    arguments[0] = '\0';
    for (int i = 1; i < argc; i++) {

        strcat(arguments, " ");
        strcat(arguments, argv[i]); 
    }

    char player_pid[10];
    sprintf(player_pid, "%d", getpid());
    strcat(arguments, " ");
    strcat(arguments, player_pid);
   //  printf("Arguments: %s\n", arguments);

    player_number = atoi(argv[1]);
    energy = atoi(argv[2]);
    next_player_number = atoi(argv[3]);
    pid_of_team1_leader = atoi(argv[4]);
    pid_of_team2_leader = atoi(argv[5]);
    next_player_pid = atoi(argv[6]); 
    gui_pid = atoi(argv[7]);


    // Set up SIGUSR1 handler

    if (set_handler(&sa_usr1, signal_handler_usr1, NULL, SIGUSR1, 0) == -1) {
        perror("sigaction for SIGUSR1");
        exit(EXIT_FAILURE);
    }

    // Set up SIGUSR2 handler
    if (set_handler(&sa_usr2, signal_handler_usr2, NULL, SIGUSR2, 0) == -1) {
        perror("sigaction for SIGUSR2");
        exit(EXIT_FAILURE);
    }

    if (set_handler(&sa_bus, NULL, handler_bus, SIGBUS, 1) == -1) {
        perror("sigaction for SIGBUS");
        exit(EXIT_FAILURE);
    }

    // Set up SIGINT handler
    if (set_handler(&sa_int,signal_handler_sigint, NULL, SIGINT, 0) == -1) {
        perror("sigaction for SIGINT");
        exit(EXIT_FAILURE);
    }

    while (1) {
        
        pause();
    }

    return 0;
}


double short_pause_duration() {
    //returns a short pause duration in milliseconds
    int random_constant = 17 + rand() % 4; // number between 17 and 20
    return 7500*((double)A / pow((double)(energy + random_constant), (double)K));
}


void open_shared_mem() {

    // Open the shared memory segment
    fd_shm = shm_open("/my_shared_memory", O_RDONLY, 0666);
    if (fd_shm == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    // Map the shared memory segment into the address space
    shared_mem = mmap(NULL, sizeof(struct shared_data), PROT_READ, MAP_SHARED, fd_shm, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    close(fd_shm);
}


void send_ball(int next_player_pid, int signum, int next_player_number) {

    my_pause(short_pause_duration());

    #ifdef __GUI__
    usleep(1000);

    
    if (player_number == 5 && next_player_number == 11) {
        value.sival_int = 511;
    }

    else if (player_number == 10 && next_player_number == 11) {
        value.sival_int = 1011;
    }

    else if (player_number == 9 && next_player_number == 10) {
        value.sival_int = 910;
    }

    else {

        value.sival_int = player_number * 10 + next_player_number;
    }

    usleep(1000);

    sigqueue(gui_pid, SIGUSR1, value);

    #endif

    printf("sending ball %d(%d) -> %d(%d), remaining energy is: %d\n", getpid(), player_number, next_player_pid, next_player_number,energy);
    fflush(stdout);
    if (player_drops_ball()) {
        red_stdout();
        printf("Player %d dropped the ball\n", player_number);
        printf("resending ball %d(%d) -> %d(%d), remaining energy is: %d\n", getpid(), player_number, next_player_pid, next_player_number,energy);
        reset_stdout();
        fflush(stdout);
        energy = (energy > 20) ? energy - 3 : 20;
        my_pause(short_pause_duration());
        kill(next_player_pid, signum);
    }

    else {
        kill(next_player_pid, signum);
    }
    
}

bool player_drops_ball() {
    int random_number = rand() % 100; // Generate a random number between 0 and 99
    // Let's say there's a 5% chance the player will drop the ball
    if (random_number < 5) {
        return true; // Player drops the ball
    } else {
        return false; // Player does not drop the ball
    }
}


