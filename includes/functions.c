#ifndef time_h
#include <time.h>
#endif

#ifndef errno_h
#include <errno.h>
#endif

#ifndef signal_h
#include <signal.h>
#endif

void my_pause(double seconds){
    struct timespec ts, rem;
    ts.tv_sec = (time_t) seconds;
    ts.tv_nsec = (long) ((seconds - ts.tv_sec) * 1e9);

    while (nanosleep(&ts, &rem) == -1 && errno == EINTR) {
        ts = rem;
    }
}


// using mode we choose either sa_handler or sa_sigaction
int set_handler(struct sigaction *sa, void (*sa_handler1)(int), void(*sa_sigaction1)(int, siginfo_t*, void* )  , int signum, int mode) {

    if (mode == 0) {
        sa->sa_handler = sa_handler1;
        sa->sa_flags = 0;
    }

    else {
        sa->sa_sigaction = sa_sigaction1;
        sa->sa_flags = SA_SIGINFO;
    }

    sigemptyset(&sa->sa_mask);

    return sigaction(signum, sa, NULL);
}
