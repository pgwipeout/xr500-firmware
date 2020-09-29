##########################################################################
# Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all copies.
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
##########################################################################

# ###################################################
# # Makefile for the NSS driver
# ###################################################

obj-m += qca-nss-drv.o

#
# List the files that belong to the driver in alphabetical order.
#
qca-nss-drv-objs := \
			nss_cmn.o \
			nss_core.o \
			nss_coredump.o \
			nss_crypto.o \
			nss_dynamic_interface.o \
			nss_gre_redir.o \
			nss_if.o \
			nss_init.o \
			nss_ipsec.o \
			nss_ipv4.o \
			nss_ipv4_reasm.o \
			nss_ipv6.o \
			nss_ipv6_reasm.o \
			nss_lag.o \
			nss_lso_rx.o \
			nss_phys_if.o \
			nss_pm.o \
			nss_sjack.o \
			nss_stats.o \
			nss_tun6rd.o \
			nss_tunipip6.o \
			nss_virt_if.o \
			nss_shaper.o \
			nss_pppoe.o \
			nss_capwap.o \
			nss_eth_rx.o \
			nss_n2h.o \
			nss_data_plane.o \
			nss_log.o \
			nss_wifi.o \
			nss_wifi_vdev.o \
			nss_wifi_if.o

#
# TODO: Deprecated files should be removed before merge
#
qca-nss-drv-objs += \
			nss_tx_rx_virt_if.o

ccflags-y += -I$(obj)/nss_hal/include -I$(obj)/exports -DNSS_DEBUG_LEVEL=0 -DNSS_PKT_STATS_ENABLED=1
ccflags-y += -DNSS_PM_DEBUG_LEVEL=0 -DNSS_PPP_SUPPORT=1

ifneq ($(findstring 3.4, $(KERNELVERSION)),)
NSS_CCFLAGS = -DNSS_DT_SUPPORT=0 -DNSS_FW_DBG_SUPPORT=1 -DNSS_PM_SUPPORT=1 -DNSS_EMPTY_BUFFER_SIZE=1984
else
NSS_CCFLAGS = -DNSS_DT_SUPPORT=1 -DNSS_FW_DBG_SUPPORT=0 -DNSS_PM_SUPPORT=0 -DNSS_EMPTY_BUFFER_SIZE=1792
ccflags-y += -I$(obj)
endif

# Disable Frequency scaling
ifeq "$(NSS_FREQ_SCALE_DISABLE)" "y"
ccflags-y += -DNSS_FREQ_SCALE_SUPPORT=0
else
qca-nss-drv-objs += nss_freq.o
ccflags-y += -DNSS_FREQ_SCALE_SUPPORT=1
endif

ccflags-y += $(NSS_CCFLAGS)

export NSS_CCFLAGS

qca-nss-drv-objs += nss_profiler.o


obj ?= .

qca-nss-drv-objs += nss_hal/ipq806x/nss_hal_pvt.o
ccflags-y += -I$(obj)/nss_hal/ipq806x
