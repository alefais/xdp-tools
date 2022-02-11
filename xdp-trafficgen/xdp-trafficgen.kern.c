/* SPDX-License-Identifier: GPL-2.0 */

#define XDP_STATS_MAP_PINNING LIBBPF_PIN_NONE

#include "xdp-trafficgen.h"
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/if_ether.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <xdp/xdp_stats_kern_user.h>
#include <xdp/xdp_stats_kern.h>

char _license[] SEC("license") = "GPL";

const volatile struct trafficgen_config config;
struct trafficgen_state state;

SEC("xdp")
int xdp_redirect_notouch(struct xdp_md *ctx)
{
	return xdp_stats_record_action(ctx, bpf_redirect(config.ifindex_out, 0));
}

SEC("xdp")
int xdp_redirect_update_port(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u16 cur_port, cksum_diff;
	int action = XDP_ABORTED;
	struct udphdr *hdr;

	hdr = data + (sizeof(struct ethhdr) + sizeof(struct ipv6hdr));
	if (hdr + 1 > data_end)
		goto out;

	cur_port = bpf_ntohs(hdr->dest);
	cksum_diff = state.next_port - cur_port;
	if (cksum_diff) {
		hdr->check = bpf_htons(~(~bpf_ntohs(hdr->check) + cksum_diff));
		hdr->dest = bpf_htons(state.next_port);
	}
	if (state.next_port++ >= config.port_start + config.port_range - 1)
		state.next_port = config.port_start;

	action = bpf_redirect(config.ifindex_out, 0);
out:
	return xdp_stats_record_action(ctx, action);
}
