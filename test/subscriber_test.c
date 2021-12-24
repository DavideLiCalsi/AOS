#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>


#define SUBSCRIBE "/dev/topics/prova/subscribe"
#define SIGNAL "/dev/topics/prova/signal_nr"
#define SIGNUM 12

void handle_sig(int sig_num){

    printf("Received signal %d", sig_num);
}

void subscribe(){

    int f1 = open(SUBSCRIBE, O_RDWR);
    char msg[3] = "c";
    write(f1, &msg, 2);
    close(f1);

    int f2 = open(SIGNAL, O_RDWR);
    char sig_n = ;
    write(f2, &sig_n, 1);
    close(f2);
}

int main(){

    struct sig_action my_action;
    my_action.sa_handler = &handle_sig;
    my_action.sa_sigaction=NULL;
    my_action.sa_mask=0;
    my_action.sa_flags = 0;
    my_action.sa_restorer=NULL;

    subscribe();
    if ( sigaction(SIGNUM,&my_action, NULL) != 0) return;

    while (1){
        sleep(1);
    }

}
