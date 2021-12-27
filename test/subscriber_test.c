#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>


#define SUBSCRIBE "/dev/topics/prova/subscribe"
#define SIGNAL "/dev/topics/prova/signal_nr"
#define SIGNUM 9

void handle_sig(int sig_num){

    printf("Received signal %d\n", sig_num);
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

    struct sigaction my_action;
    memset(&my_action, 0, sizeof(my_action));
    my_action.sa_handler = &handle_sig;


    subscribe();
    puts("Subscribed to topic\n");
    if ( sigaction(SIGNUM,&my_action, NULL) != 0) return -1;
    puts("Registered new action handler\n");

    while (1){
        sleep(1);
    }

    return 0;
}
