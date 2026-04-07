/* vi: set sw=4 ts=4: */
/*
 * static_leases.c -- Couple of functions to assist with storing and
 * retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 */

#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "dhcpd.h"

/* Takes the address of the pointer to the static_leases linked list,
 *   Address to a 6 byte mac address
 *   Address to a 4 byte ip address */
int addStaticLease(struct static_lease_t **lease_struct, u_int8_t *mac, u_int32_t *ip)
{
	struct static_lease_t *cur;
	struct static_lease_t *new_static_lease;

	/* Build new node */
	new_static_lease = xmalloc(sizeof(struct static_lease_t));
	new_static_lease->mac = mac;
	new_static_lease->ip = ip;
	new_static_lease->next = NULL;

	/* If it's the first node to be added... */
	if (*lease_struct == NULL) {
		*lease_struct = new_static_lease;
	} else {
		cur = *lease_struct;
		while (cur->next) {
			cur = cur->next;
		}

		cur->next = new_static_lease;
	}

	return 1;
}

/* Check to see if a mac has an associated static lease */
u_int32_t getIpByMac(struct static_lease_t *lease_struct, void *arg)
{
	u_int32_t return_ip;
	struct static_lease_t *cur = lease_struct;
	u_int8_t *mac = arg;

	return_ip = 0;

	while (cur) {
		/* If the client has the correct mac  */
		if (memcmp(cur->mac, mac, 6) == 0) {
			return_ip = *(cur->ip);
		}

		cur = cur->next;
	}

	return return_ip;
}

/* Check to see if an ip is reserved as a static ip */
u_int32_t reservedIp(struct static_lease_t *lease_struct, u_int32_t ip)
{
	struct static_lease_t *cur = lease_struct;

	u_int32_t return_val = 0;

	while (cur) {
		/* If the client has the correct ip  */
		if (*cur->ip == ip)
			return_val = 1;

		cur = cur->next;
	}

	return return_val;
}

#if CONFIG_FEATURE_UDHCP_DEBUG 
/* Print out static leases just to check what's going on */
/* Takes the address of the pointer to the static_leases linked list */
void printStaticLeases(struct static_lease_t **arg)
{
	/* Get a pointer to the linked list */
	struct static_lease_t *cur = *arg;

	while(cur) {
		LOG("static lease: MAC:%02x:%02x:%02x:%02x:%02x:%02x  nip:%x",
			cur->mac[0], cur->mac[1], cur->mac[2], cur->mac[3], cur->mac[4], cur->mac[5], 
			*(cur->ip));
		cur = cur->next;
	}

}
#endif
