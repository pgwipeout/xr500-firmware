#
# Copyright (C) 2007-2018 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=funjsq
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:=dnsmasq

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)


include $(INCLUDE_DIR)/package.mk

define Package/funjsq
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=The game accelerator with much fun
endef

define Build/Prepare

	$(CP) ./* $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/funjsq/install
	mkdir -p $(1)/data
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,funjsq))
