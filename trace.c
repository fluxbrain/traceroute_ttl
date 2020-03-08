#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>


#define PORT "55678"        //Port fuer udp socket
#define DEST_PORT "33445"   //Port des Ziels
#define BUFFSIZE 56         //Buffersize fuer gesendetes und empfangenes Paket
#define MAXHOPS 20          //Maximale Hops bis Abbruch
#define TIMEOUT 3           //Timeout in Sekunden fuer recvfrom

int hop = 1;                            //Zaehler fuer Hops
char ipv4[INET_ADDRSTRLEN];             //IP-Adresse des Senders
char host[1024];                        //Hostname des Senders
int ttl = 1;                            //TTL
char service[20];                       //Service des Senders
char recvbuff[BUFFSIZE] = { 0 };        //Buffer empfangenes Paket
char msg[BUFFSIZE] = { 0 };             //Buffer fuer unser Paket
int s, t;

//Ausgabefunktion
void printout()
{
    printf("%2d -%16s - %s\n",hop,ipv4,host);
}


int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,"Error: Bitte Ziel (URL oder IP-Adresse) angeben!\n");
        return 1;
    }

    struct addrinfo myaddr, *res;
    int sock_udp, sock_raw;

    /*
    struct myaddr initialisieren und manuelle Optionen setzen
    wir beschraenken uns in diesem Programm generell auf IPv4
    */
    memset(&myaddr, 0, sizeof myaddr);
    myaddr.ai_family = AF_INET;
    myaddr.ai_socktype = SOCK_DGRAM;
    myaddr.ai_protocol = IPPROTO_UDP;
    myaddr.ai_flags = AI_PASSIVE;       //lässt eigene IP-Adresse automatisch einfügen

    if ((getaddrinfo(NULL, PORT, &myaddr, &res)) != 0) {
        fprintf(stderr, "Error: getaddrinfo myaddr\n");
        return 1;
    }

    sock_udp = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if ((bind(sock_udp, res->ai_addr, res->ai_addrlen)) == -1) {
		close(sock_udp);
		fprintf(stderr, "Error: bind\n");
		return 1;
	}

    freeaddrinfo(res);

	sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    struct timeval tv;
    tv.tv_sec = TIMEOUT;    //timeout in Sekunden (oben konstant definiert)
    tv.tv_usec = 0;         //+timeout in mikrosekunden
    if ((setsockopt(sock_raw, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval))) < 0) {
        fprintf(stderr, "error: setsockop SO_RCVTIMEO\n");
    }



    struct sockaddr_in sender_addr;
	struct addrinfo dest_hints, *dest_res;

    memset(&dest_hints, 0, sizeof dest_hints);

    if ((getaddrinfo(argv[1], DEST_PORT, &dest_hints, &dest_res)) != 0) {
        fprintf(stderr, "Error: getaddrinfo dest_hints\n");
        return 1;
    }

    char ipv4_target[INET_ADDRSTRLEN];
    struct sockaddr_in *ptr_target = (struct sockaddr_in *) dest_res->ai_addr;
    //printf("Ziel_ntoa: %s\n", inet_ntoa(ptr_target->sin_addr));
    inet_ntop(AF_INET, (struct in_addr *) &(ptr_target->sin_addr), ipv4_target, INET_ADDRSTRLEN);

    printf("traceroute to %s (%s), %d hops max, %d byte packets\n",argv[1], ipv4_target,MAXHOPS,BUFFSIZE);


    while (hop < MAXHOPS) {

        //setzen TTL am udp-socket
    	if ((setsockopt(sock_udp, IPPROTO_IP, IP_TTL, &ttl, sizeof (ttl))) < 0){
    		fprintf(stderr, "error: setsockopt\n");
    		return 1;
    	}


        //senden unser Paket
    	if ((sendto(sock_udp, msg, BUFFSIZE, 0, (struct sockaddr *) dest_res->ai_addr, sizeof(struct sockaddr))) == -1) {
    		fprintf(stderr, "Error: sendto\n");
    	}


        //empfangen ICMP-Nachrichten am rawsocket
        socklen_t len = sizeof (struct sockaddr_in);
        if (recvfrom(sock_raw, recvbuff, BUFFSIZE, 0, (struct sockaddr *) &sender_addr, &len) < 0) {
            fprintf(stderr, "%2d -       - timeout -\n" ,hop);
            hop++;  //falls timeout, wird Hop trotzdem gezählt
        }

        s = getnameinfo((struct sockaddr *) &sender_addr, sizeof sender_addr, ipv4, sizeof ipv4, service, sizeof service, NI_NUMERICSERV);
        if (s != 0) {
            fprintf(stderr, "numeric getnameinfo: %s\n", gai_strerror(s));
        }

        t = getnameinfo((struct sockaddr *) &sender_addr, sizeof sender_addr, host, sizeof host, service, sizeof service, 0);
        if (t != 0) {
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
        }



        struct udphdr *udp_hdr = (struct udphdr*) (recvbuff + 48);
        struct icmphdr *icmp_hdr = (struct icmphdr *) (recvbuff + 20);
        
        if (ntohs(udp_hdr->uh_sport) != strtol(PORT, (char **)NULL, 10)) {continue;}

        else if (icmp_hdr->type == 3 && icmp_hdr->code == 3) {
            printout();
            printf("Host reached with %d Hops!\n" ,hop);
            break;
        }

        else if (icmp_hdr->type == 11) {
            printout();
        }

        else {continue;}
        
        ttl++;  //TTL und Hop erhöhen für nächsten Durchlauf
        hop++;


    }   

    if (hop >= MAXHOPS) {
        printf("Maximum hops exceeded! :(\n");
    }

    freeaddrinfo(dest_res);

	return 0;

}

