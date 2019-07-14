/*
	Simple udp client
*/
#include<stdio.h>	//printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>

//#define SERVER "127.0.0.1"
//#define SERVER "2003:cf:a738:a166:100:806b:3689:de81"
#define BUFLEN 512	//Max length of buffer
#define PORT 8888	//The port on which to send data

void die(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in6 si_other;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	char message[BUFLEN];

	if ( (s=socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin6_family = AF_INET6;
	si_other.sin6_port = htons(PORT);

	si_other.sin6_addr.s6_addr[0] = 0xfe;
	si_other.sin6_addr.s6_addr[1] = 0x80;
	si_other.sin6_addr.s6_addr[2] = 0x12;
	si_other.sin6_addr.s6_addr[3] = 0x34;
	si_other.sin6_addr.s6_addr[4] = 0;
	si_other.sin6_addr.s6_addr[5] = 0;
	si_other.sin6_addr.s6_addr[6] = 0;
	si_other.sin6_addr.s6_addr[7] = 0;

	si_other.sin6_addr.s6_addr[8] = 0;
	si_other.sin6_addr.s6_addr[9] = 10;
	si_other.sin6_addr.s6_addr[10] = 0;
	si_other.sin6_addr.s6_addr[11] = 10;
	si_other.sin6_addr.s6_addr[12] = 0xff;
	si_other.sin6_addr.s6_addr[13] = 0xcc;
	si_other.sin6_addr.s6_addr[14] = 0x88;
	si_other.sin6_addr.s6_addr[15] = 0x00;

	while(1)
	{
		//send the message
		const char* message = "THIS IS AN EMPTY STRING";
		if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
		{
			die("sendto()");
		}
		printf("SEND :)\n");
		sleep(1);
	}

	close(s);
	return 0;
}