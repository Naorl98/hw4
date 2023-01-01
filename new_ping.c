// icmp.cpp
// Robert Iakobashvili for Ariel uni, license BSD/MIT/Apache
//
// Sending ICMP Echo Requests using Raw-sockets.
//
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <resolv.h>
// IPv4 header len without options
#define IP4_HDRLEN 20
// ICMP header len for echo req
#define ICMP_HDRLEN 8
#define PORT 3000
int timeCount = 0;
int pacrec = 0;  // packet recived
int pacsend = 0; // packet sends
float mintime = 99999999; // min time
float maxtime = 0; // max time
float total = 0; // total time
int watch ,sock;

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

void finish(int);


void finish(int sig)
{

    signal(sig, SIG_IGN);
    float avr = total / (float)timeCount;
    int pacloss = 100-((pacrec*100)/pacsend);
    printf("\n");
    printf("%d packets transmitted, %d received, %d%c packet loss, time %fms\n"
    , pacrec, pacsend, pacloss, '%', total);
    printf("rtt min / avg / max  = %.3f / %.3f / %.3f ms\n", mintime, avr, maxtime);
        // Close the raw socket descriptor.
    close(watch);
    close(sock);
    exit(0);
}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

int header(char packet[IP_MAXPACKET], struct icmp *icmphdr)
{
    // char packetdata[IP_MAXPACKET] = *packet;
    char data[IP_MAXPACKET] = "This is the ping.\n";
    int datalen = strlen(data) + 1;
    (*icmphdr).icmp_type = ICMP_ECHO;
    (*icmphdr).icmp_code = 0;
    (*icmphdr).icmp_seq += 1;
    (*icmphdr).icmp_id = 18;
    (*icmphdr).icmp_cksum = 0;
    memcpy((packet), icmphdr, ICMP_HDRLEN);
    memcpy(packet + ICMP_HDRLEN, data, datalen);
    (*icmphdr).icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), icmphdr, ICMP_HDRLEN);

    return datalen;
}

void ping(int sock, struct sockaddr_in *dest_in, char packet[IP_MAXPACKET], int datalen)
{

    int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)dest_in, sizeof((*dest_in)));
    pacsend++;
    if (bytes_sent == -1)
    {
        fprintf(stderr, "Send packet failes, eror: %d", errno);
        exit(0);
    }
}
ssize_t listener(int sock, struct sockaddr_in *dest_in, char packet[IP_MAXPACKET])
{
    bzero(packet, IP_MAXPACKET);
    socklen_t len = sizeof(*dest_in);
    ssize_t bytes_received = -1;
    while ((bytes_received = recvfrom(sock, packet, IP_MAXPACKET, 0, (struct sockaddr *)dest_in, &len)))
    {
        if (bytes_received > 0)
        {

            pacrec++;
            return bytes_received;
        }
    }
    return 0;
}
int openTcp(){

    int my_Socket; // the socket

    struct sockaddr_in client_Address;
    memset(&client_Address,0,sizeof(client_Address));//reset client
    client_Address.sin_family = AF_INET; // Address family, AF_INET unsigned 
    client_Address.sin_port = htons(PORT); // Port number 

    my_Socket = socket(AF_INET,SOCK_STREAM,0);//create socket
    if(my_Socket == -1){// check if create succeed
        printf("Socket creation failed\n");
        exit(0);
    }
    while(1){
    int con = connect(my_Socket, (struct sockaddr *)&client_Address, sizeof(client_Address));// connect to server
    if(con != -1){// check if connect succeed
        break;
    }
    }
    return my_Socket;
}

int main(int count, char *argv[])
{
    if (count != 2)
    {
        printf("usage: sudo ./parta <hostname>\n");
        exit(0);
    }
    int datalen = 0;
    struct icmp icmphdr; // ICMP-header
    icmphdr.icmp_seq = 0;
    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;
    dest_in.sin_addr.s_addr = inet_addr(argv[1]);
    sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "Failed to create socket: %d", errno);
        return -1;
    }
    char ok[16]; //  message to watchdog
    char what[16]; //  message from watchdog

    sprintf(ok,"%s",argv[1]);
    signal(SIGINT, finish);
    int pid = fork();
    if (pid == 0)
    {
        char *args[2];
        // compiled watchdog.c by makefile
        args[0] = "./watchdog";
        args[1] = NULL;
        execvp(args[0], args);
        exit(1);
    }
    else{
        watch = openTcp();
        while (1)
    {
        char packet[IP_MAXPACKET] = {};
        bzero(packet, IP_MAXPACKET);
        datalen = header(packet, &icmphdr);
        struct timeval start, end;
        gettimeofday(&start, 0);
        ping(sock, &dest_in, packet, datalen);
        if(send(watch,ok,16,0)==-1) break;;
        if(timeCount == 7) sleep(15);
        int bytes_received = listener(sock, &dest_in, packet);
        gettimeofday(&end, 0);
        if(send(watch,ok,16,0)==-1) break;
        recv(watch,what,16,0);
        if(atoi(what)==1)  {
            printf("server <%s> cannot be reached.\n", argv[1]);
            pacrec -= 1;
            sprintf(ok,"%d",1);
            send(watch,ok,16,0);
            close(watch);
            break;
        }
        struct iphdr *iphdr = (struct iphdr *)packet;
        struct icmp *icmpheader = (struct icmp *)(packet + (iphdr->ihl * 4));
        char reply[IP_MAXPACKET];
        memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);
        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n", bytes_received, argv[1],
               (*icmpheader).icmp_seq, iphdr->ttl, milliseconds);
        timeCount++;
        total += milliseconds;
        mintime = MIN(mintime, milliseconds);
        maxtime = MAX(maxtime, milliseconds);
        sleep(1);
       
    }  
    wait(0);
    float avr = total / (float)timeCount;
    int pacloss = 100-((pacrec*100)/pacsend);
    printf("\n");
    printf("%d packets transmitted, %d received, %d%c packet loss, time %fms\n"
    ,pacsend, pacrec, pacloss, '%', total);
    printf("rtt min / avg / max  = %.3f / %.3f / %.3f ms\n", mintime, avr, maxtime);
    // Close the raw socket descriptor.

    close(sock);
    exit(0); 
    
    }
    return 0;
}
