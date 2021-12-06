#ifndef IP_LIST_H
#define IP_LIST_H

// A simple list structure that stores IP addresses (struct in_addr).

typedef struct IP_LIST {
    struct in_addr value;
    struct IP_LIST *next;
} ip_list;


ip_list *createIpList();
void destroyIpList(ip_list *node);
int insert(ip_list *root, struct in_addr x);
void printIpList(ip_list *root);


#endif /* IP_LIST_H */
