#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


#define ENDPOINT "/dev/topics/prova/endpoint"


void publish(){

    int f1 = open(ENDPOINT, O_RDWR);
    char msg[30] = "ciao mondo\n";
    write(f1, &msg, 30);
    close(f1);
}

int main(){

    publish();
    return 0;

}
