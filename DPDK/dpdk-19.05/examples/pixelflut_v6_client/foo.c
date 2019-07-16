#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#define PORT 1234	//The port on which to send data

const char* message = "";

void colorPixel(int x, int y, uint32_t rgb)
{
	struct sockaddr_in6 si_other;
	int s, i, slen=sizeof(si_other);

	if ( (s=socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		printf("Can not open socket\n");
		exit(1);
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin6_family = AF_INET6;
	si_other.sin6_port = htons(PORT);

	si_other.sin6_addr.s6_addr[0] = 0x40;
	si_other.sin6_addr.s6_addr[1] = 0x00;
	si_other.sin6_addr.s6_addr[2] = 0x00;
	si_other.sin6_addr.s6_addr[3] = 0x42;
	si_other.sin6_addr.s6_addr[4] = 0;
	si_other.sin6_addr.s6_addr[5] = 0;
	si_other.sin6_addr.s6_addr[6] = 0;
	si_other.sin6_addr.s6_addr[7] = 0;

	si_other.sin6_addr.s6_addr[8] = x >> 8;
	si_other.sin6_addr.s6_addr[9] = x;
	si_other.sin6_addr.s6_addr[10] = y >> 8;
	si_other.sin6_addr.s6_addr[11] = y;

	si_other.sin6_addr.s6_addr[12] = rgb >> 24;
	si_other.sin6_addr.s6_addr[13] = rgb >> 16;
	si_other.sin6_addr.s6_addr[14] = rgb >> 8;
	si_other.sin6_addr.s6_addr[15] = rgb;

	if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1) {
		printf("Can not send packet\n");
		exit(1);
	}
	close(s);
}

int main(void)
{
	while(1) {
		//srand(time(NULL));

		int startX = rand() % 700;
		int startY = rand() % 500;
		uint32_t rgb = rand();

		for (int x = startX; x < startX + 50; x++) {
			for (int y = startY; y < startY + 50; y++) {
				colorPixel(x, y, rgb);
			}
			//sleep(1);
		}
	}

	return 0;
}