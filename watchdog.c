
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <unistd.h> // read(), write(), close()
#define PORT 3000
int openTCP(){
 int mySocket, clientSocket; 
    struct sockaddr_in serverAddress, clientAddress;
    mySocket = socket(AF_INET,SOCK_STREAM,0);// crate socket
    if(mySocket < 0 ){ // check if socket crated succeed
        printf("Socket failed\n");
        exit(0);
    }
    bzero(&serverAddress,sizeof(serverAddress)); //reset server address
    serverAddress.sin_family = AF_INET; // Address family, AF_INET unsigned 
    serverAddress.sin_port = htons(PORT); // Port number 
    serverAddress.sin_addr.s_addr =  INADDR_ANY;//ip address
    int yes=1; 
    if (setsockopt(mySocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) // set socket
    { 
    perror("setsockopt\n"); 
	exit(0);
    }
    if(bind( mySocket,(struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) // be ready to connection
    {
        fprintf(stderr, "Error on bind --> %s", strerror(errno));
        exit(0);
    }


    if((listen(mySocket, 1)) != 0){ // listen to connection
        fprintf(stderr, "Error on listen --> %s", strerror(errno));
        exit(0);
    }
    socklen_t len =sizeof(clientAddress); // get size of client 
    clientSocket = accept(mySocket,(struct sockaddr *)&clientAddress, &len); // save connection with client
    if( clientSocket < 0){ // chaeck if accept succeed
        printf("accept failed/n");
        exit(0);
    }
    else printf("watchdog connection created\n");
    return clientSocket;

}
int main()
{
    char oksend[16];
    char okrecv[16];
    int stop = 0;
    int pingSocket = openTCP();
    while(1){
        recv(pingSocket,okrecv,16,0);
        time_t start;
        pid_t timer1 = fork();
        stop = 0;
            if (timer1 == 0)
            {
            start = clock();
            while(((clock()-start)/CLOCKS_PER_SEC) <10 ){}
            sprintf(oksend,"%d",1);
            send(pingSocket,oksend,16,0);
            kill(getppid(), SIGKILL);   
        }
        else {
            int len = recv(pingSocket,okrecv,16,0);
            if(len==-1){
                printf("recive send failed\n");
            }
            sprintf(oksend,"%d",2);
            send(pingSocket,oksend,16,0);
            stop = 1;
            kill(timer1,SIGKILL);
        }
        wait(0);
        if(stop ==0){
            recv(pingSocket,okrecv,16,0);
            if(atoi(okrecv) == 1){
                close(pingSocket);
                break;

            }
        }
    }
    
    return 1;
    }
