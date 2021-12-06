#include <netinet/ip_icmp.h>
#include "icmp.h"


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
