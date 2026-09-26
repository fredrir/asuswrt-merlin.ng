#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <shared.h>
#include <vpn_utils.h>
#ifdef RTCONFIG_OPENVPN
#include <openvpn_config.h>
#endif

#if defined(RTCONFIG_VPN_FUSION) || defined(RTCONFIG_TPVPN) || defined(RTCONFIG_IG_SITE2SITE) || defined(RTCONFIG_WIREGUARD)
/*******************************************************************
 * NAME: vpnc_set_basic_conf
 * AUTHOR: Andy Chiu
 * CREATE DATE: 2016/12/07
 * DESCRIPTION: set basic config dat structure
 * INPUT:  server: string. server ip. username: string. password: string.
 * OUTPUT:  basic_conf: a pointer of VPNC_BASIC_CONF
 * RETURN:  0: success, -1: fialed
 * NOTE:
 *******************************************************************/
static int
vpnc_set_basic_conf(const char *server, const char *username, const char *passwd, VPNC_BASIC_CONF *basic_conf)
{
	if (!basic_conf)
		return -1;

	memset(basic_conf, 0, sizeof(VPNC_BASIC_CONF));

	if (server)
		snprintf(basic_conf->server, sizeof(basic_conf->server), "%s", server);
	if (username)
		snprintf(basic_conf->username, sizeof(basic_conf->username), "%s", username);
	if (passwd)
		snprintf(basic_conf->password, sizeof(basic_conf->password), "%s", passwd);

	return 0;
}

#ifdef RTCONFIG_OPENVPN
static void _update_ovpn_client_enable(int unit, int enable)
{
	char buf[32] = {0};
	char *cp;
	char unit_str[4] = {0};
	int i;
	int ovpnc_enable[OVPN_CLIENT_MAX] = {0};

	nvram_safe_get_r("vpn_clientx_eas", buf, sizeof(buf));
	for( cp = strtok(buf, ","); cp != NULL; cp = strtok(NULL, ",")) {
		i = (int)strtol(cp, NULL, 0);
		if(i > OVPN_CLIENT_MAX || i <=0)
			continue;
		ovpnc_enable[i-1] = 1;
	}

	if (ovpnc_enable[unit-1] != ((enable) ? 1 : 0)) {
		ovpnc_enable[unit-1] = ((enable) ? 1 : 0);
		memset(buf, 0, sizeof(buf));
		for(i = 1; i <= OVPN_CLIENT_MAX; i++) {
			if (ovpnc_enable[i-1]) {
				snprintf(unit_str, sizeof(unit_str), "%d,", i);
				strlcat(buf, unit_str, sizeof(buf));
			}
		}
		nvram_set("vpn_clientx_eas", buf);
	}
}
#endif

/*******************************************************************
 * NAME: vpnc_load_profile
 * AUTHOR: Andy Chiu
 * CREATE DATE: 2016/12/07
 * DESCRIPTION: Parser the nvram setting and load the VPNC_PROFILE list
 * INPUT:  list: an array to store vpnc profile, list_size: size of list
 * OUTPUT:
 * RETURN:  number of profiles, -1: fialed
 * NOTE:
 *******************************************************************/
int vpnc_load_profile(VPNC_PROFILE *list, const int list_size, const int prof_ver)
{
	char *nv = NULL, *nvp = NULL, *b = NULL;
	int cnt = 0, i;
	char *desc, *proto, *server, *username, *passwd, *active, *vpnc_idx;
	char *region, *conntype;
	char *wan_idx, *caller, *tunnel;

	if (!list || list_size <= 0)
		return -1;

	// load "vpnc_clientlist" to set username, password and server ip
	nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));

	cnt = 0;
	memset(list, 0, sizeof(VPNC_PROFILE) * list_size);
	while (nv && (b = strsep(&nvp, "<")) != NULL && cnt <= list_size)
	{
		desc = proto = server = username = passwd = active = vpnc_idx = NULL;
		region = conntype = NULL;
		wan_idx = caller = tunnel = NULL;

		if (VPNC_PROFILE_VER1 == prof_ver)
		{
			// proto, server, active and vpnc_idx are mandatory
			if (vstrsep(b, ">", &desc, &proto, &server, &username, &passwd, &active, &vpnc_idx, &region, &conntype, &tunnel, &wan_idx, &caller) < 4)
				continue;

			if (!active || !vpnc_idx)
				continue;

			list[cnt].active = (int)strtol(active, NULL, 0);
			list[cnt].vpnc_idx = (int)strtol(vpnc_idx, NULL, 0);
			list[cnt].wan_idx = (wan_idx) ? (int)strtol(wan_idx, NULL, 0) : 0;
		}
		else
		{
			// proto and server are mandatory
			if (vstrsep(b, ">", &desc, &proto, &server, &username, &passwd) < 2)
				continue;
		}

		if (proto && server)
		{
			vpnc_set_basic_conf(server, username, passwd, &(list[cnt].basic));

			if (!strcmp(proto, PROTO_PPTP))
			{
				list[cnt].protocol = VPNC_PROTO_PPTP;
			}
			else if (!strcmp(proto, PROTO_L2TP))
			{
				list[cnt].protocol = VPNC_PROTO_L2TP;
			}
#ifdef RTCONFIG_OPENVPN
			else if (!strcmp(proto, PROTO_OVPN) || !strcmp(proto, PROTO_CYBERGHOST))
			{
				list[cnt].protocol = VPNC_PROTO_OVPN;
				list[cnt].config.ovpn.ovpn_idx = (int)strtol(server, NULL, 0);
				_update_ovpn_client_enable(list[cnt].config.ovpn.ovpn_idx, list[cnt].active);
			}
#endif
#ifdef RTCONFIG_WIREGUARD
			else if (!strcmp(proto, PROTO_WG) || !strcmp(proto, PROTO_SURFSHARK))
			{
				char prefix[16] = {0};
				list[cnt].protocol = VPNC_PROTO_WG;
				list[cnt].config.wg.wg_idx = (int)strtol(server, NULL, 0);
				snprintf(prefix, sizeof(prefix), "%s%d_", WG_CLIENT_NVRAM_PREFIX, list[cnt].config.wg.wg_idx);
				nvram_pf_set_int(prefix, "enable", list[cnt].active);
			}
#endif
#ifdef RTCONFIG_TPVPN
#ifdef RTCONFIG_HMA
			else if (!strcmp(proto, PROTO_HMA))
			{
				if (is_tpvpn_configured(TPVPN_HMA, region, conntype, (int)strtol(server, NULL, 0)))
				{
					list[cnt].protocol = VPNC_PROTO_OVPN;
					list[cnt].config.ovpn.ovpn_idx = (int)strtol(server, NULL, 0);
					_update_ovpn_client_enable(list[cnt].config.ovpn.ovpn_idx, list[cnt].active);
				}
				else
				{
					list[cnt].protocol = VPNC_PROTO_HMA;
					list[cnt].config.tpvpn.tpvpn_idx = (int)strtol(server, NULL, 0);
					if (region && conntype)
					{
						strlcpy(list[cnt].config.tpvpn.region, region, sizeof(list[cnt].config.tpvpn.region));
						strlcpy(list[cnt].config.tpvpn.conntype, conntype, sizeof(list[cnt].config.tpvpn.conntype));
					}
					else
						logmessage_normal("VPN", "no data for HMA\n");
				}
			}
#endif
#ifdef RTCONFIG_NORDVPN
			else if (!strcmp(proto, PROTO_NORDVPN))
			{
				if (is_tpvpn_configured(TPVPN_NORDVPN, region, conntype, (int)strtol(server, NULL, 0)))
				{
					char prefix[16] = {0};
					list[cnt].protocol = VPNC_PROTO_WG;
					list[cnt].config.wg.wg_idx = (int)strtol(server, NULL, 0);
					snprintf(prefix, sizeof(prefix), "%s%d_", WG_CLIENT_NVRAM_PREFIX, list[cnt].config.wg.wg_idx);
					nvram_pf_set_int(prefix, "enable", list[cnt].active);
				}
				else
				{
					list[cnt].protocol = VPNC_PROTO_NORDVPN;
					list[cnt].config.tpvpn.tpvpn_idx = (int)strtol(server, NULL, 0);
					if (region)
						strlcpy(list[cnt].config.tpvpn.region, region, sizeof(list[cnt].config.tpvpn.region));
					else
						logmessage_normal("VPN", "no data for NordVPN\n");
				}
			}
#endif
#endif
			else if (!strcmp(proto, PROTO_IPSec))
			{
				list[cnt].protocol = VPNC_PROTO_IPSEC;
				list[cnt].config.ipsec.prof_idx = (int)strtol(server, NULL, 0);
			}
			++cnt;
		}
	}
	SAFE_FREE(nv);

	// load "vpnc_pptp_options_x_list" to set pptp option
	nv = nvp = strdup(nvram_safe_get("vpnc_pptp_options_x_list"));
	i = 0;
	while (nv && (b = strsep(&nvp, "<")) != NULL && i <= cnt)
	{

		if (i > 0 && VPNC_PROTO_PPTP == list[i - 1].protocol)
		{
			if (!strcmp(b, "auto"))
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_AUTO;
			else if (!strcmp(b, "-mppc"))
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPC;
			else if (!strcmp(b, "+mppe-40"))
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE40;
			else if (!strcmp(b, "+mppe-56"))
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE56;
			else if (!strcmp(b, "+mppe-128"))
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE128;
			else
				list[i - 1].config.pptp.option = VPNC_PPTP_OPT_UNDEF;
		}
		++i;
	}
	SAFE_FREE(nv);
	if (i != cnt + 1)
		_dprintf("[%s]the numbers of vpnc_clientlist(%d) and vpnc_pptp_options_x_list(%d) are different!\n", __FUNCTION__, cnt, i);

	return cnt;
}

int _get_new_vpnc_index(void)
{
	VPNC_PROFILE prof[MAX_VPNC_PROFILE];
	unsigned char idx_array[MAX_VPNC_PROFILE] ;
	int prof_cnt = 0, i;

	prof_cnt = vpnc_load_profile(prof, MAX_VPNC_PROFILE, VPNC_LOAD_CLIENT_LIST);

	memset(idx_array, 0, sizeof(idx_array));

	for(i = 0; i < prof_cnt; ++i)
	{
		if((prof[i].vpnc_idx - VPNC_UNIT_BASIC) >= MAX_VPNC_PROFILE || 
			(prof[i].vpnc_idx - VPNC_UNIT_BASIC) < 0)
			continue;
		idx_array[prof[i].vpnc_idx - VPNC_UNIT_BASIC] = 1;
	}

	for(i = 0; i < MAX_VPNC_PROFILE; ++i)
	{
		if(!idx_array[i])
			return i + VPNC_UNIT_BASIC;
	}
	return 0;
}
#endif

#ifdef RTCONFIG_TPVPN
int is_tpvpn_configured(int provider, const char* region, const char* conntype, int unit)
{
	char prefix[16] = {0};

	if (!region || !conntype) return 0;

	switch (provider)
	{
		case TPVPN_HMA:
			snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
			if (!strcmp(nvram_pf_safe_get(prefix, "tp"), TPVPN_PSZ_HMA)
			 && !strcmp(nvram_pf_safe_get(prefix, "tp_region"), region)
			 && !strcmp(nvram_pf_safe_get(prefix, "tp_proto"), conntype)
			)
				return 1;
			break;
		case TPVPN_NORDVPN:
			snprintf(prefix, sizeof(prefix), "wgc%d_", unit);
			if (!strcmp(nvram_pf_safe_get(prefix, "tp"), TPVPN_PSZ_NORDVPN)
			 && !strcmp(nvram_pf_safe_get(prefix, "tp_region"), region)
			)
				return 1;
			break;
	}
	return 0;
}
#endif

#if defined(RTCONFIG_VPN_FUSION) || defined(RTCONFIG_WIREGUARD) || defined(RTCONFIG_NORDVPN)
/* Validate the complete single-peer profile before changing any NVRAM. */
static int wgconf_number(const char *value, unsigned long max)
{
	char *end;
	unsigned long number;
	if (!value[0] || strspn(value, "0123456789") != strlen(value))
		return 0;
	number = strtoul(value, &end, 10);
	return !*end && number <= max;
}

static int wgconf_key(const char *value)
{
	const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const char *last;
	int i;
	if (strlen(value) != 44 || value[43] != '=')
		return 0;
	for (i = 0; i < 43; i++)
		if (!strchr(alphabet, value[i])) return 0;
	last = strchr(alphabet, value[42]);
	return last && ((last - alphabet) & 3) == 0;
}

static int wgconf_host(const char *value)
{
	unsigned char address[16];
	size_t len = strlen(value);
	if (inet_pton(AF_INET, value, address) == 1 || inet_pton(AF_INET6, value, address) == 1)
		return 1;
	return len && len <= 253 && value[0] != '.' && value[0] != '-'
		&& !strstr(value, "..")
		&& strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.") == len;
}

static int wgconf_list(const char *value, int dns)
{
	char *copy, *cursor, *item, *mask;
	unsigned char address[16];
	int bits, valid = 1;
	if (!(copy = strdup(value))) return 0;
	cursor = copy;
	while ((item = strsep(&cursor, ","))) {
		if (dns) {
			if (!wgconf_host(item)) { valid = 0; break; }
			continue;
		}
		mask = strchr(item, '/');
		if (mask) *mask++ = '\0';
		bits = inet_pton(AF_INET, item, address) == 1 ? 32 :
			(inet_pton(AF_INET6, item, address) == 1 ? 128 : 0);
		if (!bits || (mask && !wgconf_number(mask, bits))) { valid = 0; break; }
	}
	free(copy);
	return valid;
}

int read_wgc_config_file(const char *file_path, int wgc_unit)
{
	char prefix[8], priv[45] = "", pub[45] = "", psk[45] = "";
	char addr[64] = "", dns[128] = "", mtu[6] = "", allowed[4096] = "";
	char endpoint[264] = "", alive[6] = "";
	struct {
		const char *name, *nvname;
		char *value;
		size_t capacity;
		int section, multiple, seen;
	} fields[] = {
		{ "PrivateKey", "priv", priv, sizeof(priv), 1, 0, 0 },
		{ "Address", "addr", addr, sizeof(addr), 1, 1, 0 },
		{ "DNS", "dns", dns, sizeof(dns), 1, 1, 0 },
		{ "MTU", "mtu", mtu, sizeof(mtu), 1, 0, 0 },
		{ "PublicKey", "ppub", pub, sizeof(pub), 2, 0, 0 },
		{ "PresharedKey", "psk", psk, sizeof(psk), 2, 0, 0 },
		{ "AllowedIPs", "aips", allowed, sizeof(allowed), 2, 1, 0 },
		{ "Endpoint", NULL, endpoint, sizeof(endpoint), 2, 0, 0 },
		{ "PersistentKeepalive", "alive", alive, sizeof(alive), 2, 0, 0 },
	};
	FILE *fp;
	long length;
	char *data = NULL, *cursor, *line, *value, *read, *write, *host, *port;
	unsigned char address[16];
	size_t i, used;
	int section = 0, have_interface = 0, have_peer = 0, result = -1;

	if (!file_path || !file_path[0] || wgc_unit < 1 || wgc_unit > WG_CLIENT_MAX)
		return -1;
	if (!(fp = fopen(file_path, "r"))) return -1;
	if (fseek(fp, 0, SEEK_END) || (length = ftell(fp)) <= 0 || length > 65536
		|| fseek(fp, 0, SEEK_SET) || !(data = malloc(length + 1))) {
		fclose(fp);
		return -1;
	}
	if (fread(data, 1, length, fp) != (size_t)length || ferror(fp) || memchr(data, '\0', length)) {
		fclose(fp);
		free(data);
		return -1;
	}
	fclose(fp);
	data[length] = '\0';
	cursor = data;
	while ((line = strsep(&cursor, "\n"))) {
		if ((read = strchr(line, '#'))) *read = '\0';
		/* WireGuard configuration ignores ASCII whitespace, including CRLF. */
		for (read = write = line; *read; read++)
			if (!isspace((unsigned char)*read)) *write++ = *read;
		*write = '\0';
		if (!line[0]) continue;
		if (!strcasecmp(line, "[Interface]")) {
			if (have_interface || have_peer) goto done;
			have_interface = 1;
			section = 1;
			continue;
		}
		if (!strcasecmp(line, "[Peer]")) {
			/* One router client profile cannot represent multiple peers. */
			if (!have_interface || have_peer) goto done;
			have_peer = 1;
			section = 2;
			continue;
		}
		if (!section || !(value = strchr(line, '='))) goto done;
		*value++ = '\0';
		for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
			if (!strcasecmp(line, fields[i].name)) break;
		/* Reject options that cannot be represented instead of silently dropping them. */
		if (i == sizeof(fields) / sizeof(fields[0]) || fields[i].section != section
			|| (fields[i].seen && !fields[i].multiple)) goto done;
		if (!value[0] && strcmp(fields[i].name, "PresharedKey")) goto done;
		used = strlen(fields[i].value);
		if (used + (used ? 1 : 0) + strlen(value) >= fields[i].capacity) goto done;
		if (used) strcat(fields[i].value, ",");
		strcat(fields[i].value, value);
		fields[i].seen = 1;
	}
	if (!have_peer || !wgconf_key(priv) || !wgconf_key(pub) || (psk[0] && !wgconf_key(psk))
		|| !addr[0] || !allowed[0] || !endpoint[0]
		|| !wgconf_list(addr, 0) || !wgconf_list(allowed, 0)
		|| (dns[0] && !wgconf_list(dns, 1))
		|| (mtu[0] && !wgconf_number(mtu, 65535))) goto done;
	if (!strcasecmp(alive, "off")) strcpy(alive, "0");
	if (alive[0] && !wgconf_number(alive, 65535)) goto done;
	if (endpoint[0] == '[') {
		host = endpoint + 1;
		if (!(port = strchr(host, ']')) || port[1] != ':') goto done;
		*port = '\0';
		port += 2;
		if (inet_pton(AF_INET6, host, address) != 1) goto done;
	} else {
		host = endpoint;
		/* Match wg setconf: an unbracketed IPv6 endpoint uses the last colon. */
		if (!(port = strrchr(host, ':'))) goto done;
		*port++ = '\0';
		if (!wgconf_host(host)) goto done;
	}
	if (!wgconf_number(port, 65535) || strtoul(port, NULL, 10) == 0) goto done;

	/* No parser error above this point has modified any profile settings. */
	snprintf(prefix, sizeof(prefix), "%s%d_", WG_CLIENT_NVRAM_PREFIX, wgc_unit);
	nvram_pf_restore_default("wgc_", prefix);
	/* An omitted wg keepalive means disabled, independent of the UI default. */
	if (!alive[0]) nvram_pf_set(prefix, "alive", "0");
	for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
		if (fields[i].seen && fields[i].nvname)
			nvram_pf_set(prefix, fields[i].nvname, fields[i].value);
	nvram_pf_set(prefix, "ep_addr", host);
	nvram_pf_set(prefix, "ep_port", port);
	result = 0;
done:
	free(data);
	return result;
}

#define WG_DIR_CONF    "/etc/wg"
int is_wgc_connected(int unit)
{
	char ifname[8] = {0};
	char buf[512] = {0};
	char filename[32] = {0};

	snprintf(ifname, sizeof(ifname), "%s%d", WG_CLIENT_IF_PREFIX, unit);
	snprintf(filename, sizeof(filename), "%s/%s_status", WG_DIR_CONF, ifname);
	snprintf(buf, sizeof(buf), "mkdir -m 0700 -p %s && wg show %s |grep handshake > %s 2>&1", WG_DIR_CONF, ifname, filename);
	system(buf);

	memset(buf, 0 , sizeof(buf));
	if (f_read_string(filename, buf, sizeof(buf)) > 0) {
		char *p = strstr(buf, "sec:");
		unsigned long long t = (p) ? strtoull (p + 4, NULL, 0) : 999;
		if (strstr(buf, "Now"))
			return 1;
		else if (t <= 180)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}
#endif


// Imported from rc/wireguard.c, for use in libovpn
#ifdef RTCONFIG_WIREGUARD

#ifdef RTCONFIG_HND_ROUTER
#define BLOG_SKIP_PORT "/proc/blog/skip_wireguard_port"
#define BLOG_SKIP_NET "/proc/blog/skip_wireguard_network"
#define WG_NAME_SKIP_NET "hndnet"
#endif


#if defined(RTCONFIG_HND_ROUTER_AX_6756) || defined(RTCONFIG_BCM_502L07P2) || defined(RTCONFIG_HND_ROUTER_AX_675X) || defined(RTCONFIG_HND_ROUTER_BE_4916)
int _wg_check_same_port(wg_type_t type, int unit, int port)
{
	int i;
	char prefix[16] = {0};

	for (i = 1; i <= WG_SERVER_MAX; i++) {
		if (type == WG_TYPE_SERVER && unit == i)
			continue;
		snprintf(prefix, sizeof(prefix), "%s%d_", WG_SERVER_NVRAM_PREFIX, i);
		if (nvram_pf_get_int(prefix, "enable") && port == nvram_pf_get_int(prefix, "port"))
			return 1;
	}
	for (i = 1; i <= WG_CLIENT_MAX; i++) {
		if (type == WG_TYPE_CLIENT && unit == i)
			continue;
		snprintf(prefix, sizeof(prefix), "%s%d_", WG_CLIENT_NVRAM_PREFIX, i);
		if (nvram_pf_get_int(prefix, "enable") && port == nvram_pf_get_int(prefix, "ep_port"))
			return 1;
	}
	return 0;
}

void hnd_skip_wg_port(int add, int port, wg_port_t type)
{
	char buf[64] = {0};
	char *ctrl = (add) ? "add" : "del";
	char *port_type[] = {"dport", "sport", "either"};

	snprintf(buf, sizeof(buf), "%s %d %s", ctrl, port, port_type[type]);
	f_write_string(BLOG_SKIP_PORT, buf, 0, 0);
}

void hnd_skip_wg_network(int add, const char* net)
{
	char buf[64] = {0};
	char *ctrl = (add) ? "add" : "del";
	int ret;

	if (!net || !*net)
		return;

	if (strchr(net, '/'))
		snprintf(buf, sizeof(buf), "%s %s", ctrl, net);
	else {
		ret = is_valid_ip(net);
		if (ret > 1)
			snprintf(buf, sizeof(buf), "%s %s/128", ctrl, net);
		else if (ret > 0)
			snprintf(buf, sizeof(buf), "%s %s/32", ctrl, net);
	}
	_dprintf("[%s] > %s\n", buf, BLOG_SKIP_NET);
	f_write_string(BLOG_SKIP_NET, buf, 0, 0);
}

void hnd_skip_wg_all_lan(int add)
{
	char path[128] = {0};
	char buf[512] = {0};
	char net[64] = {0}, *next = NULL;

	snprintf(path, sizeof(path), "%s/all_%s", WG_DIR_CONF, WG_NAME_SKIP_NET);

	f_read_string(path, buf, sizeof(buf));
	foreach_44(net, buf, next) {
		hnd_skip_wg_network(0, net);
	}
	unlink(path);

	if (add) {
		get_network_addr_by_ip_prefix(nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"), buf, sizeof(buf));
		hnd_skip_wg_network(add, buf);
		f_write_string(path, buf, 0, 0);

#if 0//RTCONFIG_IPV6
		int v6_service = get_ipv6_service();
		int dhcp_pd = nvram_get_int(ipv6_nvname("ipv6_dhcp_pd"));
		if ((v6_service == IPV6_NATIVE_DHCP && dhcp_pd)
		 || v6_service == IPV6_6IN4 || v6_service == IPV6_MANUAL) {
			snprintf(buf, sizeof(buf), ",%s/%d", nvram_safe_get(ipv6_nvname("ipv6_prefix")), nvram_get_int(ipv6_nvname("ipv6_prefix_length")));
			f_write_string(path, buf, FW_APPEND, 0);
		}
#endif
	}
}
#endif
#endif

#ifdef RTCONFIG_WIREGUARD
extern struct nvram_tuple router_defaults[];

void reset_wgc_setting(int unit){
	struct nvram_tuple *t;
	char varname[32];
	char *cur;

	logmessage("wireguard","Resetting VPN client %d to default settings", unit);

	// Reset vars
	for (t = router_defaults; t->name; t++) {
		if (strncmp(t->name, "wgc_", 4)==0)
		{
			snprintf(varname, sizeof (varname), "wgc%d_%s", unit, t->name+4);
			nvram_unset(varname);
		}
	}
	nvram_commit();
}
#endif

