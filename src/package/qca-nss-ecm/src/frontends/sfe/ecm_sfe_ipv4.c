/*
 **************************************************************************
 * Copyright (c) 2015 The Linux Foundation.  All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/ppp_defs.h>
#include <linux/mroute.h>

#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <net/arp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>
#ifdef ECM_INTERFACE_VLAN_ENABLE
#include <linux/../../net/8021q/vlan.h>
#include <linux/if_vlan.h>
#endif

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_SFE_IPV4_DEBUG_LEVEL

#include <sfe_drv.h>

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_tracker_datagram.h"
#include "ecm_tracker_udp.h"
#include "ecm_tracker_tcp.h"
#include "ecm_db.h"
#include "ecm_classifier_default.h"
#ifdef ECM_CLASSIFIER_NL_ENABLE
#include "ecm_classifier_nl.h"
#endif
#ifdef ECM_CLASSIFIER_HYFI_ENABLE
#include "ecm_classifier_hyfi.h"
#endif
#ifdef ECM_CLASSIFIER_DSCP_ENABLE
#include "ecm_classifier_dscp.h"
#endif
#ifdef ECM_CLASSIFIER_PCC_ENABLE
#include "ecm_classifier_pcc.h"
#endif
#include "ecm_interface.h"
#include "ecm_sfe_ipv4.h"
#include "ecm_sfe_ported_ipv4.h"
#ifdef ECM_MULTICAST_ENABLE
#include "ecm_sfe_multicast_ipv4.h"
#endif
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
#include "ecm_sfe_non_ported_ipv4.h"
#endif

#include "ecm_front_end_common.h"

int ecm_sfe_ipv4_no_action_limit_default = 250;		/* Default no-action limit. */
int ecm_sfe_ipv4_driver_fail_limit_default = 250;		/* Default driver fail limit. */
int ecm_sfe_ipv4_nack_limit_default = 250;			/* Default nack limit. */
int ecm_sfe_ipv4_accelerated_count = 0;			/* Total offloads */
int ecm_sfe_ipv4_pending_accel_count = 0;			/* Total pending offloads issued to the SFE / awaiting completion */
int ecm_sfe_ipv4_pending_decel_count = 0;			/* Total pending deceleration requests issued to the SFE / awaiting completion */

/*
 * Limiting the acceleration of connections.
 *
 * By default there is no acceleration limiting.
 * This means that when ECM has more connections (that can be accelerated) than the acceleration
 * engine will allow the ECM will continue to try to accelerate.
 * In this scenario the acceleration engine will begin removal of existing rules to make way for new ones.
 * When the accel_limit_mode is set to FIXED ECM will not permit more rules to be issued than the engine will allow.
 */
uint32_t ecm_sfe_ipv4_accel_limit_mode = ECM_FRONT_END_ACCEL_LIMIT_MODE_UNLIMITED;

/*
 * Locking of the classifier - concurrency control for file global parameters.
 * NOTE: It is safe to take this lock WHILE HOLDING a feci->lock.  The reverse is NOT SAFE.
 */
DEFINE_SPINLOCK(ecm_sfe_ipv4_lock);			/* Protect against SMP access between netfilter, events and private threaded function. */

/*
 * Management thread control
 */
bool ecm_sfe_ipv4_terminate_pending = false;		/* True when the user has signalled we should quit */

/*
 * SFE driver linkage
 */
struct sfe_drv_ctx_instance *ecm_sfe_ipv4_drv_mgr = NULL;

static unsigned long ecm_sfe_ipv4_accel_cmd_time_avg_samples = 0;	/* Sum of time taken for the set of accel command samples, used to compute average time for an accel command to complete */
static unsigned long ecm_sfe_ipv4_accel_cmd_time_avg_set = 1;	/* How many samples in the set */
static unsigned long ecm_sfe_ipv4_decel_cmd_time_avg_samples = 0;	/* Sum of time taken for the set of accel command samples, used to compute average time for an accel command to complete */
static unsigned long ecm_sfe_ipv4_decel_cmd_time_avg_set = 1;	/* How many samples in the set */

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_sfe_ipv4_dentry;

/*
 * General operational control
 */
static int ecm_sfe_ipv4_stopped = 0;			/* When non-zero further traffic will not be processed */

/*
 * ecm_sfe_ipv4_node_establish_and_ref()
 *	Returns a reference to a node, possibly creating one if necessary.
 *
 * The given_node_addr will be used if provided.
 *
 * Returns NULL on failure.
 */
struct ecm_db_node_instance *ecm_sfe_ipv4_node_establish_and_ref(struct ecm_front_end_connection_instance *feci,
							struct net_device *dev, ip_addr_t addr,
							struct ecm_db_iface_instance *interface_list[], int32_t interface_list_first,
							uint8_t *given_node_addr)
{
	struct ecm_db_node_instance *ni;
	struct ecm_db_node_instance *nni;
	struct ecm_db_iface_instance *ii;
	int i;
	bool done;
	uint8_t node_addr[ETH_ALEN];

	DEBUG_INFO("Establish node for " ECM_IP_ADDR_DOT_FMT "\n", ECM_IP_ADDR_TO_DOT(addr));

	/*
	 * The node is the datalink address, typically a MAC address.
	 * However the node address to use is not always obvious and depends on the interfaces involved.
	 * For example if the interface is PPPoE then we use the MAC of the PPPoE server as we cannot use normal ARP resolution.
	 * Not all hosts have a node address, where there is none, a suitable alternative should be located and is typically based on 'addr'
	 * or some other datalink session information.
	 * It should be, at a minimum, something that ties the host with the interface.
	 *
	 * Iterate from 'inner' to 'outer' interfaces - discover what the node is.
	 */
	memset(node_addr, 0, ETH_ALEN);
	done = false;
	if (given_node_addr) {
		memcpy(node_addr, given_node_addr, ETH_ALEN);
		done = true;
		DEBUG_TRACE("Using given node address: %pM\n", node_addr);
	}
	for (i = ECM_DB_IFACE_HEIRARCHY_MAX - 1; (!done) && (i >= interface_list_first); i--) {
		ecm_db_iface_type_t type;
		ip_addr_t gw_addr = ECM_IP_ADDR_NULL;
		bool on_link = false;
#ifdef ECM_INTERFACE_PPP_ENABLE
		struct ecm_db_interface_info_pppoe pppoe_info;
#endif
		type = ecm_db_connection_iface_type_get(interface_list[i]);
		DEBUG_INFO("Lookup node address, interface @ %d is type: %d\n", i, type);

		switch (type) {

		case ECM_DB_IFACE_TYPE_PPPOE:
#ifdef ECM_INTERFACE_PPP_ENABLE
			/*
			 * Node address is the address of the remote PPPoE server
			 */
			ecm_db_iface_pppoe_session_info_get(interface_list[i], &pppoe_info);
			memcpy(node_addr, pppoe_info.remote_mac, ETH_ALEN);
			done = true;
			break;
#else
			DEBUG_TRACE("PPPoE interface unsupported\n");
			return NULL;
#endif

		case ECM_DB_IFACE_TYPE_SIT:
		case ECM_DB_IFACE_TYPE_TUNIPIP6:
			done = true;
			break;

		case ECM_DB_IFACE_TYPE_VLAN:
#ifdef ECM_INTERFACE_VLAN_ENABLE
			/*
			 * VLAN handled same along with ethernet, lag, bridge etc.
			 */
#else
			DEBUG_TRACE("VLAN interface unsupported\n");
			return NULL;
#endif
		case ECM_DB_IFACE_TYPE_ETHERNET:
		case ECM_DB_IFACE_TYPE_LAG:
		case ECM_DB_IFACE_TYPE_BRIDGE:
		case ECM_DB_IFACE_TYPE_IPSEC_TUNNEL:
			if (!ecm_interface_mac_addr_get(addr, node_addr, &on_link, gw_addr)) {
				DEBUG_TRACE("failed to obtain node address for host " ECM_IP_ADDR_DOT_FMT "\n", ECM_IP_ADDR_TO_DOT(addr));
				ecm_interface_send_arp_request(dev, addr, on_link, gw_addr);

				/*
				 * Unable to get node address at this time.
				 */
				return NULL;
			}

			if (is_multicast_ether_addr(node_addr)) {
				DEBUG_TRACE("multicast node address for host " ECM_IP_ADDR_DOT_FMT ", node_addr: %pM\n", ECM_IP_ADDR_TO_DOT(addr), node_addr);
				return NULL;
			}

			/*
			 * Because we are iterating from inner to outer interface, this interface is the
			 * innermost one that has a node address - take this one.
			 */
			done = true;
			break;
		default:
			/*
			 * Don't know how to handle these.
			 * Just copy some part of the address for now, but keep iterating the interface list
			 * in the hope something recognisable will be seen!
			 * GGG TODO We really need to roll out support for all interface types we can deal with ASAP :-(
			 */
			memcpy(node_addr, (uint8_t *)addr, ETH_ALEN);
		}
	}
	if (!done) {
		DEBUG_INFO("Failed to establish node for " ECM_IP_ADDR_DOT_FMT "\n", ECM_IP_ADDR_TO_DOT(addr));
		return NULL;
	}

	/*
	 * Locate the node
	 */
	ni = ecm_db_node_find_and_ref(node_addr);
	if (ni) {
		DEBUG_TRACE("%p: node established\n", ni);
		return ni;
	}

	/*
	 * No node - establish iface
	 */
	ii = ecm_interface_establish_and_ref(feci, dev);
	if (!ii) {
		DEBUG_WARN("Failed to establish iface\n");
		return NULL;
	}

	/*
	 * No node - create one
	 */
	nni = ecm_db_node_alloc();
	if (!nni) {
		DEBUG_WARN("Failed to establish node\n");
		ecm_db_iface_deref(ii);
		return NULL;
	}

	/*
	 * Add node into the database, atomically to avoid races creating the same thing
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ni = ecm_db_node_find_and_ref(node_addr);
	if (ni) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		ecm_db_node_deref(nni);
		ecm_db_iface_deref(ii);
		return ni;
	}

	ecm_db_node_add(nni, ii, node_addr, NULL, nni);
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Don't need iface instance now
	 */
	ecm_db_iface_deref(ii);

	DEBUG_TRACE("%p: node established\n", nni);
	return nni;
}

/*
 * ecm_sfe_ipv4_host_establish_and_ref()
 *	Returns a reference to a host, possibly creating one if necessary.
 *
 * Returns NULL on failure.
 */
struct ecm_db_host_instance *ecm_sfe_ipv4_host_establish_and_ref(ip_addr_t addr)
{
	struct ecm_db_host_instance *hi;
	struct ecm_db_host_instance *nhi;

	DEBUG_INFO("Establish host for " ECM_IP_ADDR_DOT_FMT "\n", ECM_IP_ADDR_TO_DOT(addr));

	/*
	 * Locate the host
	 */
	hi = ecm_db_host_find_and_ref(addr);
	if (hi) {
		DEBUG_TRACE("%p: host established\n", hi);
		return hi;
	}

	/*
	 * No host - create one
	 */
	nhi = ecm_db_host_alloc();
	if (!nhi) {
		DEBUG_WARN("Failed to establish host\n");
		return NULL;
	}

	/*
	 * Add host into the database, atomically to avoid races creating the same thing
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	hi = ecm_db_host_find_and_ref(addr);
	if (hi) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		ecm_db_host_deref(nhi);
		return hi;
	}

	ecm_db_host_add(nhi, addr, true, NULL, nhi);

	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	DEBUG_TRACE("%p: host established\n", nhi);
	return nhi;
}

/*
 * ecm_sfe_ipv4_mapping_establish_and_ref()
 *	Returns a reference to a mapping, possibly creating one if necessary.
 *
 * Returns NULL on failure.
 */
struct ecm_db_mapping_instance *ecm_sfe_ipv4_mapping_establish_and_ref(ip_addr_t addr, int port)
{
	struct ecm_db_mapping_instance *mi;
	struct ecm_db_mapping_instance *nmi;
	struct ecm_db_host_instance *hi;

	DEBUG_INFO("Establish mapping for " ECM_IP_ADDR_DOT_FMT ":%d\n", ECM_IP_ADDR_TO_DOT(addr), port);

	/*
	 * Locate the mapping
	 */
	mi = ecm_db_mapping_find_and_ref(addr, port);
	if (mi) {
		DEBUG_TRACE("%p: mapping established\n", mi);
		return mi;
	}

	/*
	 * No mapping - establish host existence
	 */
	hi = ecm_sfe_ipv4_host_establish_and_ref(addr);
	if (!hi) {
		DEBUG_WARN("Failed to establish host\n");
		return NULL;
	}

	/*
	 * Create mapping instance
	 */
	nmi = ecm_db_mapping_alloc();
	if (!nmi) {
		ecm_db_host_deref(hi);
		DEBUG_WARN("Failed to establish mapping\n");
		return NULL;
	}

	/*
	 * Add mapping into the database, atomically to avoid races creating the same thing
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	mi = ecm_db_mapping_find_and_ref(addr, port);
	if (mi) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		ecm_db_mapping_deref(nmi);
		ecm_db_host_deref(hi);
		return mi;
	}

	ecm_db_mapping_add(nmi, hi, port, NULL, nmi);

	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Don't need the host instance now - the mapping maintains a reference to it now.
	 */
	ecm_db_host_deref(hi);

	/*
	 * Return the mapping instance
	 */
	DEBUG_INFO("%p: mapping established\n", nmi);
	return nmi;
}

/*
 * ecm_sfe_ipv4_accel_done_time_update()
 *	Record how long the command took to complete, updating average samples
 */
void ecm_sfe_ipv4_accel_done_time_update(struct ecm_front_end_connection_instance *feci)
{
	unsigned long delta;

	/*
	 * How long did it take the command to complete?
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_completed = jiffies;
	delta = feci->stats.cmd_time_completed - feci->stats.cmd_time_begun;
	spin_unlock_bh(&feci->lock);

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ecm_sfe_ipv4_accel_cmd_time_avg_samples += delta;
	ecm_sfe_ipv4_accel_cmd_time_avg_set++;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);
}

/*
 * ecm_sfe_ipv4_deccel_done_time_update()
 *	Record how long the command took to complete, updating average samples
 */
void ecm_sfe_ipv4_decel_done_time_update(struct ecm_front_end_connection_instance *feci)
{
	unsigned long delta;

	/*
	 * How long did it take the command to complete?
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_completed = jiffies;
	delta = feci->stats.cmd_time_completed - feci->stats.cmd_time_begun;
	spin_unlock_bh(&feci->lock);

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ecm_sfe_ipv4_decel_cmd_time_avg_samples += delta;
	ecm_sfe_ipv4_decel_cmd_time_avg_set++;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);
}

/*
 * ecm_sfe_ipv4_assign_classifier()
 *	Instantiate and assign classifier of type upon the connection, also returning it if it could be allocated.
 */
struct ecm_classifier_instance *ecm_sfe_ipv4_assign_classifier(struct ecm_db_connection_instance *ci, ecm_classifier_type_t type)
{
	DEBUG_TRACE("%p: Assign classifier of type: %d\n", ci, type);
	DEBUG_ASSERT(type != ECM_CLASSIFIER_TYPE_DEFAULT, "Must never need to instantiate default type in this way");

#ifdef ECM_CLASSIFIER_PCC_ENABLE
	if (type == ECM_CLASSIFIER_TYPE_PCC) {
		struct ecm_classifier_pcc_instance *pcci;
		pcci = ecm_classifier_pcc_instance_alloc(ci);
		if (!pcci) {
			DEBUG_TRACE("%p: Failed to create Parental Controls classifier\n", ci);
			return NULL;
		}
		DEBUG_TRACE("%p: Created Parental Controls classifier: %p\n", ci, pcci);
		ecm_db_connection_classifier_assign(ci, (struct ecm_classifier_instance *)pcci);
		return (struct ecm_classifier_instance *)pcci;
	}
#endif

#ifdef ECM_CLASSIFIER_NL_ENABLE
	if (type == ECM_CLASSIFIER_TYPE_NL) {
		struct ecm_classifier_nl_instance *cnli;
		cnli = ecm_classifier_nl_instance_alloc(ci);
		if (!cnli) {
			DEBUG_TRACE("%p: Failed to create Netlink classifier\n", ci);
			return NULL;
		}
		DEBUG_TRACE("%p: Created Netlink classifier: %p\n", ci, cnli);
		ecm_db_connection_classifier_assign(ci, (struct ecm_classifier_instance *)cnli);
		return (struct ecm_classifier_instance *)cnli;
	}
#endif

#ifdef ECM_CLASSIFIER_DSCP_ENABLE
	if (type == ECM_CLASSIFIER_TYPE_DSCP) {
		struct ecm_classifier_dscp_instance *cdscpi;
		cdscpi = ecm_classifier_dscp_instance_alloc(ci);
		if (!cdscpi) {
			DEBUG_TRACE("%p: Failed to create DSCP classifier\n", ci);
			return NULL;
		}
		DEBUG_TRACE("%p: Created DSCP classifier: %p\n", ci, cdscpi);
		ecm_db_connection_classifier_assign(ci, (struct ecm_classifier_instance *)cdscpi);
		return (struct ecm_classifier_instance *)cdscpi;
	}
#endif

#ifdef ECM_CLASSIFIER_HYFI_ENABLE
	if (type == ECM_CLASSIFIER_TYPE_HYFI) {
		struct ecm_classifier_hyfi_instance *chfi;
		chfi = ecm_classifier_hyfi_instance_alloc(ci);
		if (!chfi) {
			DEBUG_TRACE("%p: Failed to create HyFi classifier\n", ci);
			return NULL;
		}
		DEBUG_TRACE("%p: Created HyFi classifier: %p\n", ci, chfi);
		ecm_db_connection_classifier_assign(ci, (struct ecm_classifier_instance *)chfi);
		return (struct ecm_classifier_instance *)chfi;
	}
#endif

	// GGG TODO Add other classifier types.
	DEBUG_ASSERT(NULL, "%p: Unsupported type: %d\n", ci, type);
	return NULL;
}

/*
 * ecm_sfe_ipv4_reclassify()
 *	Signal reclassify upon the assigned classifiers.
 *
 * Classifiers that unassigned themselves we TRY to re-instantiate them.
 * Returns false if the function is not able to instantiate all missing classifiers.
 * This function does not release and references to classifiers in the assignments[].
 */
bool ecm_sfe_ipv4_reclassify(struct ecm_db_connection_instance *ci, int assignment_count, struct ecm_classifier_instance *assignments[])
{
	ecm_classifier_type_t classifier_type;
	int i;
	bool full_reclassification = true;

	/*
	 * assignment_count will always be <= the number of classifier types available
	 */
	for (i = 0, classifier_type = ECM_CLASSIFIER_TYPE_DEFAULT; i < assignment_count; ++i, ++classifier_type) {
		ecm_classifier_type_t aci_type;
		struct ecm_classifier_instance *aci;

		aci = assignments[i];
		aci_type = aci->type_get(aci);
		DEBUG_TRACE("%p: Reclassify: %d\n", ci, aci_type);
		aci->reclassify(aci);

		/*
		 * If the connection has a full complement of assigned classifiers then these will match 1:1 with the classifier_type (all in same order).
		 * If not, we have to create the missing ones.
		 */
		if (aci_type == classifier_type) {
			continue;
		}

		/*
		 * Need to instantiate the missing classifier types until we get to the same type as aci_type then we are back in sync to continue reclassification
		 */
		while (classifier_type != aci_type) {
			struct ecm_classifier_instance *naci;
			DEBUG_TRACE("%p: Instantiate missing type: %d\n", ci, classifier_type);
			DEBUG_ASSERT(classifier_type < ECM_CLASSIFIER_TYPES, "Algorithm bad");

			naci = ecm_sfe_ipv4_assign_classifier(ci, classifier_type);
			if (!naci) {
				full_reclassification = false;
			} else {
				naci->deref(naci);
			}

			classifier_type++;
		}
	}

	/*
	 * Add missing types
	 */
	for (; classifier_type < ECM_CLASSIFIER_TYPES; ++classifier_type) {
		struct ecm_classifier_instance *naci;
		DEBUG_TRACE("%p: Instantiate missing type: %d\n", ci, classifier_type);

		naci = ecm_sfe_ipv4_assign_classifier(ci, classifier_type);
		if (!naci) {
			full_reclassification = false;
		} else {
			naci->deref(naci);
		}
	}

	DEBUG_TRACE("%p: reclassify done: %u\n", ci, full_reclassification);
	return full_reclassification;
}

/*
 * ecm_sfe_ipv4_connection_regenerate()
 *	Re-generate a connection.
 *
 * Re-generating a connection involves re-evaluating the interface lists in case interface heirarchies have changed.
 * It also involves the possible triggering of classifier re-evaluation but only if all currently assigned
 * classifiers permit this operation.
 */
void ecm_sfe_ipv4_connection_regenerate(struct ecm_db_connection_instance *ci, ecm_tracker_sender_type_t sender,
							struct net_device *out_dev, struct net_device *out_dev_nat,
							struct net_device *in_dev, struct net_device *in_dev_nat,
							__be16 *layer4hdr)
{
	int i;
	bool reclassify_allowed;
	int32_t to_list_first;
	struct ecm_db_iface_instance *to_list[ECM_DB_IFACE_HEIRARCHY_MAX];
	int32_t to_nat_list_first;
	struct ecm_db_iface_instance *to_nat_list[ECM_DB_IFACE_HEIRARCHY_MAX];
	int32_t from_list_first;
	struct ecm_db_iface_instance *from_list[ECM_DB_IFACE_HEIRARCHY_MAX];
	int32_t from_nat_list_first;
	struct ecm_db_iface_instance *from_nat_list[ECM_DB_IFACE_HEIRARCHY_MAX];
	ip_addr_t ip_src_addr;
	ip_addr_t ip_dest_addr;
	ip_addr_t ip_src_addr_nat;
	ip_addr_t ip_dest_addr_nat;
	int protocol;
	bool is_routed;
	uint8_t src_node_addr[ETH_ALEN];
	uint8_t dest_node_addr[ETH_ALEN];
	uint8_t src_node_addr_nat[ETH_ALEN];
	uint8_t dest_node_addr_nat[ETH_ALEN];
	int assignment_count;
	struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
	struct ecm_front_end_connection_instance *feci;

	DEBUG_INFO("%p: re-gen needed\n", ci);

	/*
	 * We may need to swap the devices around depending on who the sender of the packet that triggered the re-gen is
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_DEST) {
		struct net_device *tmp_dev;

		/*
		 * This is a packet sent by the destination of the connection, i.e. it is a packet issued by the 'from' side of the connection.
		 */
		DEBUG_TRACE("%p: Re-gen swap devs\n", ci);
		tmp_dev = out_dev;
		out_dev = in_dev;
		in_dev = tmp_dev;

		tmp_dev = out_dev_nat;
		out_dev_nat = in_dev_nat;
		in_dev_nat = tmp_dev;
	}

	/*
	 * Update the interface lists - these may have changed, e.g. LAG path change etc.
	 * NOTE: We never have to change the usual mapping->host->node_iface arrangements for each side of the connection (to/from sides)
	 * This is because if these interfaces change then the connection is dead anyway.
	 * But a LAG slave might change the heirarchy the connection is using but the LAG master is still sane.
	 * If any of the new interface heirarchies cannot be created then simply set empty-lists as this will deny
	 * acceleration and ensure that a bad rule cannot be created.
	 * IMPORTANT: The 'sender' defines who has sent the packet that triggered this re-generation
	 */
	protocol = ecm_db_connection_protocol_get(ci);

	is_routed = ecm_db_connection_is_routed_get(ci);

	ecm_db_connection_from_address_get(ci, ip_src_addr);
	ecm_db_connection_from_address_nat_get(ci, ip_src_addr_nat);

	ecm_db_connection_to_address_get(ci, ip_dest_addr);
	ecm_db_connection_to_address_nat_get(ci, ip_dest_addr_nat);

	ecm_db_connection_from_node_address_get(ci, src_node_addr);
	ecm_db_connection_from_nat_node_address_get(ci, src_node_addr_nat);

	ecm_db_connection_to_node_address_get(ci, dest_node_addr);
	ecm_db_connection_to_nat_node_address_get(ci, dest_node_addr_nat);

	feci = ecm_db_connection_front_end_get_and_ref(ci);

	DEBUG_TRACE("%p: Update the 'from' interface heirarchy list\n", ci);
	from_list_first = ecm_interface_heirarchy_construct(feci, from_list, ip_dest_addr, ip_src_addr, 4, protocol, in_dev, is_routed, in_dev, src_node_addr, dest_node_addr, layer4hdr);
	if (from_list_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		goto ecm_ipv4_retry_regen;
	}

	ecm_db_connection_from_interfaces_reset(ci, from_list, from_list_first);
	ecm_db_connection_interfaces_deref(from_list, from_list_first);

	DEBUG_TRACE("%p: Update the 'from NAT' interface heirarchy list\n", ci);
	from_nat_list_first = ecm_interface_heirarchy_construct(feci, from_nat_list, ip_dest_addr, ip_src_addr_nat, 4, protocol, in_dev_nat, is_routed, in_dev_nat, src_node_addr_nat, dest_node_addr_nat, layer4hdr);
	if (from_nat_list_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		goto ecm_ipv4_retry_regen;
	}

	ecm_db_connection_from_nat_interfaces_reset(ci, from_nat_list, from_nat_list_first);
	ecm_db_connection_interfaces_deref(from_nat_list, from_nat_list_first);

	DEBUG_TRACE("%p: Update the 'to' interface heirarchy list\n", ci);
	to_list_first = ecm_interface_heirarchy_construct(feci, to_list, ip_src_addr, ip_dest_addr, 4, protocol, out_dev, is_routed, in_dev, dest_node_addr, src_node_addr, layer4hdr);
	if (to_list_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		goto ecm_ipv4_retry_regen;
	}

	ecm_db_connection_to_interfaces_reset(ci, to_list, to_list_first);
	ecm_db_connection_interfaces_deref(to_list, to_list_first);

	DEBUG_TRACE("%p: Update the 'to NAT' interface heirarchy list\n", ci);
	to_nat_list_first = ecm_interface_heirarchy_construct(feci, to_nat_list, ip_src_addr, ip_dest_addr_nat, 4, protocol, out_dev_nat, is_routed, in_dev, dest_node_addr_nat, src_node_addr_nat, layer4hdr);
	if (to_nat_list_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		goto ecm_ipv4_retry_regen;
	}

	feci->deref(feci);
	ecm_db_connection_to_nat_interfaces_reset(ci, to_nat_list, to_nat_list_first);
	ecm_db_connection_interfaces_deref(to_nat_list, to_nat_list_first);

	/*
	 * Get list of assigned classifiers to reclassify.
	 * Remember: This also includes our default classifier too.
	 */
	assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(ci, assignments);

	/*
	 * All of the assigned classifiers must permit reclassification.
	 */
	reclassify_allowed = true;
	for (i = 0; i < assignment_count; ++i) {
		DEBUG_TRACE("%p: Calling to reclassify: %p, type: %d\n", ci, assignments[i], assignments[i]->type_get(assignments[i]));
		if (!assignments[i]->reclassify_allowed(assignments[i])) {
			DEBUG_TRACE("%p: reclassify denied: %p, by type: %d\n", ci, assignments[i], assignments[i]->type_get(assignments[i]));
			reclassify_allowed = false;
			break;
		}
	}

	/*
	 * Re-generation of state is successful.
	 */
	ecm_db_conection_regeneration_completed(ci);

	/*
	 * Now we action any classifier re-classify
	 */
	if (!reclassify_allowed) {
		/*
		 * Regeneration came to a successful conclusion even though reclassification was denied
		 */
		DEBUG_WARN("%p: re-classify denied\n", ci);

		/*
		 * Release the assignments
		 */
		ecm_db_connection_assignments_release(assignment_count, assignments);
		return;
	}

	/*
	 * Reclassify
	 */
	DEBUG_INFO("%p: reclassify\n", ci);
	if (!ecm_sfe_ipv4_reclassify(ci, assignment_count, assignments)) {
		/*
		 * We could not set up the classifiers to reclassify, it is safer to fail out and try again next time
		 */
		DEBUG_WARN("%p: Regeneration: reclassify failed\n", ci);
		ecm_db_connection_assignments_release(assignment_count, assignments);
		return;
	}
	DEBUG_INFO("%p: reclassify success\n", ci);

	/*
	 * Release the assignments
	 */
	ecm_db_connection_assignments_release(assignment_count, assignments);
	return;

ecm_ipv4_retry_regen:
	feci->deref(feci);
	ecm_db_conection_regeneration_failed(ci);
	return;
}

/*
 * ecm_sfe_ipv4_ip_process()
 *	Process IP datagram skb
 */
static unsigned int ecm_sfe_ipv4_ip_process(struct net_device *out_dev, struct net_device *in_dev,
							uint8_t *src_node_addr, uint8_t *dest_node_addr,
							bool can_accel, bool is_routed, bool is_l2_encap, struct sk_buff *skb)
{
	struct ecm_tracker_ip_header ip_hdr;
        struct nf_conn *ct;
        enum ip_conntrack_info ctinfo;
	struct nf_conntrack_tuple orig_tuple;
	struct nf_conntrack_tuple reply_tuple;
	ecm_db_direction_t ecm_dir;
	ecm_tracker_sender_type_t sender;
	ip_addr_t ip_src_addr;
	ip_addr_t ip_dest_addr;
	ip_addr_t ip_src_addr_nat;
	ip_addr_t ip_dest_addr_nat;
	struct net_device *out_dev_nat;
	struct net_device *in_dev_nat;
	uint8_t *src_node_addr_nat;
	uint8_t *dest_node_addr_nat;

	/*
	 * Obtain the IP header from the skb
	 */
	if (!ecm_tracker_ip_check_header_and_read(&ip_hdr, skb)) {
		DEBUG_WARN("Invalid ip header in skb %p\n", skb);
		return NF_ACCEPT;
	}

	if (ip_hdr.fragmented) {
		DEBUG_TRACE("skb %p is fragmented\n", skb);
		return NF_ACCEPT;
	}

	/*
	 * Extract information, if we have conntrack then use that info as far as we can.
	 */
        ct = nf_ct_get(skb, &ctinfo);
	if (unlikely(!ct)) {
		DEBUG_TRACE("%p: no ct\n", skb);
		ECM_IP_ADDR_TO_NIN4_ADDR(orig_tuple.src.u3.ip, ip_hdr.src_addr);
		ECM_IP_ADDR_TO_NIN4_ADDR(orig_tuple.dst.u3.ip, ip_hdr.dest_addr);
		orig_tuple.dst.protonum = ip_hdr.protocol;
		reply_tuple.src.u3.ip = orig_tuple.dst.u3.ip;
		reply_tuple.dst.u3.ip = orig_tuple.src.u3.ip;
		sender = ECM_TRACKER_SENDER_TYPE_SRC;
	} else {
		if (unlikely(ct == &nf_conntrack_untracked)) {
			DEBUG_TRACE("%p: ct: untracked\n", skb);
			return NF_ACCEPT;
		}

		/*
		 * If the conntrack connection is using a helper (i.e. Application Layer Gateway)
		 * then acceleration is denied (connection needs assistance from HLOS to function)
		 */
		if (nfct_help(ct)) {
			DEBUG_TRACE("%p: Connection has helper\n", ct);
			can_accel = false;
		}

		/*
		 * Extract conntrack connection information
		 */
		DEBUG_TRACE("%p: ct: %p, ctinfo: %x\n", skb, ct, ctinfo);
		orig_tuple = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
		reply_tuple = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
		if (IP_CT_DIR_ORIGINAL == CTINFO2DIR(ctinfo)) {
			sender = ECM_TRACKER_SENDER_TYPE_SRC;
		} else {
			sender = ECM_TRACKER_SENDER_TYPE_DEST;
		}

		/*
		 * Is this a related connection?
		 */
		if ((ctinfo == IP_CT_RELATED) || (ctinfo == IP_CT_RELATED_REPLY)) {
			/*
			 * ct is related to the packet at hand.
			 * We can use the IP src/dest information and the direction information.
			 * We cannot use the protocol information from the ct (typically the packet at hand is ICMP error that is related to the ct we have here).
			 */
			orig_tuple.dst.protonum = ip_hdr.protocol;
			DEBUG_TRACE("%p: related ct, actual protocol: %u\n", skb, orig_tuple.dst.protonum);
		}
	}

#ifdef ECM_MULTICAST_ENABLE
	/*
	 * Check for a multicast Destination address here.
	 */
	ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.dst.u3.ip);
	if (ecm_ip_addr_is_multicast(ip_dest_addr)) {
		DEBUG_TRACE("Multicast, Processing: %p\n", skb);
		return ecm_sfe_multicast_ipv4_connection_process(out_dev,
				in_dev,
				src_node_addr,
				dest_node_addr,
				can_accel, is_routed, skb,
				&ip_hdr,
				ct, sender,
				&orig_tuple, &reply_tuple);
	}
#endif

	/*
	 * Work out if this packet involves NAT or not.
	 * If it does involve NAT then work out if this is an ingressing or egressing packet.
	 */
	if (orig_tuple.src.u3.ip != reply_tuple.dst.u3.ip) {
		/*
		 * Egressing NAT
		 */
		ecm_dir = ECM_DB_DIRECTION_EGRESS_NAT;
	} else if (orig_tuple.dst.u3.ip != reply_tuple.src.u3.ip) {
		/*
		 * Ingressing NAT
		 */
		ecm_dir = ECM_DB_DIRECTION_INGRESS_NAT;
	} else if (is_routed) {
		/*
		 * Non-NAT
		 */
		ecm_dir = ECM_DB_DIRECTION_NON_NAT;
	} else {
		/*
		 * Bridged
		 */
		ecm_dir = ECM_DB_DIRECTION_BRIDGED;
	}

	DEBUG_TRACE("IP Packet ORIGINAL src: %pI4 ORIGINAL dst: %pI4 protocol: %u, sender: %d ecm_dir: %d\n", &orig_tuple.src.u3.ip, &orig_tuple.dst.u3.ip, orig_tuple.dst.protonum, sender, ecm_dir);

	/*
	 * Get IP addressing information.  This same logic is applied when extracting port information too.
	 * This is tricky to do as what we are after is src and destination addressing that is non-nat but we also need the nat information too.
	 * INGRESS connections have their conntrack information reversed!
	 * We have to keep in mind the connection direction AND the packet direction in order to be able to work out what is what.
	 *
	 * ip_src_addr and ip_dest_addr MUST always be the NON-NAT endpoint addresses and reflect PACKET direction and not connection direction 'dir'.
	 *
	 * Examples 1 through 4 cater for NAT and NON-NAT in the INGRESS or EGRESS cases.
	 *
	 * Example 1:
	 * An 'original' direction packet to an egress connection from br-lan:192.168.0.133:12345 to eth0:80.132.221.34:80 via NAT'ing router mapping eth0:10.10.10.30:33333 looks like:
	 *	orig_tuple->src == 192.168.0.133:12345		This becomes ip_src_addr
	 *	orig_tuple->dst == 80.132.221.34:80		This becomes ip_dest_addr
	 *	reply_tuple->src == 80.132.221.34:80		This becomes ip_dest_addr_nat
	 *	reply_tuple->dest == 10.10.10.30:33333		This becomes ip_src_addr_nat
	 *
	 *	in_dev would be br-lan - i.e. the device of ip_src_addr
	 *	out_dev would be eth0 - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth0 - i.e. out_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth0 - i.e. out_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN_PC======BR-LAN___ETH0====X====WAN_PC
	 *
	 *	src_node_addr refers to node address of of ip_src_addr_nat
	 *	src_node_addr_nat is set to src_node_addr
	 *	src_node_addr is then set to NULL as there is no node address available here for ip_src_addr
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is the node of ip_dest_addr_nat which is the same as dest_node_addr
	 *
	 * Example 2:
	 * However an 'original' direction packet to an ingress connection from eth0:80.132.221.34:3321 to a LAN host (e.g. via DMZ) br-lan@192.168.0.133:12345 via NAT'ing router mapping eth0:10.10.10.30:12345 looks like:
	 *	orig_tuple->src == 80.132.221.34:3321		This becomes ip_src_addr
	 *	orig_tuple->dst == 10.10.10.30:12345		This becomes ip_dest_addr_nat
	 *	reply_tuple->src == 192.168.0.133:12345		This becomes ip_dest_addr
	 *	reply_tuple->dest == 80.132.221.34:3321		This becomes ip_src_addr_nat
	 *
	 *	in_dev would be eth0 - i.e. the device of ip_src_addr
	 *	out_dev would be br-lan - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth0 - i.e. in_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth0 - i.e. in_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN_PC===X===BR-LAN___ETH0========WAN_PC
	 *
	 *	src_node_addr refers to node address of br-lan which is not useful
	 *	src_node_addr_nat AND src_node_addr become NULL
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is set to NULL
	 *
	 * When dealing with reply packets this confuses things even more.  Reply packets to the above two examples are as follows:
	 *
	 * Example 3:
	 * A 'reply' direction packet to the egress connection above:
	 *	orig_tuple->src == 192.168.0.133:12345		This becomes ip_dest_addr
	 *	orig_tuple->dst == 80.132.221.34:80		This becomes ip_src_addr
	 *	reply_tuple->src == 80.132.221.34:80		This becomes ip_src_addr_nat
	 *	reply_tuple->dest == 10.10.10.30:33333		This becomes ip_dest_addr_nat
	 *
	 *	in_dev would be eth0 - i.e. the device of ip_src_addr
	 *	out_dev would be br-lan - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth0 - i.e. in_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth0 - i.e. in_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN_PC===X===BR-LAN___ETH0========WAN_PC
	 *
	 *	src_node_addr refers to node address of br-lan which is not useful
	 *	src_node_addr_nat AND src_node_addr become NULL
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is set to NULL
	 *
	 * Example 4:
	 * A 'reply' direction packet to the ingress connection above:
	 *	orig_tuple->src == 80.132.221.34:3321		This becomes ip_dest_addr
	 *	orig_tuple->dst == 10.10.10.30:12345		This becomes ip_src_addr_nat
	 *	reply_tuple->src == 192.168.0.133:12345		This becomes ip_src_addr
	 *	reply_tuple->dest == 80.132.221.34:3321		This becomes ip_dest_addr_nat
	 *
	 *	in_dev would be br-lan - i.e. the device of ip_src_addr
	 *	out_dev would be eth0 - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth0 - i.e. out_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth0 - i.e. out_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN_PC======BR-LAN___ETH0====X====WAN_PC
	 *
	 *	src_node_addr refers to node address of ip_src_addr_nat
	 *	src_node_addr_nat is set to src_node_addr
	 *	src_node_addr becomes NULL
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is set to dest_node_addr also.
	 *
	 * The following examples are for BRIDGED cases:
	 *
	 * Example 5:
	 * An 'original' direction packet to an bridged connection from eth1:192.168.0.133:12345 to eth2:192.168.0.244:80 looks like:
	 *	orig_tuple->src == 192.168.0.133:12345		This becomes ip_src_addr
	 *	orig_tuple->dst == 192.168.0.244:80		This becomes ip_dest_addr
	 *	reply_tuple->src == 192.168.0.244:80		This becomes ip_dest_addr_nat
	 *	reply_tuple->dest == 192.168.0.133:12345	This becomes ip_src_addr_nat
	 *
	 *	in_dev would be eth1 - i.e. the device of ip_src_addr
	 *	out_dev would be eth2 - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth1 - i.e. in_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth2 - i.e. out_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN PC======ETH1___ETH2====X====LAN PC
	 *
	 *	src_node_addr refers to node address of ip_src_addr
	 *	src_node_addr_nat is set to src_node_addr
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is set to dest_node_addr
	 *
	 * Example 6:
	 * An 'reply' direction packet to the bridged connection above:
	 *	orig_tuple->src == 192.168.0.133:12345		This becomes ip_dest_addr
	 *	orig_tuple->dst == 192.168.0.244:80		This becomes ip_src_addr
	 *	reply_tuple->src == 192.168.0.244:80		This becomes ip_src_addr_nat
	 *	reply_tuple->dest == 192.168.0.133:12345	This becomes ip_dest_addr_nat
	 *
	 *	in_dev would be eth2 - i.e. the device of ip_src_addr
	 *	out_dev would be eth1 - i.e. the device of ip_dest_addr
	 *	in_dev_nat would be eth2 - i.e. in_dev, the device of ip_src_addr_nat
	 *	out_dev_nat would be eth1 - i.e. out_dev, the device of ip_dest_addr_nat
	 *
	 *	From a Node address perspective we are at position X in the following topology:
	 *	LAN PC===X===ETH1___ETH2========LAN PC
	 *
	 *	src_node_addr refers to node address of ip_src_addr
	 *	src_node_addr_nat is set to src_node_addr
	 *
	 *	dest_node_addr refers to node address of ip_dest_addr
	 *	dest_node_addr_nat is set to dest_node_addr
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		if ((ecm_dir == ECM_DB_DIRECTION_EGRESS_NAT) || (ecm_dir == ECM_DB_DIRECTION_NON_NAT)) {
			/*
			 * Example 1
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat = out_dev;
			out_dev_nat = out_dev;

			src_node_addr_nat = src_node_addr;
			src_node_addr = NULL;

			dest_node_addr_nat = dest_node_addr;
		} else if (ecm_dir == ECM_DB_DIRECTION_INGRESS_NAT) {
			/*
			 * Example 2
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat = in_dev;
			out_dev_nat = in_dev;

			src_node_addr = NULL;
			src_node_addr_nat = NULL;

			dest_node_addr_nat = NULL;
		} else if (ecm_dir == ECM_DB_DIRECTION_BRIDGED) {
			/*
			 * Example 5
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat = in_dev;
			out_dev_nat = out_dev;

			src_node_addr_nat = src_node_addr;

			dest_node_addr_nat = dest_node_addr;
		} else {
			DEBUG_ASSERT(false, "Unhandled ecm_dir: %d\n", ecm_dir);
		}
	} else {
		if ((ecm_dir == ECM_DB_DIRECTION_EGRESS_NAT) || (ecm_dir == ECM_DB_DIRECTION_NON_NAT)) {
			/*
			 * Example 3
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat  = in_dev;
			out_dev_nat = in_dev;

			src_node_addr = NULL;
			src_node_addr_nat = NULL;

			dest_node_addr_nat = NULL;
		} else if (ecm_dir == ECM_DB_DIRECTION_INGRESS_NAT) {
			/*
			 * Example 4
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat = out_dev;
			out_dev_nat = out_dev;

			src_node_addr_nat = src_node_addr;
			src_node_addr = NULL;

			dest_node_addr_nat = dest_node_addr;
		} else if (ecm_dir == ECM_DB_DIRECTION_BRIDGED) {
			/*
			 * Example 6
			 */
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr, orig_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr, orig_tuple.dst.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_src_addr_nat, reply_tuple.src.u3.ip);
			ECM_NIN4_ADDR_TO_IP_ADDR(ip_dest_addr_nat, reply_tuple.dst.u3.ip);

			in_dev_nat  = in_dev;
			out_dev_nat = out_dev;

			src_node_addr_nat = src_node_addr;

			dest_node_addr_nat = dest_node_addr;
		} else {
			DEBUG_ASSERT(false, "Unhandled ecm_dir: %d\n", ecm_dir);
		}
	}

	/*
	 * Non-unicast source or destination packets are ignored
	 * NOTE: Only need to check the non-nat src/dest addresses here.
	 */
	if (unlikely(ecm_ip_addr_is_non_unicast(ip_dest_addr))) {
		DEBUG_TRACE("skb %p non-unicast daddr " ECM_IP_ADDR_DOT_FMT "\n", skb, ECM_IP_ADDR_TO_DOT(ip_dest_addr));
		return NF_ACCEPT;
	}
	if (unlikely(ecm_ip_addr_is_non_unicast(ip_src_addr))) {
		DEBUG_TRACE("skb %p non-unicast saddr " ECM_IP_ADDR_DOT_FMT "\n", skb, ECM_IP_ADDR_TO_DOT(ip_src_addr));
		return NF_ACCEPT;
	}

	/*
	 * Process IP specific protocol
	 * TCP and UDP are the most likliest protocols.
	 */
	if (likely(orig_tuple.dst.protonum == IPPROTO_TCP) || likely(orig_tuple.dst.protonum == IPPROTO_UDP)) {
		return ecm_sfe_ported_ipv4_process(out_dev, out_dev_nat,
				in_dev, in_dev_nat,
				src_node_addr, src_node_addr_nat,
				dest_node_addr, dest_node_addr_nat,
				can_accel, is_routed, is_l2_encap, skb,
				&ip_hdr,
				ct, sender, ecm_dir,
				&orig_tuple, &reply_tuple,
				ip_src_addr, ip_dest_addr, ip_src_addr_nat, ip_dest_addr_nat);
	}
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	return ecm_sfe_non_ported_ipv4_process(out_dev, out_dev_nat,
				in_dev, in_dev_nat,
				src_node_addr, src_node_addr_nat,
				dest_node_addr, dest_node_addr_nat,
				can_accel, is_routed, is_l2_encap, skb,
				&ip_hdr,
				ct, sender, ecm_dir,
				&orig_tuple, &reply_tuple,
				ip_src_addr, ip_dest_addr, ip_src_addr_nat, ip_dest_addr_nat);
#else
	return NF_ACCEPT;
#endif
}

/*
 * ecm_sfe_ipv4_post_routing_hook()
 *	Called for IP packets that are going out to interfaces after IP routing stage.
 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,6,0))
static unsigned int ecm_sfe_ipv4_post_routing_hook(unsigned int hooknum,
				struct sk_buff *skb,
				const struct net_device *in_unused,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#else
static unsigned int ecm_sfe_ipv4_post_routing_hook(const struct nf_hook_ops *ops,
				struct sk_buff *skb,
				const struct net_device *in_unused,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#endif
{
	struct net_device *in;
	bool can_accel = true;
	unsigned int result;

	DEBUG_TRACE("%p: Routing: %s\n", out, out->name);

	if (ecm_front_end_acceleration_rejected(skb)) {
		DEBUG_TRACE("Acceleration rejected\n");
		return NF_ACCEPT;
	}

	/*
	 * If operations have stopped then do not process packets
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	if (unlikely(ecm_sfe_ipv4_stopped)) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		DEBUG_TRACE("Front end stopped\n");
		return NF_ACCEPT;
	}
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Don't process broadcast or multicast
	 */
	if (skb->pkt_type == PACKET_BROADCAST) {
		DEBUG_TRACE("Broadcast, ignoring: %p\n", skb);
		return NF_ACCEPT;
	}

#ifndef ECM_MULTICAST_ENABLE
	if (skb->pkt_type == PACKET_MULTICAST) {
		DEBUG_TRACE("Multicast, ignoring: %p\n", skb);
		return NF_ACCEPT;
	}
#endif

#ifdef ECM_INTERFACE_PPP_ENABLE
	/*
	 * skip l2tp/pptp because we don't accelerate them
	 */
	if (ecm_interface_skip_l2tp_pptp(skb, out)) {
		return NF_ACCEPT;
	}
#endif

	/*
	 * Identify interface from where this packet came
	 */
	in = dev_get_by_index(&init_net, skb->skb_iif);
	if (unlikely(!in)) {
		/*
		 * Locally sourced packets are not processed in ECM.
		 */
		return NF_ACCEPT;
	}

	DEBUG_TRACE("Post routing process skb %p, out: %p (%s), in: %p (%s)\n", skb, out, out->name, in, in->name);
	result = ecm_sfe_ipv4_ip_process((struct net_device *)out, in, NULL, NULL,
							can_accel, true, false, skb);
	dev_put(in);
	return result;
}

/*
 * ecm_sfe_ipv4_pppoe_bridge_process()
 *	Called for PPPoE session packets that are going
 *	out to one of the bridge physical interfaces.
 */
static unsigned int ecm_sfe_ipv4_pppoe_bridge_process(struct net_device *out,
						     struct net_device *in,
						     struct ethhdr *skb_eth_hdr,
						     bool can_accel,
						     struct sk_buff *skb)
{
	unsigned int result = NF_ACCEPT;
	struct pppoe_hdr *ph = pppoe_hdr(skb);
	uint16_t ppp_proto = *(uint16_t *)ph->tag;
	uint32_t encap_header_len = 0;

	ppp_proto = ntohs(ppp_proto);
	if (ppp_proto != PPP_IP) {
		return NF_ACCEPT;
	}

	encap_header_len = ecm_front_end_l2_encap_header_len(skb);
	ecm_front_end_pull_l2_encap_header(skb, encap_header_len);
	skb->protocol = htons(ETH_P_IP);

	result = ecm_sfe_ipv4_ip_process(out, in, skb_eth_hdr->h_source,
					 skb_eth_hdr->h_dest, can_accel,
					 false, true, skb);

	ecm_front_end_push_l2_encap_header(skb, encap_header_len);
	skb->protocol = htons(ETH_P_PPP_SES);

	return result;
}

/*
 * ecm_sfe_ipv4_bridge_post_routing_hook()
 *	Called for packets that are going out to one of the bridge physical interfaces.
 *
 * These may have come from another bridged interface or from a non-bridged interface.
 * Conntrack information may be available or not if this skb is bridged.
 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,6,0))
static unsigned int ecm_sfe_ipv4_bridge_post_routing_hook(unsigned int hooknum,
					struct sk_buff *skb,
					const struct net_device *in_unused,
					const struct net_device *out,
					int (*okfn)(struct sk_buff *))
#else
static unsigned int ecm_sfe_ipv4_bridge_post_routing_hook(const struct nf_hook_ops *ops,
					struct sk_buff *skb,
					const struct net_device *in_unused,
					const struct net_device *out,
					int (*okfn)(struct sk_buff *))
#endif
{
	struct ethhdr *skb_eth_hdr;
	uint16_t eth_type;
	struct net_device *bridge;
	struct net_device *in;
	bool can_accel = true;
	unsigned int result;

	DEBUG_TRACE("%p: Bridge: %s\n", out, out->name);

	/*
	 * If operations have stopped then do not process packets
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	if (unlikely(ecm_sfe_ipv4_stopped)) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		DEBUG_TRACE("Front end stopped\n");
		return NF_ACCEPT;
	}
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Don't process broadcast or multicast
	 */
	if (skb->pkt_type == PACKET_BROADCAST) {
		DEBUG_TRACE("Broadcast, ignoring: %p\n", skb);
		return NF_ACCEPT;
	}

#ifndef ECM_MULTICAST_ENABLE
	if (skb->pkt_type == PACKET_MULTICAST) {
		DEBUG_TRACE("Multicast, ignoring: %p\n", skb);
		return NF_ACCEPT;
	}
#endif

#ifdef ECM_INTERFACE_PPP_ENABLE
	/*
	 * skip l2tp/pptp because we don't accelerate them
	 */
	if (ecm_interface_skip_l2tp_pptp(skb, out)) {
		return NF_ACCEPT;
	}
#endif

	/*
	 * Check packet is an IP Ethernet packet
	 */
	skb_eth_hdr = eth_hdr(skb);
	if (!skb_eth_hdr) {
		DEBUG_TRACE("%p: Not Eth\n", skb);
		return NF_ACCEPT;
	}
	eth_type = ntohs(skb_eth_hdr->h_proto);
	if (unlikely((eth_type != 0x0800) && (eth_type != ETH_P_PPP_SES))) {
		DEBUG_TRACE("%p: Not IP/PPPoE session\n", skb);
		return NF_ACCEPT;
	}

	/*
	 * Identify interface from where this packet came.
	 * There are three scenarios to consider here:
	 * 1. Packet came from a local source.
	 *	Ignore - local is not handled.
	 * 2. Packet came from a routed path.
	 *	Ignore - it was handled in INET post routing.
	 * 3. Packet is bridged from another port.
	 *	Process.
	 *
	 * Begin by identifying case 1.
	 * NOTE: We are given 'out' (which we implicitly know is a bridge port) so out interface's master is the 'bridge'.
	 */
	bridge = ecm_interface_get_and_hold_dev_master((struct net_device *)out);
	DEBUG_ASSERT(bridge, "Expected bridge\n");
	in = dev_get_by_index(&init_net, skb->skb_iif);
	if  (!in) {
		/*
		 * Case 1.
		 */
		DEBUG_TRACE("Local traffic: %p, ignoring traffic to bridge: %p (%s) \n", skb, bridge, bridge->name);
		dev_put(bridge);
		return NF_ACCEPT;
	}
	dev_put(in);

	/*
	 * Case 2:
	 *	For routed packets the skb will have the src mac matching the bridge mac.
	 * Case 3:
	 *	If the packet was not local (case 1) or routed (case 2) then we process.
	 */
	in = br_port_dev_get(bridge, skb_eth_hdr->h_source);
	if (!in) {
		DEBUG_TRACE("skb: %p, no in device for bridge: %p (%s)\n", skb, bridge, bridge->name);
		dev_put(bridge);
		return NF_ACCEPT;
	}
	if (in == out) {
		DEBUG_TRACE("skb: %p, bridge: %p (%s), port bounce on %p (%s)\n", skb, bridge, bridge->name, out, out->name);
		dev_put(in);
		dev_put(bridge);
		return NF_ACCEPT;
	}
	if (!ecm_mac_addr_equal(skb_eth_hdr->h_source, bridge->dev_addr)) {
		/*
		 * Case 2: Routed trafffic would be handled by the INET post routing.
		 */
		DEBUG_TRACE("skb: %p, Ignoring routed packet to bridge: %p (%s)\n", skb, bridge, bridge->name);
		dev_put(in);
		dev_put(bridge);
		return NF_ACCEPT;
	}

	if (!is_multicast_ether_addr(skb_eth_hdr->h_dest)) {
		/*
		 * Process the packet, if we have this mac address in the fdb table.
		 * TODO: For the kernel versions later than 3.6.x, the API needs vlan id.
		 * 	 For now, we are passing 0, but this needs to be handled later.
		 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,6,0))
		if (!br_fdb_has_entry((struct net_device *)out, skb_eth_hdr->h_dest)) {
#else
		if (!br_fdb_has_entry((struct net_device *)out, skb_eth_hdr->h_dest, 0)) {
#endif
			DEBUG_WARN("skb: %p, No fdb entry for this mac address %pM in the bridge: %p (%s)\n",
					skb, skb_eth_hdr->h_dest, bridge, bridge->name);
			dev_put(in);
			dev_put(bridge);
			return NF_ACCEPT;
		}
	}
	DEBUG_TRACE("Bridge process skb: %p, bridge: %p (%s), In: %p (%s), Out: %p (%s)\n",
			skb, bridge, bridge->name, in, in->name, out, out->name);

	if (unlikely(eth_type != 0x0800)) {
		result = ecm_sfe_ipv4_pppoe_bridge_process((struct net_device *)out, in, skb_eth_hdr, can_accel, skb);
		dev_put(in);
		dev_put(bridge);
		return result;
	}

	result = ecm_sfe_ipv4_ip_process((struct net_device *)out, in,
				skb_eth_hdr->h_source, skb_eth_hdr->h_dest, can_accel, false, false, skb);


	dev_put(in);
	dev_put(bridge);
	return result;
}

/*
 * ecm_sfe_ipv4_stats_sync_callback()
 *	Callback handler from the sfe driver.
 */
static void ecm_sfe_ipv4_stats_sync_callback(void *app_data, struct sfe_ipv4_msg *nim)
{
	struct sfe_ipv4_conn_sync *sync;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conntrack_tuple tuple;
	struct nf_conn *ct;
	struct nf_conn_counter *acct;
	struct ecm_db_connection_instance *ci;
	struct ecm_front_end_connection_instance *feci;
	struct neighbour *neigh;
	ip_addr_t flow_ip;
	ip_addr_t return_ip_xlate;
	ip_addr_t return_ip;
	struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
	int aci_index;
	int assignment_count;
	struct ecm_classifier_rule_sync class_sync;

	/*
	 * Only respond to sync messages
	 */
	if (nim->cm.type != SFE_RX_CONN_STATS_SYNC_MSG) {
		DEBUG_TRACE("Ignoring nim: %p - not sync: %d", nim, nim->cm.type);
		return;
	}
	sync = &nim->msg.conn_stats;

	/*
	 * Look up ecm connection with a view to synchronising the connection, classifier and data tracker.
	 * Note that we use _xlate versions for destination - for egressing connections this would be the wan IP address,
	 * but for ingressing this would be the LAN side (non-nat'ed) address and is what we need for lookup of our connection.
	 */
	DEBUG_INFO("%p: SFE Sync, lookup connection using\n"
			"Protocol: %d\n" \
			"src_addr: %pI4n:%d\n" \
			"dest_addr: %pI4n:%d\n",
			sync,
			(int)sync->protocol,
			&sync->flow_ip, (int)sync->flow_ident,
			&sync->return_ip_xlate, (int)sync->return_ident_xlate);

	ECM_NIN4_ADDR_TO_IP_ADDR(flow_ip, sync->flow_ip);
	ECM_NIN4_ADDR_TO_IP_ADDR(return_ip_xlate, sync->return_ip_xlate);
	ECM_NIN4_ADDR_TO_IP_ADDR(return_ip, sync->return_ip);

#ifdef ECM_MULTICAST_ENABLE
	/*
	 * Check for multicast flow
	 */
	if (ecm_ip_addr_is_multicast(return_ip)) {
		ci = ecm_db_connection_find_and_ref(flow_ip, return_ip, sync->protocol, (int)ntohs(sync->flow_ident), (int)ntohs(sync->return_ident));
	} else {
		ci = ecm_db_connection_find_and_ref(flow_ip, return_ip_xlate, sync->protocol, (int)ntohs(sync->flow_ident), (int)ntohs(sync->return_ident_xlate));
	}
#else
	ci = ecm_db_connection_find_and_ref(flow_ip, return_ip_xlate, sync->protocol, (int)ntohs(sync->flow_ident), (int)ntohs(sync->return_ident_xlate));
#endif
	if (!ci) {
		DEBUG_TRACE("%p: SFE Sync: no connection\n", sync);
		goto sync_conntrack;
	}
	DEBUG_TRACE("%p: Sync conn %p\n", sync, ci);

	/*
	 * Keep connection alive and updated
	 */
	if (!ecm_db_connection_defunct_timer_touch(ci)) {
		ecm_db_connection_deref(ci);
		goto sync_conntrack;
	}

	/*
	 * Get the front end instance
	 */
	feci = ecm_db_connection_front_end_get_and_ref(ci);

	if (sync->flow_tx_packet_count || sync->return_tx_packet_count) {
		DEBUG_TRACE("%p: flow_rx_packet_count: %u, flow_rx_byte_count: %u, return_rx_packet_count: %u, return_rx_byte_count: %u\n",
				ci, sync->flow_rx_packet_count, sync->flow_rx_byte_count, sync->return_rx_packet_count, sync->return_rx_byte_count);
#ifdef ECM_MULTICAST_ENABLE
		if (ecm_ip_addr_is_multicast(return_ip)) {
			/*
			 * The amount of data *sent* by the ECM multicast connection 'from' side is the amount the SFE has *received* in the 'flow' direction.
			 */
			ecm_db_multicast_connection_data_totals_update(ci, true, sync->flow_rx_byte_count, sync->flow_rx_packet_count);
			ecm_db_multicast_connection_data_totals_update(ci, false, sync->return_rx_byte_count, sync->return_rx_packet_count);
			ecm_db_multicast_connection_interface_heirarchy_stats_update(ci, sync->flow_rx_byte_count, sync->flow_rx_packet_count);

			/*
			 * As packets have been accelerated we have seen some action.
			 */
			feci->action_seen(feci);

			/*
			 * Update IP multicast routing cache stats
			 */
			ipmr_mfc_stats_update(&init_net, htonl(flow_ip[0]), htonl(return_ip[0]), sync->flow_rx_packet_count,
										 sync->flow_rx_byte_count, sync->flow_rx_packet_count, sync->flow_rx_byte_count);
		} else {
			/*
			 * The amount of data *sent* by the ECM connection 'from' side is the amount the SFE has *received* in the 'flow' direction.
			 */
			ecm_db_connection_data_totals_update(ci, true, sync->flow_rx_byte_count, sync->flow_rx_packet_count);

			/*
			 * The amount of data *sent* by the ECM connection 'to' side is the amount the SFE has *received* in the 'return' direction.
			 */
			ecm_db_connection_data_totals_update(ci, false, sync->return_rx_byte_count, sync->return_rx_packet_count);

			/*
			 * As packets have been accelerated we have seen some action.
			 */
			feci->action_seen(feci);

		}

#else
		/*
		 * The amount of data *sent* by the ECM connection 'from' side is the amount the SFE has *received* in the 'flow' direction.
		 */
		ecm_db_connection_data_totals_update(ci, true, sync->flow_rx_byte_count, sync->flow_rx_packet_count);

		/*
		 * The amount of data *sent* by the ECM connection 'to' side is the amount the SFE has *received* in the 'return' direction.
		 */
		ecm_db_connection_data_totals_update(ci, false, sync->return_rx_byte_count, sync->return_rx_packet_count);

		/*
		 * As packets have been accelerated we have seen some action.
		 */
		feci->action_seen(feci);

#endif
	}

	/*
	 * Copy the sync data to the classifier sync structure to
	 * update the classifiers' stats.
	 */
	class_sync.flow_tx_packet_count = sync->flow_tx_packet_count;
	class_sync.return_tx_packet_count = sync->return_tx_packet_count;
	class_sync.reason = sync->reason;

	/*
	 * Sync assigned classifiers
	 */
	assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(ci, assignments);
	for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
		struct ecm_classifier_instance *aci;
		aci = assignments[aci_index];
		DEBUG_TRACE("%p: sync to: %p, type: %d\n", feci, aci, aci->type_get(aci));
		aci->sync_to_v4(aci, &class_sync);
	}
	ecm_db_connection_assignments_release(assignment_count, assignments);

	switch(sync->reason) {
	case SFE_RULE_SYNC_REASON_DESTROY:
		/*
		 * This is the final sync from the SFE for a connection whose acceleration was
		 * terminated by the ecm.
		 * NOTE: We take no action here since that is performed by the destroy message ack.
		 */
		DEBUG_INFO("%p: ECM initiated final sync seen: %d\n", ci, sync->reason);
		break;
	case SFE_RULE_SYNC_REASON_FLUSH:
	case SFE_RULE_SYNC_REASON_EVICT:
		/*
		 * SFE has ended acceleration without instruction from the ECM.
		 */
		DEBUG_INFO("%p: SFE Initiated final sync seen: %d cause:%d\n", ci, sync->reason, sync->cause);

		/*
		 * SFE Decelerated the connection
		 */
		feci->accel_ceased(feci);
		break;
	default:

		/*
		 * Update the neighbour entry for source IP address
		 */
		neigh = ecm_interface_ipv4_neigh_get(flow_ip);
		if (!neigh) {
			DEBUG_WARN("Neighbour entry for %pI4n not found\n", &sync->flow_ip);
		} else {
			DEBUG_TRACE("Neighbour entry for %pI4n update: %p\n", &sync->flow_ip, neigh);
			neigh_update(neigh, NULL, neigh->nud_state, NEIGH_UPDATE_F_WEAK_OVERRIDE);
			neigh_release(neigh);
		}

#ifdef ECM_MULTICAST_ENABLE
		/*
		 * Update the neighbour entry for destination IP address
		 */
		if (!ecm_ip_addr_is_multicast(return_ip)) {
			neigh = ecm_interface_ipv4_neigh_get(return_ip);
			if (!neigh) {
				DEBUG_WARN("Neighbour entry for %pI4n not found\n", &sync->return_ip);
			} else {
				DEBUG_TRACE("Neighbour entry for %pI4n update: %p\n", &sync->return_ip, neigh);
				neigh_update(neigh, NULL, neigh->nud_state, NEIGH_UPDATE_F_WEAK_OVERRIDE);
				neigh_release(neigh);
			}
		}
#else
		/*
		 * Update the neighbour entry for destination IP address
		 */
		neigh = ecm_interface_ipv4_neigh_get(return_ip);
		if (!neigh) {
			DEBUG_WARN("Neighbour entry for %pI4n not found\n", &sync->return_ip);
		} else {
			DEBUG_TRACE("Neighbour entry for %pI4n update: %p\n", &sync->return_ip, neigh);
			neigh_update(neigh, NULL, neigh->nud_state, NEIGH_UPDATE_F_WEAK_OVERRIDE);
			neigh_release(neigh);
		}
#endif
	}

	/*
	 * If connection should be re-generated then we need to force a deceleration
	 */
	if (unlikely(ecm_db_connection_regeneration_required_peek(ci))) {
		DEBUG_TRACE("%p: Connection generation changing, terminating acceleration", ci);
		feci->decelerate(feci);
	}

	feci->deref(feci);
	ecm_db_connection_deref(ci);

sync_conntrack:
	;

	/*
	 * Create a tuple so as to be able to look up a conntrack connection
	 */
	memset(&tuple, 0, sizeof(tuple));
	tuple.src.u3.ip = sync->flow_ip;
	tuple.src.u.all = sync->flow_ident;
	tuple.src.l3num = AF_INET;

	tuple.dst.u3.ip = sync->return_ip;
	tuple.dst.dir = IP_CT_DIR_ORIGINAL;
	tuple.dst.protonum = (uint8_t)sync->protocol;
	tuple.dst.u.all = sync->return_ident;

	DEBUG_TRACE("Conntrack sync, lookup conntrack connection using\n"
			"Protocol: %d\n"
			"src_addr: %pI4:%d\n"
			"dest_addr: %pI4:%d\n",
			(int)tuple.dst.protonum,
			&tuple.src.u3.ip, (int)tuple.src.u.all,
			&tuple.dst.u3.ip, (int)tuple.dst.u.all);

	/*
	 * Look up conntrack connection
	 */
	h = nf_conntrack_find_get(&init_net, NF_CT_DEFAULT_ZONE, &tuple);
	if (!h) {
		DEBUG_WARN("%p: SFE Sync: no conntrack connection\n", sync);
		return;
	}

	ct = nf_ct_tuplehash_to_ctrack(h);
	NF_CT_ASSERT(ct->timeout.data == (unsigned long)ct);
	DEBUG_TRACE("%p: SFE Sync: conntrack connection\n", ct);

	/*
	 * Only update if this is not a fixed timeout
	 */
	if (!test_bit(IPS_FIXED_TIMEOUT_BIT, &ct->status)) {
		unsigned long int delta_jiffies;

		/*
		 * Convert ms ticks from the SFE to jiffies.  We know that inc_ticks is small
		 * and we expect HZ to be small too so we can multiply without worrying about
		 * wrap-around problems.  We add a rounding constant to ensure that the different
		 * time bases don't cause truncation errors.
		 */
		DEBUG_ASSERT(HZ <= 100000, "Bad HZ\n");
		delta_jiffies = ((sync->inc_ticks * HZ) + (MSEC_PER_SEC / 2)) / MSEC_PER_SEC;

		spin_lock_bh(&ct->lock);
		ct->timeout.expires += delta_jiffies;
		spin_unlock_bh(&ct->lock);
	}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,6,0))
	acct = nf_conn_acct_find(ct);
#else
	acct = nf_conn_acct_find(ct)->counter;
#endif
	if (acct) {
		spin_lock_bh(&ct->lock);
		atomic64_add(sync->flow_rx_packet_count, &acct[IP_CT_DIR_ORIGINAL].packets);
		atomic64_add(sync->flow_rx_byte_count, &acct[IP_CT_DIR_ORIGINAL].bytes);

		atomic64_add(sync->return_rx_packet_count, &acct[IP_CT_DIR_REPLY].packets);
		atomic64_add(sync->return_rx_byte_count, &acct[IP_CT_DIR_REPLY].bytes);
		spin_unlock_bh(&ct->lock);
	}

	switch (sync->protocol) {
	case IPPROTO_TCP:
		spin_lock_bh(&ct->lock);
		if (ct->proto.tcp.seen[0].td_maxwin < sync->flow_max_window) {
			ct->proto.tcp.seen[0].td_maxwin = sync->flow_max_window;
		}
		if ((int32_t)(ct->proto.tcp.seen[0].td_end - sync->flow_end) < 0) {
			ct->proto.tcp.seen[0].td_end = sync->flow_end;
		}
		if ((int32_t)(ct->proto.tcp.seen[0].td_maxend - sync->flow_max_end) < 0) {
			ct->proto.tcp.seen[0].td_maxend = sync->flow_max_end;
		}
		if (ct->proto.tcp.seen[1].td_maxwin < sync->return_max_window) {
			ct->proto.tcp.seen[1].td_maxwin = sync->return_max_window;
		}
		if ((int32_t)(ct->proto.tcp.seen[1].td_end - sync->return_end) < 0) {
			ct->proto.tcp.seen[1].td_end = sync->return_end;
		}
		if ((int32_t)(ct->proto.tcp.seen[1].td_maxend - sync->return_max_end) < 0) {
			ct->proto.tcp.seen[1].td_maxend = sync->return_max_end;
		}
		spin_unlock_bh(&ct->lock);
		break;
	}

	/*
	 * Release connection
	 */
	nf_ct_put(ct);
}

/*
 * struct nf_hook_ops ecm_sfe_ipv4_netfilter_hooks[]
 *	Hooks into netfilter packet monitoring points.
 */
static struct nf_hook_ops ecm_sfe_ipv4_netfilter_hooks[] __read_mostly = {
	/*
	 * Post routing hook is used to monitor packets going to interfaces that are NOT bridged in some way, e.g. packets to the WAN.
	 */
	{
		.hook           = ecm_sfe_ipv4_post_routing_hook,
		.owner          = THIS_MODULE,
		.pf             = PF_INET,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP_PRI_NAT_SRC + 1,
	},

	/*
	 * The bridge post routing hook monitors packets going to interfaces that are part of a bridge arrangement.
	 * For example Wireles LAN (WLAN) and Wired LAN (LAN).
	 */
	{
		.hook		= ecm_sfe_ipv4_bridge_post_routing_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_BRIDGE,
		.hooknum	= NF_BR_POST_ROUTING,
		.priority	= NF_BR_PRI_FILTER_OTHER,
	},
};

/*
 * ecm_sfe_ipv4_connection_from_ct_get_and_ref()
 *	Return, if any, a connection given a ct
 */
static struct ecm_db_connection_instance *ecm_sfe_ipv4_connection_from_ct_get_and_ref(struct nf_conn *ct)
{
	struct nf_conntrack_tuple orig_tuple;
	struct nf_conntrack_tuple reply_tuple;
	ip_addr_t host1_addr;
	ip_addr_t host2_addr;
	int host1_port;
	int host2_port;
	int protocol;

	/*
	 * Look up the associated connection for this conntrack connection
	 */
	orig_tuple = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	reply_tuple = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	ECM_NIN4_ADDR_TO_IP_ADDR(host1_addr, orig_tuple.src.u3.ip);
	ECM_NIN4_ADDR_TO_IP_ADDR(host2_addr, reply_tuple.src.u3.ip);
	protocol = orig_tuple.dst.protonum;
	if (protocol == IPPROTO_TCP) {
		host1_port = ntohs(orig_tuple.src.u.tcp.port);
		host2_port = ntohs(reply_tuple.src.u.tcp.port);
	} else if (protocol == IPPROTO_UDP) {
		host1_port = ntohs(orig_tuple.src.u.udp.port);
		host2_port = ntohs(reply_tuple.src.u.udp.port);
	} else if ((protocol == IPPROTO_IPV6) || (protocol == IPPROTO_ESP)) {
		host1_port = 0;
		host2_port = 0;
	} else {
		host1_port = -protocol;
		host2_port = -protocol;
	}

	DEBUG_TRACE("%p: lookup src: " ECM_IP_ADDR_DOT_FMT ":%d, "
		    "dest: " ECM_IP_ADDR_DOT_FMT ":%d, "
		    "protocol %d\n",
		    ct,
		    ECM_IP_ADDR_TO_DOT(host1_addr),
		    host1_port,
		    ECM_IP_ADDR_TO_DOT(host2_addr),
		    host2_port,
		    protocol);

	return ecm_db_connection_find_and_ref(host1_addr,
					      host2_addr,
					      protocol,
					      host1_port,
					      host2_port);
}

/*
 * ecm_sfe_ipv4_conntrack_event_destroy()
 *	Handles conntrack destroy events
 */
static void ecm_sfe_ipv4_conntrack_event_destroy(struct nf_conn *ct)
{
	struct ecm_db_connection_instance *ci;
	struct ecm_front_end_connection_instance *feci;

	DEBUG_INFO("Destroy event for ct: %p\n", ct);

	ci = ecm_sfe_ipv4_connection_from_ct_get_and_ref(ct);
	if (!ci) {
		DEBUG_TRACE("%p: not found\n", ct);
		return;
	}
	DEBUG_INFO("%p: Connection defunct %p\n", ct, ci);

	/*
	 * If this connection is accelerated then we need to issue a destroy command
	 */
	feci = ecm_db_connection_front_end_get_and_ref(ci);
	feci->decelerate(feci);
	feci->deref(feci);

	/*
	 * Force destruction of the connection my making it defunct
	 */
	ecm_db_connection_make_defunct(ci);
	ecm_db_connection_deref(ci);
}

/*
 * ecm_sfe_ipv4_conntrack_event_mark()
 *	Handles conntrack mark events
 */
static void ecm_sfe_ipv4_conntrack_event_mark(struct nf_conn *ct)
{
	struct ecm_db_connection_instance *ci;
	struct ecm_classifier_instance *__attribute__((unused))cls;

	DEBUG_INFO("Mark event for ct: %p\n", ct);

	/*
	 * Ignore transitions to zero
	 */
	if (ct->mark == 0) {
		return;
	}

	ci = ecm_sfe_ipv4_connection_from_ct_get_and_ref(ct);
	if (!ci) {
		DEBUG_TRACE("%p: not found\n", ct);
		return;
	}

#ifdef ECM_CLASSIFIER_NL_ENABLE
	/*
	 * As of now, only the Netlink classifier is interested in conmark changes
	 * GGG TODO Add a classifier method to propagate this information to any and all types of classifier.
	 */
	cls = ecm_db_connection_assigned_classifier_find_and_ref(ci, ECM_CLASSIFIER_TYPE_NL);
	if (cls) {
		ecm_classifier_nl_process_mark((struct ecm_classifier_nl_instance *)cls, ct->mark);
		cls->deref(cls);
	}
#endif

	/*
	 * All done
	 */
	ecm_db_connection_deref(ci);
}

/*
 * ecm_sfe_ipv4_conntrack_event()
 *	Callback event invoked when conntrack connection state changes, currently we handle destroy events to quickly release state
 */
int ecm_sfe_ipv4_conntrack_event(unsigned long events, struct nf_conn *ct)
{
	/*
	 * If operations have stopped then do not process event
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	if (unlikely(ecm_sfe_ipv4_stopped)) {
		DEBUG_WARN("Ignoring event - stopped\n");
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		return NOTIFY_DONE;
	}
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	if (!ct) {
		DEBUG_WARN("Error: no ct\n");
		return NOTIFY_DONE;
	}

	/*
	 * handle destroy events
	 */
	if (events & (1 << IPCT_DESTROY)) {
		DEBUG_TRACE("%p: Event is destroy\n", ct);
		ecm_sfe_ipv4_conntrack_event_destroy(ct);
	}

	/*
	 * handle mark change events
	 */
	if (events & (1 << IPCT_MARK)) {
		DEBUG_TRACE("%p: Event is mark\n", ct);
		ecm_sfe_ipv4_conntrack_event_mark(ct);
	}

	return NOTIFY_DONE;
}
EXPORT_SYMBOL(ecm_sfe_ipv4_conntrack_event);

void ecm_sfe_ipv4_stop(int num)
{
	ecm_sfe_ipv4_stopped = num;
}
EXPORT_SYMBOL(ecm_sfe_ipv4_stop);

/*
 * ecm_sfe_ipv4_get_accel_limit_mode()
 */
static int ecm_sfe_ipv4_get_accel_limit_mode(void *data, u64 *val)
{
	*val = ecm_sfe_ipv4_accel_limit_mode;

	return 0;
}

/*
 * ecm_sfe_ipv4_set_accel_limit_mode()
 */
static int ecm_sfe_ipv4_set_accel_limit_mode(void *data, u64 val)
{
	DEBUG_TRACE("ecm_sfe_ipv4_accel_limit_mode = %x\n", (int)val);

	/*
	 * Check that only valid bits are set.
	 * It's fine for no bits to be set as that suggests no modes are wanted.
	 */
	if (val && (val ^ (ECM_FRONT_END_ACCEL_LIMIT_MODE_FIXED | ECM_FRONT_END_ACCEL_LIMIT_MODE_UNLIMITED))) {
		DEBUG_WARN("ecm_sfe_ipv4_accel_limit_mode = %x bad\n", (int)val);
		return -EINVAL;
	}

	ecm_sfe_ipv4_accel_limit_mode = (int)val;

	return 0;
}

/*
 * Debugfs attribute for accel limit mode.
 */
DEFINE_SIMPLE_ATTRIBUTE(ecm_sfe_ipv4_accel_limit_mode_fops, ecm_sfe_ipv4_get_accel_limit_mode, ecm_sfe_ipv4_set_accel_limit_mode, "%llu\n");

/*
 * ecm_sfe_ipv4_get_accel_cmd_avg_millis()
 */
static ssize_t ecm_sfe_ipv4_get_accel_cmd_avg_millis(struct file *file,
								char __user *user_buf,
								size_t sz, loff_t *ppos)
{
	unsigned long set;
	unsigned long samples;
	unsigned long avg;
	char *buf;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	/*
	 * Operate under our locks.
	 * Compute the average of the samples taken and seed the next set of samples with the result of this one.
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	samples = ecm_sfe_ipv4_accel_cmd_time_avg_samples;
	set = ecm_sfe_ipv4_accel_cmd_time_avg_set;
	ecm_sfe_ipv4_accel_cmd_time_avg_samples /= ecm_sfe_ipv4_accel_cmd_time_avg_set;
	ecm_sfe_ipv4_accel_cmd_time_avg_set = 1;
	avg = ecm_sfe_ipv4_accel_cmd_time_avg_samples;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Convert average jiffies to milliseconds
	 */
	avg *= 1000;
	avg /= HZ;

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "avg=%lu\tsamples=%lu\tset_size=%lu\n", avg, samples, set);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * File operations for accel command average time.
 */
static struct file_operations ecm_sfe_ipv4_accel_cmd_avg_millis_fops = {
	.read = ecm_sfe_ipv4_get_accel_cmd_avg_millis,
};

/*
 * ecm_sfe_ipv4_get_decel_average_millis()
 */
static ssize_t ecm_sfe_ipv4_get_decel_cmd_avg_millis(struct file *file,
								char __user *user_buf,
								size_t sz, loff_t *ppos)
{
	unsigned long set;
	unsigned long samples;
	unsigned long avg;
	char *buf;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	/*
	 * Operate under our locks.
	 * Compute the average of the samples taken and seed the next set of samples with the result of this one.
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	samples = ecm_sfe_ipv4_decel_cmd_time_avg_samples;
	set = ecm_sfe_ipv4_decel_cmd_time_avg_set;
	ecm_sfe_ipv4_decel_cmd_time_avg_samples /= ecm_sfe_ipv4_decel_cmd_time_avg_set;
	ecm_sfe_ipv4_decel_cmd_time_avg_set = 1;
	avg = ecm_sfe_ipv4_decel_cmd_time_avg_samples;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Convert average jiffies to milliseconds
	 */
	avg *= 1000;
	avg /= HZ;

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "avg=%lu\tsamples=%lu\tset_size=%lu\n", avg, samples, set);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * File operations for decel command average time.
 */
static struct file_operations ecm_sfe_ipv4_decel_cmd_avg_millis_fops = {
	.read = ecm_sfe_ipv4_get_decel_cmd_avg_millis,
};

/*
 * ecm_sfe_ipv4_init()
 */
int ecm_sfe_ipv4_init(struct dentry *dentry)
{
	int result = -1;

	DEBUG_INFO("ECM SFE IPv4 init\n");

	ecm_sfe_ipv4_dentry = debugfs_create_dir("ecm_sfe_ipv4", dentry);
	if (!ecm_sfe_ipv4_dentry) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 directory in debugfs\n");
		return result;
	}

	if (!debugfs_create_u32("stop", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_stopped)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 stop file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("no_action_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_no_action_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 no_action_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("driver_fail_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_driver_fail_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 driver_fail_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("nack_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_nack_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 nack_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("accelerated_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_accelerated_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accelerated_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("pending_accel_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_pending_accel_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 pending_accel_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_u32("pending_decel_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_pending_decel_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 pending_decel_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("accel_limit_mode", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_accel_limit_mode_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accel_limit_mode file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("accel_cmd_avg_millis", S_IRUGO, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_accel_cmd_avg_millis_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accel_cmd_avg_millis file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("decel_cmd_avg_millis", S_IRUGO, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_decel_cmd_avg_millis_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 decel_cmd_avg_millis file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_sfe_ported_ipv4_debugfs_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to create ecm ported files in debugfs\n");
		goto task_cleanup;
	}

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	if (!ecm_sfe_non_ported_ipv4_debugfs_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to create ecm non-ported files in debugfs\n");
		goto task_cleanup;
	}
#endif

#ifdef ECM_MULTICAST_ENABLE
	if (!ecm_sfe_multicast_ipv4_debugfs_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to create ecm multicast files in debugfs\n");
		goto task_cleanup;
	}
#endif

	/*
	 * Register netfilter hooks
	 */
	result = nf_register_hooks(ecm_sfe_ipv4_netfilter_hooks, ARRAY_SIZE(ecm_sfe_ipv4_netfilter_hooks));
	if (result < 0) {
		DEBUG_ERROR("Can't register netfilter hooks.\n");
		goto task_cleanup;
	}

#ifdef ECM_MULTICAST_ENABLE
	ecm_sfe_multicast_ipv4_init();
#endif

	/*
	 * Register this module with the simulated sfe driver
	 */
	ecm_sfe_ipv4_drv_mgr = sfe_drv_ipv4_notify_register(ecm_sfe_ipv4_stats_sync_callback, NULL);

	return 0;

task_cleanup:

	debugfs_remove_recursive(ecm_sfe_ipv4_dentry);
	return result;
}
EXPORT_SYMBOL(ecm_sfe_ipv4_init);

/*
 * ecm_sfe_ipv4_exit()
 */
void ecm_sfe_ipv4_exit(void)
{
	DEBUG_INFO("ECM SFE IPv4 Module exit\n");

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ecm_sfe_ipv4_terminate_pending = true;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Stop the network stack hooks
	 */
	nf_unregister_hooks(ecm_sfe_ipv4_netfilter_hooks,
			    ARRAY_SIZE(ecm_sfe_ipv4_netfilter_hooks));

	/*
	 * Unregister from the simulated sfe driver
	 */
	sfe_drv_ipv4_notify_unregister();

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_sfe_ipv4_dentry) {
		debugfs_remove_recursive(ecm_sfe_ipv4_dentry);
	}

#ifdef ECM_MULTICAST_ENABLE
	ecm_sfe_multicast_ipv4_exit();
#endif
}
EXPORT_SYMBOL(ecm_sfe_ipv4_exit);
