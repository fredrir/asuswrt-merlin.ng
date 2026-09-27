#ifndef _QCA_VPN_DEFERRED_H_
#define _QCA_VPN_DEFERRED_H_

#include <net/if.h>
#include <netinet/in.h>

/* Raw Fusion/SDN indices are not explicit Merlin Director targets. */
int qca_vpn_binding_deferred(int index);
/* Validate the original SDN/default target before an atoi-converted zero or
 * out-of-range value can silently become WAN. Explicit valid zero stays WAN. */
int qca_vpn_sdn_deferred(int sdn_idx, int parsed_target);

struct qca_vpn_deferred_policy {
	int family, index;
	char src[INET6_ADDRSTRLEN + 4], dst[INET6_ADDRSTRLEN + 4];
	char iif[IFNAMSIZ];
};

/* Read original NVRAM selectors without legacy VPNC_DEV_POLICY truncation.
 * AF_UNSPEC means both families. Empty selectors mean all LAN ingress.
 * Returns 0 for a complete plan, 1 for unresolved records (valid scopes are
 * still delivered), or -1 for a resource/callback failure. */
int qca_vpn_deferred_foreach(int (*callback)(const struct qca_vpn_deferred_policy *, void *), void *arg);

/* Legacy source and interface routes are independent branches. A matching
 * enabled stock WAN branch must survive cleanup of ambiguous main lookups.
 * Returns 1 for a valid match, 0 for none, -1 for a resource failure. */
int qca_vpn_deferred_wan_override(const char *src, const char *iif);

#endif
