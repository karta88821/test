#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include "ip_list.h"

#define IP_LIST_ERROR(str) { fprintf(stderr, "%s\n", str); exit(1); }


// Returns a pointer to a newly created list.
ip_list *createIpList() {
    ip_list *root = (ip_list *) malloc(sizeof(ip_list));
    if(root == NULL) IP_LIST_ERROR("Malloc error: Could not create an IP address list.");
    root->next = NULL;
    return root;
}


// Frees the list recursively
void destroyIpList(ip_list *node) {
    if(node == NULL) return;
    destroyIpList(node->next);
    free(node);
}


// Inserts an IP address right after the given node
void addAfterTheNode(ip_list *preceding, struct in_addr x) {
    ip_list *newNode = (ip_list *) malloc(sizeof(ip_list));
    if(newNode == NULL) IP_LIST_ERROR("Malloc error: Could not add an IP address to the list.");
    newNode->value = x;
    newNode->next = preceding->next;
    preceding->next = newNode;
}
  

// Inserts in_addr x into the ordered list
int insert(ip_list *root, struct in_addr x) {
    ip_list *cur = root, *tmp;
    
    while(cur->next != NULL) {
		tmp = cur->next;
		if(x.s_addr == tmp->value.s_addr) return 0;
		if(x.s_addr < tmp->value.s_addr) break;
		cur = tmp;
    }
    addAfterTheNode(cur, x);
    return 1;
}


// Prints the list
void printIpList(ip_list *root) {
    ip_list *cur = root->next;
    
    while(cur != NULL) {
		printf("%-16s", inet_ntoa(cur->value));
		cur = cur->next;
    }
}
