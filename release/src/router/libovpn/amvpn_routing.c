/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

/*
 * VPN utility library for Asuswrt-Merlin
 * Provides some of the functions found in Asuswrt's
 * proprietary libvpn, either re-implemented, or
 * implemented as wrappers around AM's functions.
 * Also includes additional functions developed
 * for Asuswrt-Merlin.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <shutils.h>
#include <shared.h>
#ifdef RTCONFIG_SOC_IPQ53XX
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#endif
#include "openvpn_config.h"
#include "openvpn_control.h"
#include "openvpn_setup.h"
#include "amvpn_routing.h"


// Remove all rules pointing to a specific client table
// If unit is 0, then remove rules targetting main (i.e. WAN)
void amvpn_clear_routing_rules(int unit, vpndir_proto_t proto) {
	FILE *fp;
	char buffer[128], buffer2[128], buffer3[128];
	int prio, verb = 3;
	char table[12], *lookup;
	char target_table[12];

	if (proto == VPNDIR_PROTO_OPENVPN) {
		snprintf(buffer, sizeof (buffer), "vpn_client%d_verb", unit);
		verb = nvram_get_int(buffer);
#ifdef RTCONFIG_WIREGUARD
	} else if (proto == VPNDIR_PROTO_WIREGUARD) {
		verb = 3;
#endif
	} else if (unit != 0){
		return;
	}

	snprintf(buffer, sizeof (buffer), "/usr/sbin/ip rule show > /tmp/vpnrules%d_tmp", unit);
	system(buffer);

	snprintf(buffer, sizeof (buffer), "/tmp/vpnrules%d_tmp", unit);
	fp = fopen(buffer, "r");
	if (fp) {
		if (unit == 0)
			strlcpy(target_table, "main", sizeof (target_table));
		else if (proto == VPNDIR_PROTO_OPENVPN)
			snprintf(target_table, sizeof (target_table), "ovpnc%d", unit);
#ifdef RTCONFIG_WIREGUARD
		else if (proto == VPNDIR_PROTO_WIREGUARD)
			snprintf(target_table, sizeof (target_table), "wgc%d", unit);
#endif
		while (fgets(buffer2, sizeof(buffer2), fp) != NULL) {
			if (buffer2[strlen(buffer2)-1] == '\n')
				buffer2[strlen(buffer2)-1] = '\0';

			if (sscanf(buffer2, "%u", &prio) != 1)
				continue;

			// Only remove rules within our official range
			// max = base + 10 all + (200 wan) + (5 * 200 ovpn) + (5 * 200 wg)
			if ((prio < VPNDIR_PRIO_BASE) ||
			     (prio > (VPNDIR_PRIO_BASE + (OVPN_CLIENT_MAX + WG_CLIENT_MAX) + VPNDIR_PRIO_MAX_RULES + (OVPN_CLIENT_MAX * VPNDIR_PRIO_MAX_RULES) + (WG_CLIENT_MAX * VPNDIR_PRIO_MAX_RULES))))
				continue;

			if ((lookup = strstr(buffer2, "lookup")) == NULL)
				continue;

			if (sscanf(lookup, "lookup %11s", table) != 1)
				continue;
			if (strcmp(table, target_table))
				continue;

			snprintf(buffer3, sizeof (buffer3), "/usr/sbin/ip rule del prio %d", prio);
			if (verb >= 6)
				logmessage("vpndirector", "Removed rule %d", prio);
			system(buffer3);
		}
		fclose(fp);
	}
	unlink(buffer);

#if defined(RTCONFIG_WIREGUARD) && (defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X)) || defined(RTCONFIG_HND_ROUTER_BE_4916)
	// Remove all bypass for this unit
	if (proto == VPNDIR_PROTO_WIREGUARD) {
		_amvpn_apply_wg_bypass(unit, 0);
	}
#endif
}

#if defined(RTCONFIG_WIREGUARD) && (defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X)) || defined(RTCONFIG_HND_ROUTER_BE_4916)
/* Add or remove WG bypass rules for a specific unit */
void _amvpn_apply_wg_bypass(int unit, int add) {
	char buffer[32], buffer2[128];
	FILE *fp;

	snprintf(buffer, sizeof (buffer), "/etc/wg/vpndirector%d", unit);
	fp = fopen(buffer, "r");
	if (fp) {
		while (fgets(buffer2, sizeof(buffer2), fp) != NULL) {
			if (buffer2[strlen(buffer2)-1] == '\n')
				buffer2[strlen(buffer2)-1] = '\0';
			if (!strcmp(buffer2, "LAN"))
				hnd_skip_wg_all_lan(add);
			else
				hnd_skip_wg_network(add, buffer2);
		}
		fclose(fp);
	}

	if (!add)
		unlink(buffer);
}

/* Re-apply all WGC bypass rules (following a removal) in case we have overlaps */
void amvpn_refresh_wg_bypass_rules() {
	int unit;
	char buffer[32];

	for (unit = 1; unit <= WG_CLIENT_MAX; unit++ ) {
		sprintf(buffer, "wgc%d_enable", unit);
		if (nvram_get_int(buffer)) {
			_amvpn_apply_wg_bypass(unit, 1);
		}
	}
}
#endif


/*
	Rule priority allocations:

	VPNDIR_PRIO_ALL
	- 10000-10009: clients set to OVPN_RGW_ALL

	VPNDIR_PRIO_WAN
	- 10010-10209: WAN rules


	VPNDIR_PRIO_OPENVPN
	- 10210-10409: OVPN 1
	- 10410-10609: OVPN 2
	- 10610-10809: OVPN 3
	- 10810-11009: OVPN 4
	- 11010-11209: OVPN 5

	VPNDIR_PRIO_WIREGUARD
	- 11210-11409: WGC 1
	- 11410-11609: WGC 2
	- 11610-11809: WGC 3
	- 11810-12009: WGC 4
	- 12010-12209: WGC 5

	VPNDIR_PRIO_KS_OPENVPN
	- 12210-12214 (OVPN1 through 5, multiple rules per unit)

	VPNDIR_PRIO_KS_WIREGUARD
	- 12215-12219 (WG1 through 5, multiple rules per unit)

	VPNDIR_PRIO_KS_SDN
	- 122220
*/
void amvpn_set_wan_routing_rules() {
	char buffer[8000];

	amvpn_clear_routing_rules(0, VPNDIR_PROTO_NONE);
	amvpn_get_policy_rules(0, buffer, sizeof (buffer), VPNDIR_PROTO_NONE);
	_write_routing_rules(0, buffer, 3, VPNDIR_PROTO_NONE);
}



void amvpn_set_routing_rules(int unit, vpndir_proto_t proto) {
	char prefix[32], buffer[8000];
	int rgw, state, verb;
#if 0	//#ifdef RTCONFIG_MULTILAN_CFG
	int i;
	MTLAN_T *pmtl = NULL;
	size_t mtl_sz = 0;
#endif

	if (unit < 1)
		return;

	if (proto == VPNDIR_PROTO_OPENVPN && unit <= OVPN_CLIENT_MAX) {
		snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
		verb = nvram_pf_get_int(prefix, "verb");
		rgw = nvram_pf_get_int(prefix, "rgw");
#ifdef RTCONFIG_WIREGUARD
	} else if (proto == VPNDIR_PROTO_WIREGUARD && unit <= WG_CLIENT_MAX) {
		snprintf(prefix, sizeof(prefix), "wgc%d_", unit);
		verb = 3;
		rgw = OVPN_RGW_POLICY;
#endif
	} else {
		return;
	}

	amvpn_clear_routing_rules(unit, proto);

	switch (rgw) {
		case OVPN_RGW_NONE:
		case OVPN_RGW_ALL:
			// Set client rules if running or currently connecting
			state = get_ovpn_status(OVPN_TYPE_CLIENT, unit);
			if (state == OVPN_STS_RUNNING || state == OVPN_STS_INIT) {
//				snprintf(buffer, sizeof (buffer), "/usr/sbin/ip rule add table ovpnc%d priority %d iif %s", unit, VPNDIR_PRIO_ALL + unit, nvram_safe_get("lan_ifname"));
				snprintf(buffer, sizeof (buffer), "/usr/sbin/ip rule add table ovpnc%d priority %d", unit, VPNDIR_PRIO_ALL + unit);
				system(buffer);
				if (verb >= 3)
					logmessage("vpndirector","Routing all traffic through ovpnc%d", unit);
#ifdef RTCONFIG_MULTILAN_CFG
#if 0	// redirect everything, not just by interface - rely on main table copied to vpn table
				pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
				if (pmtl) {
					get_mtlan(pmtl, &mtl_sz);
					// Skip first SDN (Default)
					for (i = 1; i < mtl_sz; ++i) {
						if (pmtl[i].sdn_t.vpnc_idx == 0) {	// If not associated with a VPN, route it
							snprintf(buffer, sizeof (buffer), "/usr/sbin/ip rule add table ovpnc%d priority %d iif %s", unit, VPNDIR_PRIO_ALL + unit, pmtl[i].nw_t.ifname);
							system(buffer);
							if (verb > 3)
								logmessage("vpndirector", "Routing all traffic for SDN %s through ovpnc%d", pmtl[i].nw_t.ifname, unit);
						}
					}
					FREE_MTLAN((void *)pmtl);
				}
#endif
#endif

			}
			break;

		case OVPN_RGW_POLICY:
			amvpn_get_policy_rules(unit, buffer, sizeof (buffer), proto);
			_write_routing_rules(unit, buffer, verb, proto);
			break;
	}
}


void _write_routing_rules(int unit, char *rules, int verb, vpndir_proto_t proto) {
	char *buffer_tmp, *buffer_tmp2, *rule;
	char buffer[128], table[16];
	int ruleprio, vpnprio, wanprio;
	char *enable, *desc, *target, *src, *dst;
	char srcstr[64], dststr[64];
#if defined(RTCONFIG_WIREGUARD) && (defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X)) || defined(RTCONFIG_HND_ROUTER_BE_4916)
	int ret;
	char bypass_filename[64];
#endif

	wanprio = VPNDIR_PRIO_WAN;

	if (proto == VPNDIR_PROTO_OPENVPN)
		vpnprio = VPNDIR_PRIO_OPENVPN + (VPNDIR_PRIO_MAX_RULES * (unit-1));
#ifdef RTCONFIG_WIREGUARD
	else if (proto == VPNDIR_PROTO_WIREGUARD)
		vpnprio = VPNDIR_PRIO_WIREGUARD + (VPNDIR_PRIO_MAX_RULES * (unit-1));
#endif
	else if (proto == VPNDIR_PROTO_NONE)
		vpnprio = 0;	// Unused
	else
		return;

	buffer_tmp = buffer_tmp2 = strdup(rules);

	while (buffer_tmp && (rule = strsep(&buffer_tmp2, "<")) != NULL) {
		if((vstrsep(rule, ">", &enable, &desc, &src, &dst, &target)) != 5)
			 continue;

		if (!atoi(&enable[0]))
			continue;

		if (!strcmp(target,"WAN")) {
			strcpy(table, "main");
			ruleprio = wanprio++;
		}
		else if (!strncmp(target, "OVPN", 4) && proto == VPNDIR_PROTO_OPENVPN) {
			snprintf(table, sizeof (table), "ovpnc%d", unit);
			ruleprio = vpnprio++;
		}
#ifdef RTCONFIG_WIREGUARD
		else if (!strncmp(target, "WGC", 3) && proto == VPNDIR_PROTO_WIREGUARD) {
			snprintf(table, sizeof (table), "wgc%d", unit);
			ruleprio = vpnprio++;
		}
#endif

		else
			continue;

		if (*src && strcmp(src, "0.0.0.0")) {
			snprintf(srcstr, sizeof (srcstr), "from %s", src);
#if defined(RTCONFIG_WIREGUARD) && (defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X)) || defined(RTCONFIG_HND_ROUTER_BE_4916)
			if ((proto == VPNDIR_PROTO_WIREGUARD) && (!strncmp(target,"WGC", 3))) {
				snprintf(bypass_filename, sizeof (bypass_filename), "/etc/wg/vpndirector%d", unit);

				if (strchr(src, '/'))
					snprintf(buffer, sizeof(buffer), "%s\n", src);
				else {
					ret = is_valid_ip(src);
					if (ret > 1)
						snprintf(buffer, sizeof(buffer), "%s/128\n", src);
					else if (ret > 0)
						snprintf(buffer, sizeof(buffer), "%s/32\n", src);
				}
				f_write_string(bypass_filename, buffer, FW_APPEND, 0);
				sprintf(buffer, "wgc%d_enable", unit);
				if (nvram_get_int(buffer))
					hnd_skip_wg_network(1, src);
			}
#endif
		}
		else {
			*srcstr = '\0';
#if defined(RTCONFIG_WIREGUARD) && (defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X)) || defined(RTCONFIG_HND_ROUTER_BE_4916)
			if ((proto == VPNDIR_PROTO_WIREGUARD) && (!strncmp(target,"WGC", 3))) {
				snprintf(bypass_filename, sizeof (bypass_filename), "/etc/wg/vpndirector%d", unit);
				f_write_string(bypass_filename, "LAN\n", FW_APPEND, 0);
				sprintf(buffer, "wgc%d_enable", unit);
				if (nvram_get_int(buffer))
					hnd_skip_wg_all_lan(1);
			}
#endif
		}

		if (*dst && strcmp(dst, "0.0.0.0"))
			snprintf(dststr, sizeof (dststr), "to %s", dst);
		else
			*dststr = '\0';

		snprintf(buffer, sizeof (buffer), "/usr/sbin/ip rule add %s %s table %s priority %d",
				                                   srcstr, dststr, table, ruleprio);

		if (verb >= 3)
			logmessage("vpndirector","Routing %s from %s to %s through %s", desc, (*src ? src : "any"), (*dst ? dst : "any"), table);

		system(buffer);
	}
	free(buffer_tmp);
}

inline void _flush_routing_cache() {
        system("/usr/sbin/ip route flush cache");
}


/* Remove/add server routes from all client routing tables when stopping/starting a client */

void update_client_routes(char *server_iface, int addroute) {
	int unit;
	char buffer[32];

	for (unit = 1; unit <= OVPN_CLIENT_MAX; unit++ ) {
		sprintf(buffer, "vpnclient%d", unit);
		if ( pidof(buffer) >= 0 ) {
			if (addroute)
				_add_server_routes(server_iface, unit, VPNDIR_PROTO_OPENVPN);
			else
				_del_server_routes(server_iface, unit, VPNDIR_PROTO_OPENVPN);
		}
	}

#ifdef RTCONFIG_WIREGUARD
	for (unit = 1; unit <= WG_CLIENT_MAX; unit++ ) {
		sprintf(buffer, "wgc%d_enable", unit);
		if (nvram_get_int(buffer)) {
			if (addroute)
				_add_server_routes(server_iface, unit, VPNDIR_PROTO_WIREGUARD);
			else
				_del_server_routes(server_iface, unit, VPNDIR_PROTO_WIREGUARD);
		}
	}
#endif
}


/* Add / remove OpenVPN server routes from client tables */
/* Server-agnostic, could eventually be reused for other servers like WG/IPSEC */

void _add_server_routes(char *server_iface, int client_unit, vpndir_proto_t proto) {
	char buffer[128], routecmd[128], line[128], table[8];
	FILE *fp_route;

	if (proto == VPNDIR_PROTO_OPENVPN)
		snprintf(table, sizeof (table), "ovpnc%d", client_unit);
#ifdef RTCONFIG_WIREGUARD
	else if (proto == VPNDIR_PROTO_WIREGUARD)
		snprintf(table, sizeof (table), "wgc%d", client_unit);
#endif
	else
		return;

	snprintf(buffer, sizeof (buffer), "/usr/sbin/ip route list table main | grep %s > /tmp/vpnroute_%s_tmp", server_iface, table);
	system(buffer);

	snprintf(buffer, sizeof (buffer), "/tmp/vpnroute_%s_tmp", table);
	fp_route = fopen(buffer, "r");

	if (fp_route) {
		while (fgets(line, sizeof(line), fp_route) != NULL) {
			snprintf(routecmd, sizeof (routecmd), "/usr/sbin/ip route add %s table %s", trimNL(line), table);
			system(routecmd);
		}
		fclose(fp_route);
	}
	unlink(buffer);
}


void _del_server_routes(char *server_iface, int client_unit, vpndir_proto_t proto) {
	char buffer[128], routecmd[128], line[128], table[8];
	FILE *fp_route;

	if (proto == VPNDIR_PROTO_OPENVPN)
		snprintf(table, sizeof (table), "ovpnc%d", client_unit);
#ifdef RTCONFIG_WIREGUARD
	else if (proto == VPNDIR_PROTO_WIREGUARD)
		snprintf(table, sizeof (table), "wgc%d", client_unit);
#endif
	else
		return;

	snprintf(buffer, sizeof (buffer), "/usr/sbin/ip route list table %s | grep %s > /tmp/vpnroute_%s_tmp", table, server_iface, table);
	system(buffer);

	snprintf(buffer, sizeof (buffer), "/tmp/vpnroute_%s_tmp", table);
	fp_route = fopen(buffer, "r");

	if (fp_route) {
		while (fgets(line, sizeof(line), fp_route) != NULL) {
			snprintf(routecmd, sizeof (routecmd), "/usr/sbin/ip route del %s table %s", trimNL(line), table);
			system(routecmd);
		}
		fclose(fp_route);
	}
	unlink(buffer);
}

// Unit -1 = all rules;  unit 0 = WAN rules,  unit 1-5: OVPN or WG instance rules
char *amvpn_get_policy_rules(int unit, char *buffer, int bufferlen, vpndir_proto_t proto)
{
	char filename[128];
	int datalen;
	char *buffer_tmp, *buffer_tmp2;
	char *rule, *enable, *desc, *src, *dst, *target;
	char entry[128];

	snprintf(filename, sizeof(filename), "%s/vpndirector_rulelist", OVPN_FS_PATH);

	datalen = f_read(filename, buffer, bufferlen-1);
	if (datalen < 0) {
#ifdef RTCONFIG_SOC_IPQ53XX
		/* Migration commits policy and SDN/profile mappings together in NVRAM.
		 * A later UI save creates the JFFS file, which always takes precedence. */
		if (errno == ENOENT && nvram_match("qca_merlin_vpn_migrated", "1"))
			strlcpy(buffer, nvram_safe_get("vpndirector_rulelist"), bufferlen);
		else
#endif
		buffer[0] = '\0';
	} else {
		buffer[datalen] = '\0';
	}

	if (unit == -1)
		return buffer;

	buffer_tmp = buffer_tmp2 = strdup(buffer);
	buffer[0] = '\0';

	while (buffer_tmp && (rule = strsep(&buffer_tmp2, "<")) != NULL) {
		if((vstrsep(rule, ">", &enable, &desc, &src, &dst, &target)) != 5)
			continue;

		snprintf(entry, sizeof(entry),"<%s>%s>%s>%s>%s",enable, desc, src, dst, target);

		if (unit == 0 && !strcmp(target, "WAN")) {
			strlcat(buffer, entry, bufferlen);
		}
		else if (unit != 0 && !strncmp(target, "OVPN", 4) &&
		    strlen(target) > 4 &&
		    atoi(&target[4]) == unit &&
		    proto == VPNDIR_PROTO_OPENVPN) {
			strlcat(buffer, entry, bufferlen);
		}
#ifdef RTCONFIG_WIREGUARD
		else if (unit != 0 && !strncmp(target, "WGC", 3) &&
		    strlen(target) > 3 &&
		    atoi(&target[3]) == unit &&
		    proto == VPNDIR_PROTO_WIREGUARD) {
			strlcat(buffer, entry, bufferlen);
		}
#endif
	}
	free(buffer_tmp);

	return buffer;
}


int amvpn_set_policy_rules(char* buffer)
{
#ifdef RTCONFIG_SOC_IPQ53XX
	char filename[128], temporary[160];
	const char *next = buffer;
	size_t left = strlen(buffer);
	ssize_t written;
	int fd = -1, dir = -1, created = 0, result = -1, saved_errno;

	if (mkdir(OVPN_FS_PATH, S_IRWXU) && errno != EEXIST)
		return -1;
	/* Persist the directory entry too if this save created openvpn. */
	dir = open("/jffs", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
	if (dir < 0)
		return -1;
	if (fsync(dir))
		goto done;
	close(dir);
	dir = open(OVPN_FS_PATH, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
	if (dir < 0)
		return -1;
	snprintf(filename, sizeof(filename), "%s/vpndirector_rulelist", OVPN_FS_PATH);
	snprintf(temporary, sizeof(temporary), "%s/.vpndirector_rulelist.XXXXXX", OVPN_FS_PATH);
	fd = mkstemp(temporary);
	if (fd < 0)
		goto done;
	created = 1;
	if (fchmod(fd, S_IRUSR | S_IWUSR))
		goto done;
	while (left) {
		written = write(fd, next, left);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0) {
			if (!written) errno = EIO;
			goto done;
		}
		next += written;
		left -= written;
	}
	if (fsync(fd))
		goto done;
	if (close(fd)) {
		fd = -1;
		goto done;
	}
	fd = -1;
	/* Publish only a complete file; a failed first save must not mask the
	 * migration fallback with a truncated or empty destination. */
	if (rename(temporary, filename) || fsync(dir))
		goto done;
	/* The caller commits this retirement after successful publication. */
	result = nvram_unset("vpndirector_rulelist");
done:
	saved_errno = errno;
	if (fd >= 0) close(fd);
	if (created) unlink(temporary);
	close(dir);
	errno = saved_errno;
	return result;
#else
	char filename[128];

	if (!d_exists(OVPN_FS_PATH))
		mkdir(OVPN_FS_PATH, S_IRWXU);

	snprintf(filename, sizeof(filename), "%s/vpndirector_rulelist", OVPN_FS_PATH);
	if (f_write(filename, buffer, strlen(buffer), 0, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) < 0)
		return -1;

	return 0;
#endif
}


void amvpn_clear_exclusive_dns(int unit, vpndir_proto_t proto)
{
	char buffer[32];
	char filename[32];

	if (proto == VPNDIR_PROTO_OPENVPN) {
		snprintf(filename, sizeof (filename), "/etc/openvpn/client%d/dns.sh", unit);
		unit = unit + WG_CLIENT_MAX;
	}
#ifdef RTCONFIG_WIREGUARD
	else if (proto == VPNDIR_PROTO_WIREGUARD) {
		snprintf(filename, sizeof (filename), "/etc/wg/dns%d.sh", unit);
	}
#endif

	sprintf(buffer, "DNSVPN%d", unit);
	eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "udp", "-m", "udp", "--dport", "53", "-j", buffer);
	eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "tcp", "-m", "tcp", "--dport", "53", "-j", buffer);
	eval("/usr/sbin/iptables", "-t", "nat", "-F", buffer);
	eval("/usr/sbin/iptables", "-t", "nat", "-X", buffer);

	if (f_exists(filename))
		unlink(filename);
}


// Recreate the port 53 PREROUTING rules to ensure they are in the correct order (OVPN1 first, OVPN5 last, followed by WGC)
void amvpn_update_exclusive_dns_rules()
{
	int unit;
	char buffer[100];

#ifdef RTCONFIG_WIREGUARD
	for (unit = WG_CLIENT_MAX; unit > 0; unit--) {
		snprintf(buffer, sizeof (buffer), "/etc/wg/dns%d.sh", unit);
		if (f_exists(buffer)) {
			// Remove and re-add to ensure proper order
			snprintf(buffer, sizeof (buffer), "DNSVPN%d", unit);

			eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "udp", "-m", "udp", "--dport", "53", "-j", buffer);
			eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "tcp", "-m", "tcp", "--dport", "53", "-j", buffer);

			eval("/usr/sbin/iptables", "-t", "nat", "-I", "PREROUTING", "-p", "udp", "-m", "udp", "--dport", "53", "-j", buffer);
			eval("/usr/sbin/iptables", "-t", "nat", "-I", "PREROUTING", "-p", "tcp", "-m", "tcp", "--dport", "53", "-j", buffer);
		}
	}
#endif

	for (unit = OVPN_CLIENT_MAX; unit > 0; unit--) {
		snprintf(buffer, sizeof (buffer), "/etc/openvpn/client%d/dns.sh", unit);
		if (f_exists(buffer)) {
			// Remove and re-add to ensure proper order
			snprintf(buffer, sizeof (buffer), "DNSVPN%d", unit + WG_CLIENT_MAX);

			eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "udp", "-m", "udp", "--dport", "53", "-j", buffer);
			eval("/usr/sbin/iptables", "-t", "nat", "-D", "PREROUTING", "-p", "tcp", "-m", "tcp", "--dport", "53", "-j", buffer);

			eval("/usr/sbin/iptables", "-t", "nat", "-I", "PREROUTING", "-p", "udp", "-m", "udp", "--dport", "53", "-j", buffer);
			eval("/usr/sbin/iptables", "-t", "nat", "-I", "PREROUTING", "-p", "tcp", "-m", "tcp", "--dport", "53", "-j", buffer);
		}
	}
}


void ovpn_set_exclusive_dns(int unit) {
	char rules[8000], wanrules[8000], buffer[64], buffer2[64], server[20], iface_match[8];
	char *nvp, *entry;
	char *src, *dst, *iface, *desc, *enable, *netptr;
	struct in_addr addr;
	int mask;
	char prefix[32];

	FILE *fp_resolv, *fp_dns;

	snprintf(buffer, sizeof (buffer), "/etc/openvpn/client%d/resolv.dnsmasq", unit);
	fp_resolv = fopen(buffer, "r");
	snprintf(buffer, sizeof (buffer), "/etc/openvpn/client%d/dns.sh", unit);
	fp_dns = fopen(buffer, "w");

	if (!fp_resolv || !fp_dns) {
		if (fp_resolv)
			fclose(fp_resolv);
		if (fp_dns)
			fclose(fp_dns);
		return;
	}

	fprintf(fp_dns, "#!/bin/sh\n"
	                "/usr/sbin/iptables -t nat -N DNSVPN%d\n",
	                 unit + WG_CLIENT_MAX);

	sprintf(prefix, "vpn_client%d_", unit);

	if (nvram_pf_get_int(prefix, "rgw") == OVPN_RGW_ALL) {
		// Iterate through servers
		while (fgets(buffer2, sizeof(buffer2), fp_resolv) != NULL) {
			if (sscanf(buffer2, "server=%16s", server) != 1)
				continue;

			if (!inet_aton(server, &addr))
				continue;

			fprintf(fp_dns, "/usr/sbin/iptables -t nat -A DNSVPN%d -j DNAT --to-destination %s\n", unit + WG_CLIENT_MAX, server);
			logmessage("openvpn", "Forcing all to use DNS server %s (OpenVPN client %d is set to Exclusive DNS mode)", server, unit);
			// Only configure first server found, as others would never get used
			break;
		}
	} else if (nvram_pf_get_int(prefix, "rgw") == OVPN_RGW_POLICY) {
		amvpn_get_policy_rules(unit, rules, sizeof (rules), VPNDIR_PROTO_OPENVPN);
		amvpn_get_policy_rules(0, wanrules, sizeof (wanrules), VPNDIR_PROTO_OPENVPN);
		strlcat(rules, wanrules, sizeof (rules));

		nvp = rules;

		snprintf(iface_match, sizeof (iface_match), "OVPN%d", unit);

		while ((entry = strsep(&nvp, "<")) != NULL) {
			if (vstrsep(entry, ">", &enable, &desc, &src, &dst, &iface) != 5)
				continue;

			if (atoi(&enable[0]) == 0)
				continue;

			if (*src && !*dst) {
				strlcpy(buffer, src, sizeof(buffer));

				if ((netptr = strchr(buffer, '/'))) {
					*netptr = '\0';
					mask = atoi(&netptr[1]);
				} else {
					mask = 32;
				}

				if ((mask >= 0) &&
				    (mask <= 32) &&
				    (inet_aton(buffer, &addr))) {
					if (!strcmp(iface, iface_match)) {

						// Iterate through servers
						rewind(fp_resolv);
						while (fgets(buffer2, sizeof(buffer2), fp_resolv) != NULL) {
							if (sscanf(buffer2, "server=%16s", server) != 1)
								continue;

							if (!inet_aton(server, &addr))
								continue;

			                                fprintf(fp_dns, "/usr/sbin/iptables -t nat -A DNSVPN%d -s %s -j DNAT --to-destination %s\n", unit + WG_CLIENT_MAX, src, server);
			                                logmessage("openvpn", "Forcing %s to use DNS server %s for OVPNC%d", src, server, unit);
							// Only configure first server found, as others would never get used
							break;
						}
		                        } else if (!strcmp(iface, "WAN")) {
		                                fprintf(fp_dns, "/usr/sbin/iptables -t nat -I DNSVPN%d -s %s -j RETURN\n", unit + WG_CLIENT_MAX, src);
		                                logmessage("openvpn", "Excluding %s from forced DNS routing for OVPNC%d", src, unit);
		                        }
				}
			}
		}
	}

	fprintf(fp_dns, "/usr/sbin/iptables -t nat -I PREROUTING -p udp -m udp --dport 53 -j DNSVPN%d\n"
	                "/usr/sbin/iptables -t nat -I PREROUTING -p tcp -m tcp --dport 53 -j DNSVPN%d\n",
	                 unit + WG_CLIENT_MAX, unit + WG_CLIENT_MAX);

	fclose(fp_resolv);
	fclose(fp_dns);
	sprintf(buffer, "/etc/openvpn/client%d/dns.sh", unit);
	if (f_exists(buffer)) {
		chmod(buffer, 0755);
		eval(buffer);
	}
}


#ifdef RTCONFIG_WIREGUARD
void wgc_set_exclusive_dns(int unit) {
	char rules[8000], wanrules[8000], buffer[64], server[32] = {0}, iface_match[8];
	char *nvp, *entry;
	char *src, *dst, *iface, *desc, *enable, *netptr;
	struct in_addr addr;
	int mask;
	char prefix[32];
	char dns[128] = {0};
	FILE *fp_dns;
	char scriptname[32];

	snprintf(buffer, sizeof (buffer), "wgc%d_enable", unit);
	if (nvram_get_int(buffer) != 1)
		return;

	snprintf(scriptname, sizeof (scriptname), "/etc/wg/dns%d.sh", unit);
	fp_dns = fopen(scriptname, "w");
	if (!fp_dns)
		return;

	snprintf(prefix, sizeof(prefix), "wgc%d_", unit);
	snprintf(dns, sizeof(dns), "%s", nvram_pf_safe_get(prefix, "dns"));

	fprintf(fp_dns, "#!/bin/sh\n"
	                "/usr/sbin/iptables -t nat -N DNSVPN%d\n",
	                 unit);

	amvpn_get_policy_rules(unit, rules, sizeof (rules), VPNDIR_PROTO_WIREGUARD);
	amvpn_get_policy_rules(0, wanrules, sizeof (wanrules), VPNDIR_PROTO_WIREGUARD);
	strlcat(rules, wanrules, sizeof (rules));

	nvp = rules;

	snprintf(iface_match, sizeof (iface_match), "WGC%d", unit);

	while ((entry = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(entry, ">", &enable, &desc, &src, &dst, &iface) != 5)
			continue;

		if (atoi(&enable[0]) == 0)
			continue;

		if (*src && !*dst) {
			strlcpy(buffer, src, sizeof(buffer));

			if ((netptr = strchr(buffer, '/'))) {
				*netptr = '\0';
				mask = atoi(&netptr[1]);
			} else {
				mask = 32;
			}

			if ((mask >= 0) &&
			    (mask <= 32) &&
			    (inet_aton(buffer, &addr))) {
				if (!strcmp(iface, iface_match)) {
					if (dns[0] != '\0') {
						char *next = NULL;
						char *p = NULL;

						foreach_44 (server, dns, next) {
							if ((p = strchr(server, '/')) != NULL)
								*p = '\0';

							fprintf(fp_dns, "/usr/sbin/iptables -t nat -A DNSVPN%d -s %s -j DNAT --to-destination %s\n", unit, src, server);
							logmessage("wireguard", "Forcing %s to use DNS server %s for WGC%d", src, server, unit);

							// currently added by rc/wireguard.c - should I add it to the correct table, like Fusion?
							//eval("ip", "route", "add", server, "dev", ifname);
						}
					}
	                        } else if (!strcmp(iface, "WAN")) {
	                                fprintf(fp_dns, "/usr/sbin/iptables -t nat -I DNSVPN%d -s %s -j RETURN\n", unit, src);
	                                logmessage("wireguard", "Excluding %s from forced DNS routing for WGC%d", src, unit);
	                        }
			}
		}
	}

	fprintf(fp_dns, "/usr/sbin/iptables -t nat -I PREROUTING -p udp -m udp --dport 53 -j DNSVPN%d\n"
	                "/usr/sbin/iptables -t nat -I PREROUTING -p tcp -m tcp --dport 53 -j DNSVPN%d\n",
	                 unit, unit);

	fclose(fp_dns);
	chmod(scriptname, 0755);
	eval(scriptname);
}
#endif


#if defined(RTCONFIG_SOC_IPQ53XX) && defined(RTCONFIG_IPV6)
/* The IPQ53xx kernel has no IPv6 policy tables. Until dual-stack policy
 * routing exists, protect whole-interface assignments by blocking forwarded
 * IPv6, including while the IPv4 tunnel is up. Never guess an IPv6 identity
 * from an IPv4 source/destination rule.
 */
static int amvpn_ipv6_enforced(vpndir_proto_t proto, int unit)
{
	char prefix[32];
	int rgw;

	if (proto == VPNDIR_PROTO_OPENVPN && unit >= 1 && unit <= OVPN_CLIENT_MAX) {
		snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
		rgw = nvram_pf_get_int(prefix, "rgw");
		return nvram_pf_get_int(prefix, "enforce") && ovpn_is_client_enabled(unit) &&
		       (rgw == OVPN_RGW_ALL || rgw == OVPN_RGW_POLICY);
	}
#ifdef RTCONFIG_WIREGUARD
	if (proto == VPNDIR_PROTO_WIREGUARD && unit >= 1 && unit <= WG_CLIENT_MAX) {
		snprintf(prefix, sizeof(prefix), "wgc%d_", unit);
		return nvram_pf_get_int(prefix, "enforce") && nvram_pf_get_int(prefix, "enable");
	}
#endif
	return 0;
}

static int amvpn_ipv4_wildcard(const char *address)
{
	return !*address || !strcmp(address, "0.0.0.0") || !strcmp(address, "0.0.0.0/0");
}

static int amvpn_ipv6_catchall(vpndir_proto_t proto, int unit)
{
	char buffer[8000], *next, *rule;
	char *enable, *desc, *src, *dst, *target;

	amvpn_get_policy_rules(unit, buffer, sizeof(buffer), proto);
	next = buffer;
	while ((rule = strsep(&next, "<")) != NULL) {
		if (vstrsep(rule, ">", &enable, &desc, &src, &dst, &target) == 5 &&
		    atoi(enable) && strcmp(target, "WAN") &&
		    amvpn_ipv4_wildcard(src) && amvpn_ipv4_wildcard(dst))
			return 1;
	}
	return 0;
}

static void amvpn_ipv6_drop(FILE *fp, const char *ifname)
{
	const char *p;
	if (!ifname || !*ifname || strlen(ifname) >= IFNAMSIZ)
		return;
	/* Interface names are emitted into an iptables-restore file. */
	for (p = ifname; *p; ++p)
		if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.')
			return;
	fprintf(fp, "-A VPN6KS -i %s -j DROP\n", ifname);
}

/* Called inside rc's full filter transaction, after other FORWARD rules.
 * Insertion at the head also protects already-established connections.
 */
void amvpn_write_ipv6_killswitch(FILE *fp)
{
	int unit, global = 0, catchall = 0;
	char prefix[32];
#ifdef RTCONFIG_MULTILAN_CFG
	MTLAN_T *pmtl;
	VPN_VPNX_T vpnx;
	size_t mtl_sz = 0;
	int i, j, protect;
#endif

	for (unit = 1; unit <= OVPN_CLIENT_MAX; ++unit) {
		if (!amvpn_ipv6_enforced(VPNDIR_PROTO_OPENVPN, unit))
			continue;
		snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
		if (nvram_pf_get_int(prefix, "rgw") == OVPN_RGW_ALL)
			global = 1;
		else if (amvpn_ipv6_catchall(VPNDIR_PROTO_OPENVPN, unit))
			catchall = 1;
	}
#ifdef RTCONFIG_WIREGUARD
	for (unit = 1; unit <= WG_CLIENT_MAX; ++unit)
		if (amvpn_ipv6_enforced(VPNDIR_PROTO_WIREGUARD, unit) &&
		    amvpn_ipv6_catchall(VPNDIR_PROTO_WIREGUARD, unit))
			catchall = 1;
#endif

	fprintf(fp, ":VPN6KS - [0:0]\n");
	if (global || catchall)
		amvpn_ipv6_drop(fp, nvram_safe_get("lan_ifname"));
#ifdef RTCONFIG_MULTILAN_CFG
	pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
	if (pmtl) {
		get_mtlan(pmtl, &mtl_sz);
		/* Include the default network if it has an explicit VPN assignment. */
		for (i = 0; i < mtl_sz; ++i) {
			if (!pmtl[i].enable || !*pmtl[i].nw_t.ifname)
				continue;
			if ((global || catchall) && !strcmp(pmtl[i].nw_t.ifname, nvram_safe_get("lan_ifname")))
				continue;
			protect = catchall || (global && pmtl[i].sdn_t.vpnc_idx == 0);
			if (pmtl[i].sdn_t.vpnc_idx && get_vpnx_by_vpnc_idx(&vpnx, pmtl[i].sdn_t.vpnc_idx)) {
				if (vpnx.proto == VPN_PROTO_OVPN)
					protect |= amvpn_ipv6_enforced(VPNDIR_PROTO_OPENVPN, vpnx.unit);
#ifdef RTCONFIG_WIREGUARD
				else if (vpnx.proto == VPN_PROTO_WG)
					protect |= amvpn_ipv6_enforced(VPNDIR_PROTO_WIREGUARD, vpnx.unit);
#endif
			}
			if (!protect)
				continue;
			/* Duplicate network records must not duplicate the rule. */
			for (j = 0; j < i; ++j)
				if (pmtl[j].enable && pmtl[j].sdn_t.vpnc_idx == pmtl[i].sdn_t.vpnc_idx &&
				    !strcmp(pmtl[j].nw_t.ifname, pmtl[i].nw_t.ifname))
					break;
			if (j == i)
				amvpn_ipv6_drop(fp, pmtl[i].nw_t.ifname);
		}
		FREE_MTLAN((void *)pmtl);
	}
#endif
	fprintf(fp, "-I FORWARD 1 -j VPN6KS\n");
}

/* Recompute all units from configuration: stopping one tunnel must not remove
 * another's protection. --noflush with a declared user chain replaces that
 * chain atomically while retaining the rest of the filter table.
 */
void amvpn_refresh_ipv6_killswitch(void)
{
	char path[] = "/tmp/vpn6ks.XXXXXX";
	int fd, lock, ret;
	FILE *fp;

	lock = file_lock("vpn6ks");
	fd = mkstemp(path);
	if (fd < 0) {
		file_unlock(lock);
		return;
	}
	fp = fdopen(fd, "w");
	if (!fp) {
		close(fd);
		unlink(path);
		file_unlock(lock);
		return;
	}
	fprintf(fp, "*filter\n");
	if (!eval("ip6tables", "-C", "FORWARD", "-j", "VPN6KS"))
		fprintf(fp, "-D FORWARD -j VPN6KS\n");
	amvpn_write_ipv6_killswitch(fp);
	fprintf(fp, "COMMIT\n");
	ret = ferror(fp);
	if (fclose(fp))
		ret = 1;
	if (ret || eval("ip6tables-restore", "--noflush", path))
		logmessage("vpndirector", "Failed to update IPv6 kill switch");
	unlink(path);
	file_unlock(lock);
}
#endif


/* Will remove all rules for a specific unit
   Will also remove all rules for a specific SDN interface,
   or search for any SDN that is tied to specified unit and remove
   the rules.
*/

void amvpn_clear_killswitch_rules(vpndir_proto_t proto, int unit, char *sdn_ifname) {
	int prio, verb = 3;
	char buffer[256], prio_str[6];
#ifdef RTCONFIG_MULTILAN_CFG
	int vpnc_idx, i;
	MTLAN_T *pmtl = NULL;
	size_t mtl_sz = 0;
#endif

	if (proto == VPNDIR_PROTO_OPENVPN) {
		prio = VPNDIR_PRIO_KS_OPENVPN + unit - 1;

		snprintf(buffer, sizeof (buffer), "vpn_client%d_verb", unit);
		verb = nvram_get_int(buffer);

		// Delete as many priority "prio" as there are rules of that priority
		snprintf(buffer, sizeof (buffer), "ip rule show priority %d | while read -r rule; do ip rule del priority %d; done", prio, prio);
		if (verb > 3)
			logmessage("openvpn-routing", "Clearing killswitch for OpenVPN unit %d", unit);
		system(buffer);
#ifdef RTCONFIG_WIREGUARD
	} else if (proto == VPNDIR_PROTO_WIREGUARD) {
		prio = VPNDIR_PRIO_KS_WIREGUARD + unit - 1;

		// Delete as many priority "prio" as there are rules of that priority
		snprintf(buffer, sizeof (buffer), "ip rule show priority %d | while read -r rule; do ip rule del priority %d; done", prio, prio);
		if (verb > 3)
			logmessage("openvpn-routing", "Clearing killswitch for WireGuard unit %d", unit);
		system(buffer);
#endif
	}


#ifdef RTCONFIG_MULTILAN_CFG
	prio = VPNDIR_PRIO_KS_SDN;
	snprintf(prio_str, sizeof (prio_str), "%d", prio);

	// SDN specified interface
	if (sdn_ifname && *sdn_ifname) {
		eval("ip", "rule", "del", "priority", prio_str, "iif", sdn_ifname);
		if (verb > 3)
			logmessage("openvpn-routing", "Clearing killswitch for SDN %s", sdn_ifname);

	} else if (unit == -1) {	// Remove all existing SDN rules
		snprintf(buffer, sizeof (buffer), "ip rule show priority %d | while read -r rule; do ip rule del priority %d; done", prio, prio);
		if (verb > 3)
			logmessage("openvpn-routing", "Clearing killswitch for all SDN");
		system(buffer);

	} else {	// Only by unit, so walk through whole SDN list for matching vpnc_idx
		vpnc_idx = get_vpnc_idx_by_proto_unit((proto == VPNDIR_PROTO_OPENVPN ? VPN_PROTO_OVPN : VPN_PROTO_WG), unit);
		pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
		if (pmtl) {
			get_mtlan(pmtl, &mtl_sz);

			for (i = 1; i < mtl_sz; ++i) {	// Skip first (Default) SDN
				if (pmtl[i].sdn_t.vpnc_idx == vpnc_idx) {
					eval("ip", "rule", "del", "priority", prio_str,  "iif", pmtl[i].nw_t.ifname);
					if (verb > 3)
						logmessage("openvpn-routing", "Clearing killswitch for SDN %s", pmtl[i].nw_t.ifname);
				}
			}
			FREE_MTLAN((void *)pmtl);
		}

	}
#endif
}


#ifdef RTCONFIG_SOC_IPQ53XX
/* Keep the same selectors as the Director rule, including destination-only and
 * catch-all policies. An omitted selector means all, just as in routing rules.
 */
static void amvpn_add_policy_killswitch(char *src, char *dst, char *prio)
{
	char *destination = (*dst && strcmp(dst, "0.0.0.0")) ? dst : "all";
#ifdef RTCONFIG_MULTILAN_CFG
	MTLAN_T *pmtl;
	size_t mtl_sz = 0;
	int i, j;
#endif

	if (*src && strcmp(src, "0.0.0.0") && strcmp(src, "0.0.0.0/0")) {
		eval("ip", "rule", "add", "from", src, "to", destination,
		     "priority", prio, "prohibit");
		return;
	}

	/* Wildcard source policies protect forwarded LAN traffic. Blocking locally
	 * originated traffic here would prevent the tunnel from reconnecting.
	 */
	eval("ip", "rule", "add", "iif", nvram_safe_get("lan_ifname"),
	     "to", destination, "priority", prio, "prohibit");
#ifdef RTCONFIG_MULTILAN_CFG
	pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
	if (pmtl) {
		get_mtlan(pmtl, &mtl_sz);
		for (i = 1; i < mtl_sz; ++i) {
			if (!pmtl[i].enable || !*pmtl[i].nw_t.ifname ||
			    !strcmp(pmtl[i].nw_t.ifname, nvram_safe_get("lan_ifname")))
				continue;
			for (j = 1; j < i; ++j)
				if (pmtl[j].enable && !strcmp(pmtl[j].nw_t.ifname, pmtl[i].nw_t.ifname))
					break;
			if (j < i)
				continue;
			eval("ip", "rule", "add", "iif", pmtl[i].nw_t.ifname,
			     "to", destination, "priority", prio, "prohibit");
		}
		FREE_MTLAN((void *)pmtl);
	}
#endif
}

#endif

void amvpn_set_killswitch_rules(vpndir_proto_t proto, int unit, char *sdn_ifname) {
	char buffer[8000], prefix[32], prio_str[6];
	char *buffer_tmp, *buffer_tmp2, *rule;
	char *enable, *desc, *target, *src, *dst;
	int killswitch, rgw, verb = 3;
#ifdef RTCONFIG_MULTILAN_CFG
	int vpnc_idx, i;
	MTLAN_T *pmtl = NULL;
	size_t mtl_sz = 0;
#endif
#ifdef RTCONFIG_WIREGUARD
	int enabled;
#endif

#if defined(RTCONFIG_SOC_IPQ53XX) && defined(RTCONFIG_IPV6)
	amvpn_refresh_ipv6_killswitch();
#endif

	if (proto == VPNDIR_PROTO_OPENVPN) {
		// Clear existing rules
		amvpn_clear_killswitch_rules(VPNDIR_PROTO_OPENVPN, unit, sdn_ifname);

		snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_OPENVPN + unit - 1);

                snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
                killswitch = nvram_pf_get_int(prefix, "enforce");
                rgw = nvram_pf_get_int(prefix, "rgw");
		verb = nvram_pf_get_int(prefix, "verb");

		if (killswitch == 0 || ovpn_is_client_enabled(unit) == 0)
			return;

		if (rgw == OVPN_RGW_ALL) {
			eval("ip", "rule", "add", "from", "all", "iif", nvram_safe_get("lan_ifname"), "priority", prio_str, "prohibit");
			if (verb > 3)
				logmessage("openvpn-routing","Setting global killswitch rule for OpenVPN client %d", unit);
		} else if (rgw == OVPN_RGW_POLICY) {
			/* Do VPNDirector */
			amvpn_get_policy_rules(unit, buffer, sizeof (buffer), proto);
			buffer_tmp = buffer_tmp2 = strdup(buffer);

			while (buffer_tmp && (rule = strsep(&buffer_tmp2, "<")) != NULL) {
				if((vstrsep(rule, ">", &enable, &desc, &src, &dst, &target)) != 5)
					continue;

				if (!atoi(&enable[0]))
					continue;

				if (!strcmp(target,"WAN"))
					continue;

#ifdef RTCONFIG_SOC_IPQ53XX
				if (!strncmp(target, "OVPN", 4)) {
					// Create deny rule
					amvpn_add_policy_killswitch(src, dst, prio_str);
#else
				if (!strncmp(target, "OVPN", 4) && *src && strcmp(src, "0.0.0.0")) {
					// Create deny rule
					eval("ip", "rule", "add", "from", src, "priority", prio_str, "prohibit");
#endif
					if (verb > 3)
						logmessage("openvpn-routing","Setting killswitch rule for %s", src);
				}
			}
			free(buffer_tmp);
		}
#ifdef RTCONFIG_MULTILAN_CFG
		if (rgw == OVPN_RGW_ALL || rgw == OVPN_RGW_POLICY) {
			/* Do SDN - use supplied interface if available */
			if (sdn_ifname && *sdn_ifname) {
				snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_SDN);
				eval("ip", "rule", "add", "from", "all", "iif", sdn_ifname, "priority", prio_str, "prohibit");
				if (verb > 3)
					logmessage("openvpn-routing", "Setting killswitch rule for SDN %s", sdn_ifname);
			} else {	/* Locate SDN instance that might be using this VPN client */
				vpnc_idx = get_vpnc_idx_by_proto_unit(VPN_PROTO_OVPN, unit);
				pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
				if (pmtl) {
					get_mtlan(pmtl, &mtl_sz);

					for (i = 1; i < mtl_sz; ++i) {	// Skip first (Default) SDN
						if ((rgw == OVPN_RGW_POLICY && pmtl[i].sdn_t.vpnc_idx == vpnc_idx) ||
						    (rgw == OVPN_RGW_ALL && pmtl[i].sdn_t.vpnc_idx == 0)) {
#ifdef RTCONFIG_SOC_IPQ53XX
							/* Assigned SDNs own a separate guard from Director policy. */
							if (rgw == OVPN_RGW_POLICY)
								snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_SDN);
#endif
							eval("ip", "rule", "add", "from", "all", "priority", prio_str,  "iif", pmtl[i].nw_t.ifname, "prohibit");
							if (verb > 3)
								logmessage("openvpn-routing", "Setting killswitch rule for SDN %s", pmtl[i].nw_t.ifname);
						}
					}
					FREE_MTLAN((void *)pmtl);
				}
			}
		}
#endif
#ifdef RTCONFIG_WIREGUARD
	} else if (proto == VPNDIR_PROTO_WIREGUARD) {
		// Clear existing rules
		amvpn_clear_killswitch_rules(VPNDIR_PROTO_WIREGUARD, unit, sdn_ifname);

		snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_WIREGUARD + unit - 1);

		snprintf(prefix, sizeof(prefix), "wgc%d_", unit);
		killswitch = nvram_pf_get_int(prefix, "enforce");

		enabled = nvram_pf_get_int(prefix, "enable");
		if (killswitch == 0 || enabled == 0)
			return;

		/* Do VPNDirector */
		amvpn_get_policy_rules(unit, buffer, sizeof (buffer), proto);
		buffer_tmp = buffer_tmp2 = strdup(buffer);

		while (buffer_tmp && (rule = strsep(&buffer_tmp2, "<")) != NULL) {
			if((vstrsep(rule, ">", &enable, &desc, &src, &dst, &target)) != 5)
				continue;

			if (!atoi(&enable[0]))
				continue;

			if (!strcmp(target,"WAN"))
				continue;

#ifdef RTCONFIG_SOC_IPQ53XX
			if (!strncmp(target, "WGC", 3)) {
				// Create deny rule
				amvpn_add_policy_killswitch(src, dst, prio_str);
#else
			if (!strncmp(target, "WGC", 3) && *src && strcmp(src, "0.0.0.0")) {
				// Create deny rule
				eval("ip", "rule", "add", "from", src, "priority", prio_str, "prohibit");
#endif
				if (verb > 3)
					logmessage("openvpn-routing","Setting killswitch rule for %s", src);
			}
		}
		free(buffer_tmp);
#ifdef RTCONFIG_MULTILAN_CFG
		/* Do SDN - use supplied interface if available */
		if (sdn_ifname && *sdn_ifname) {
			snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_SDN);
			eval("ip", "rule", "add", "from", "all", "iif", sdn_ifname, "priority", prio_str, "prohibit");
			if (verb > 3)
				logmessage("openvpn-routing", "Setting killswitch rule for SDN %s", sdn_ifname);
		} else {	/* Locate SDN instance that might be using this VPN client */
			vpnc_idx = get_vpnc_idx_by_proto_unit(VPN_PROTO_WG, unit);
			pmtl = (MTLAN_T *)INIT_MTLAN(sizeof(MTLAN_T));
			if (pmtl) {
				get_mtlan(pmtl, &mtl_sz);

				for (i = 1; i < mtl_sz; ++i) {	// Skip first (Default) SDN
					if (pmtl[i].sdn_t.vpnc_idx == vpnc_idx) {
#ifdef RTCONFIG_SOC_IPQ53XX
						snprintf(prio_str, sizeof(prio_str), "%d", VPNDIR_PRIO_KS_SDN);
#endif
						eval("ip", "rule", "add", "from", "all", "priority", prio_str,  "iif", pmtl[i].nw_t.ifname, "prohibit");
						if (verb > 3)
							logmessage("openvpn-routing", "Setting killswitch rule for SDN %s", pmtl[i].nw_t.ifname);
					}
				}
				FREE_MTLAN((void *)pmtl);
			}
		}
#endif
#endif
	}
}

void amvpn_set_kilswitch_rules_all() {
	int i;
		logmessage("openvpn-routing", "Applying all killswitches");
#ifdef RTCONFIG_WIREGUARD
	for (i = WG_CLIENT_MAX; i > 0; i--) {
		amvpn_set_killswitch_rules(VPNDIR_PROTO_WIREGUARD, i, NULL);
	}
#endif
	for (i = OVPN_CLIENT_MAX; i > 0; i --) {
		amvpn_set_killswitch_rules(VPNDIR_PROTO_OPENVPN, i, NULL);
	}
}
