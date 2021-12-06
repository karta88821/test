#ifndef __ICMP_H
#define __ICMP_H

#define ICMP_HEADER_LEN 8


uint16_t in_cksum(uint16_t *addr, int len, int csum);


#endif

