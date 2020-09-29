CONFIG_QCA_SINGLE_IMG_TREEISH = 4b360057b82b41aefc2f8afb928aa2c227147c0b

export CONFIG_QCA_SINGLE_IMG_TREEISH

single_img_dep = $(obj)u-boot.mbn board/$(BOARDDIR)/qca_single_img/files/apss_proc/out/pack.py

define BuildSingleImg
	$(MAKE) -C tools/qca_single_img/ patch_clean
	cp -R board/"$(BOARDDIR)"/qca_single_img/./ \
			$(obj)tools/qca_single_img/
	$(MAKE) -C tools/qca_single_img/

	@ ### Steps described in QSDK release notes ###
	cp $(obj)u-boot.mbn \
			$(obj)tools/qca_single_img/qsdk-chipcode/common/build/ipq/
	cd $(obj)tools/qca_single_img/qsdk-chipcode/common/build && \
			python update_common_info.py

	cp $(obj)tools/qca_single_img/qsdk-chipcode/common/build/bin/nand-ipq806x-single.img $@
endef
