#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include "sockwrap.h"
#include "ip_list.h"

#define ICMP_HEADER_LEN 8
#define TIMEOUT 1000  /* Time of waiting for packets with last set TTL in miliseconds (= 1 second) */
#define TTL_LIMIT 30
#define REQUESTS_PER_TTL 3
#define BUFFER_SIZE 128

uint16_t in_cksum(uint16_t *addr, int len, int csum) {
    int sum = csum;

    while(len > 1)  {
        sum += *addr++;
        len -= 2;
    }

    if(len == 1) sum += htons(*(uint8_t *)addr << 8);

    sum = (sum >> 16) + (sum & 0xffff); 
    sum += (sum >> 16);        
    return ~sum; 
}

// Returns the difference between two times in miliseconds.
double timeDifference(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec)*1000.0 + (end.tv_usec - start.tv_usec)/1000.0;
}

/*
	struct sockaddr_in {
		short            sin_family;   // 因為是IPv4，設為AF_INET
		unsigned short   sin_port;     
		struct in_addr   sin_addr;     
		char             sin_zero[8];  // Not used, must be zero 
	};

	struct in_addr {
		unsigned long s_addr;          // load with inet_pton()
	};
*/

int main(int argc, char* argv[]) {
    
    if(argc != 3) { printf("Usage: ./prog hop-distance <IP_address>\n"); exit(1); }

	int hops = (int) strtol(argv[1], NULL, 10);

	if (hops > TTL_LIMIT) { printf("hop-distance can't be greater than 30\n"); exit(1);  }
    
	/* 檢查user給的ip address */
    struct sockaddr_in remoteAddr;
    bzero(&remoteAddr, sizeof(remoteAddr)); // 將remoteAddr涵蓋的bits全設為0
    remoteAddr.sin_family = AF_INET;
    Inet_pton(AF_INET, argv[2], &remoteAddr.sin_addr);  // 如果給的ip address是錯的，則會產生error message
              
    int pid = getpid();  // get process id for later identification
    
	/* 建立socket */
    int sockId = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);  // acquire socket, store socket's id
    struct timeval begin, current;
    begin.tv_sec = 0;
    begin.tv_usec = 1000;  // (= 1 ms)
    Setsockopt(sockId, SOL_SOCKET, SO_RCVTIMEO, &begin, sizeof(begin));  // 設定等待封包的時間
    
    char icmpRequestBuffer[BUFFER_SIZE], replyBuffer[BUFFER_SIZE];  // ICMP request 和 收到的IP封包

	/* 建立ICMP Request(Echo message) */
    struct icmp *icmpRequest = (struct icmp *) icmpRequestBuffer;
    icmpRequest->icmp_type = ICMP_ECHO;
    icmpRequest->icmp_code = htons(0);  // htons(x) returns the value of x in TCP/IP network byte order(little endian -> big endian)
    icmpRequest->icmp_id = htons(pid);  // 用process id當作icmp_id
    
    int ttl, sequence = 0, repliedPacketsCnt, i;
    bool stop = 0;  // set to true, when echo reply has been received
    double elapsedTime;  // variable used to compute the average time of responses
    struct timeval sendTime[REQUESTS_PER_TTL];  // send time of a specific packet
    ip_list *ipsThatReplied;  // list of IPs that replied to echo request
    
	/* 根據user給定的hop distance來做Expanding ring search(ERS) */
    for(ttl=1; ttl<=hops; ttl++) {
		repliedPacketsCnt = 0;
		elapsedTime = 0.0;
		ipsThatReplied = createIpList();

		for(i=1; i<=REQUESTS_PER_TTL; i++) {
			Setsockopt(sockId, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));  // set TTL of IP packet that is being sent
			icmpRequest->icmp_seq = htons(++sequence);  // set sequence number, for later identification
			icmpRequest->icmp_cksum = 0;
			icmpRequest->icmp_cksum = in_cksum((uint16_t*) icmpRequest, ICMP_HEADER_LEN, 0);
			
			gettimeofday(&sendTime[(sequence-1) % REQUESTS_PER_TTL], NULL);
			Sendto(sockId, icmpRequestBuffer, ICMP_HEADER_LEN, 0, &remoteAddr, sizeof(remoteAddr));
		}
	
		gettimeofday(&begin, NULL);  // get time after sending the packets
	
		while(repliedPacketsCnt < REQUESTS_PER_TTL) {
		  
			int RecvRetVal = Recvfrom(sockId, replyBuffer, BUFFER_SIZE, 0, 0, 0);  // wait 1 ms for a packet (at most)
			gettimeofday(&current, NULL);
			
			if(RecvRetVal < 0) {
				if(timeDifference(begin, current) > TIMEOUT) break;
				continue;
			}
			
			struct ip *reply = (struct ip *) replyBuffer;
			
			if(reply->ip_p != IPPROTO_ICMP) continue;  // 確認封包的protocol是否為ICMP
			
			struct icmp *icmpHeader = (struct icmp *) (replyBuffer + reply->ip_hl*4);  // 從封包取出ICMP header
			
			// 檢查ICMP的type
			if(icmpHeader->icmp_type != ICMP_ECHOREPLY && 
			  !(icmpHeader->icmp_type == ICMP_TIME_EXCEEDED && icmpHeader->icmp_code == ICMP_EXC_TTL)) continue;
			
			// 若ICMP的type為time_exceeded，shift icmpHeader to the copy of our request
			if(icmpHeader->icmp_type == ICMP_TIME_EXCEEDED)
			icmpHeader = (struct icmp *) (icmpHeader->icmp_data + ((struct ip *) (icmpHeader->icmp_data))->ip_hl*4);
			
			// is icmp_id equal to our pid and it's one of the latest (three) packets sent?
			if(ntohs(icmpHeader->icmp_id) != pid || sequence - ntohs(icmpHeader->icmp_seq) >= REQUESTS_PER_TTL) continue;
			
			elapsedTime += timeDifference(sendTime[(ntohs(icmpHeader->icmp_seq)-1) % REQUESTS_PER_TTL], current);
			insert(ipsThatReplied, reply->ip_src);
			repliedPacketsCnt++;
			
			if(icmpHeader->icmp_type == ICMP_ECHOREPLY) stop = 1;
		}
	
		// Prints router info
		printf("%2d. ", ttl);
		if(repliedPacketsCnt == 0) { printf("*\n"); continue; }
		printIpList(ipsThatReplied);
		destroyIpList(ipsThatReplied);
			
		if(repliedPacketsCnt == REQUESTS_PER_TTL) printf("%.1f ms\n", elapsedTime / repliedPacketsCnt);
		else printf("\t???\n");
	
		if(stop == 1) break;
    }
    return 0;
}
