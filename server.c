
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#define USE_TCP 1

int main(int argc, char** argv)
{
	int s;
	int sAck;
	struct sockaddr_in serv_addr;
	struct sockaddr_in ack_addr;
	int slen;
	int	recv_len;
	int len = 2;
	uint8_t buf[1024] = {0x01, 0x02};
	uint8_t txbuf[256];
	int	count;
	int prevCount;
	int i;

	if (argc < 2) {
		printf("invalid args\n");
		return 1;
	}

	printf("Hello\n");	
	
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s<0) {
		printf("socket failed\n");
		return 1;
	}

	sAck = socket(AF_INET, SOCK_STREAM, 0);
	if (s<0) {
		printf("socket failed\n");
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5000); 

	ack_addr.sin_family = AF_INET;
	ack_addr.sin_port = htons(5002); 

	if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
	{
	        printf("inet_pton error occured\n");
        	return 1;
	}

	if(inet_pton(AF_INET, argv[1], &ack_addr.sin_addr)<=0)
	{
	        printf("inet_pton error occured\n");
        	return 1;
	}

	if (connect(sAck, (struct sockaddr*)&ack_addr, sizeof(ack_addr)) < 0) {
		printf("connect error\n");
		return 1;
	}

	if (sendto(s, buf, len, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
	{
		printf("sendto()\n");
		return 1;
	}

	while (1) {
		recv_len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *) &serv_addr, &slen);
		if (recv_len < 1)
		{
			printf("recvfrom()\n");
			return 1;
		}

		sscanf(buf, "%d", &count);
		printf("recv_len=%d, cout=%d\n", recv_len, count);

		if (count - prevCount > 1) {
#if USE_TCP
			txbuf[0] = 0x55;
			txbuf[1] = 0x70;
			txbuf[2] = 0x05;
			txbuf[3] = (count) & 0xff;
			txbuf[4] = (count>>8) & 0xff;
			txbuf[5] = (count>>16) & 0xff;
			txbuf[6] = (count>>24) & 0xff;
			txbuf[7] = count - prevCount;

			printf("sending ack\n");
			send(sAck, txbuf, 8, 0);
#else
			txbuf[0] = 0x70;
			txbuf[1] = (count) & 0xff;
			txbuf[2] = (count>>8) & 0xff;
			txbuf[3] = (count>>16) & 0xff;
			txbuf[4] = (count>>24) & 0xff;

			for (i=0; i<2; i++) {
				if (sendto(s, txbuf, 5, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
				{
					printf("err sendto()\n");
				}
			}
#endif		
		}
		prevCount = count;
	}
	
	return 0;

}

