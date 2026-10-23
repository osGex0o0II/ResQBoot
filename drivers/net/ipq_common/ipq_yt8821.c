/*
 * Motorcomm YT8821 2.5Gbps PHY driver for ipq5332.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <asm-generic/errno.h>
#include "ipq_yt8821.h"
#include "ipq_phy.h"

extern int ipq_mdio_read(int mii_id, int regnum, ushort *data);
extern int ipq_mdio_write(int mii_id, int regnum, u16 data);

static int yt8821_mdio_read(u32 phy_id, u32 reg) {
	return ipq_mdio_read(phy_id, reg, NULL);
}

static int yt8821_mdio_write(u32 phy_id, u32 reg, u16 val) {
	return ipq_mdio_write(phy_id, reg, val);
}

static int ytphy_read_ext(u32 phy_id, u16 regnum) {
	int ret;

	ret = yt8821_mdio_write(phy_id, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return yt8821_mdio_read(phy_id, YTPHY_PAGE_DATA);
}

static int ytphy_write_ext(u32 phy_id, u16 regnum,u16 val) {
	int ret;

	ret = yt8821_mdio_write(phy_id, YTPHY_PAGE_SELECT, regnum);
	if (ret < 0)
		return ret;

	return yt8821_mdio_write(phy_id, YTPHY_PAGE_DATA, val);
}

static int ytphy_modify_ext(u32 phy_id, u16 regnum, u16 mask, u16 set) {
	int val;

	val = ytphy_read_ext(phy_id, regnum);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set;

	return ytphy_write_ext(phy_id, regnum, (u16)val);
}

static int yt8821_select_utp_space(u32 phy_id) {
	int val = ytphy_read_ext(phy_id, YTPHY_REG_SPACE_SELECT_REG);
	if (val < 0)
		return val;
	if (val & YTPHY_RSSR_SPACE_MASK)
		return ytphy_modify_ext(phy_id, YTPHY_REG_SPACE_SELECT_REG, YTPHY_RSSR_SPACE_MASK, YTPHY_RSSR_UTP_SPACE);
	return 0;
}

static int yt8821_serdes_init(u32 phy_id) {
	int ret;

	ret = ytphy_modify_ext(phy_id, YTPHY_REG_SPACE_SELECT_REG, YTPHY_RSSR_SPACE_MASK, YTPHY_RSSR_SPACE_MASK);
	if (ret < 0)
		return ret;

	ret = ytphy_modify_ext(phy_id, YT8821_SDS_EXT_CSR_CTRL_REG, YT8821_SDS_EXT_CSR_VCO_LDO_EN | YT8821_SDS_EXT_CSR_VCO_BIAS_LPF_EN, YT8821_SDS_EXT_CSR_VCO_LDO_EN);
	if (ret < 0)
		return ret;

	return yt8821_select_utp_space(phy_id);
}

static const struct {u16 reg, mask, set;} yt8821_utp_cfg_pre[] = {
	{ YT8821_UTP_EXT_RPDN_CTRL_REG, YT8821_UTP_EXT_RPDN_BP_FFE_LNG_2500 | YT8821_UTP_EXT_RPDN_BP_FFE_SHT_2500 |
		YT8821_UTP_EXT_RPDN_IPR_SHT_2500, YT8821_UTP_EXT_RPDN_BP_FFE_LNG_2500 | YT8821_UTP_EXT_RPDN_BP_FFE_SHT_2500 },
	{ YT8821_UTP_EXT_VGA_LPF1_CAP_CTRL_REG, YT8821_UTP_EXT_VGA_LPF1_CAP_OTHER | YT8821_UTP_EXT_VGA_LPF1_CAP_2500, 0 },
	{ YT8821_UTP_EXT_VGA_LPF2_CAP_CTRL_REG, YT8821_UTP_EXT_VGA_LPF2_CAP_OTHER | YT8821_UTP_EXT_VGA_LPF2_CAP_2500, 0 },
	{ YT8821_UTP_EXT_TRACE_CTRL_REG, YT8821_UTP_EXT_TRACE_LNG_GAIN_THE_2500 | YT8821_UTP_EXT_TRACE_MED_GAIN_THE_2500, 0x5a3c },
	{ YT8821_UTP_EXT_ALPHA_IPR_CTRL_REG, YT8821_UTP_EXT_IPR_LNG_2500, 0x6c },
	{ YT8821_UTP_EXT_ECHO_CTRL_REG, YT8821_UTP_EXT_TRACE_LNG_GAIN_THR_1000, 0x2a00 },
	{ YT8821_UTP_EXT_GAIN_CTRL_REG, YT8821_UTP_EXT_TRACE_MED_GAIN_THR_1000, 0x22 },
	{ YT8821_UTP_EXT_TH_20DB_2500_CTRL_REG, YT8821_UTP_EXT_TH_20DB_2500, 0x8000 },
	{ YT8821_UTP_EXT_MU_COARSE_FR_CTRL_REG, YT8821_UTP_EXT_MU_COARSE_FR_F_FFE | YT8821_UTP_EXT_MU_COARSE_FR_F_FBE, 0x7700 },
	{ YT8821_UTP_EXT_MU_FINE_FR_CTRL_REG, YT8821_UTP_EXT_MU_FINE_FR_F_FFE | YT8821_UTP_EXT_MU_FINE_FR_F_FBE, 0x2200 },
}, yt8821_utp_cfg_post[] = {
	{ YT8821_UTP_EXT_VCT_CFG6_CTRL_REG, YT8821_UTP_EXT_FECHO_AMP_TH_HUGE, 0x3800 },
	{ YT8821_UTP_EXT_TXGE_NFR_FR_THP_CTRL_REG, YT8821_UTP_EXT_NFR_TX_ABILITY, YT8821_UTP_EXT_NFR_TX_ABILITY },
	{ YT8821_UTP_EXT_PLL_CTRL_REG, YT8821_UTP_EXT_PLL_SPARE_CFG, 0xe9 },
	{ YT8821_UTP_EXT_DAC_IMID_CH_2_3_CTRL_REG, YT8821_UTP_EXT_DAC_IMID_CH_3_10_ORG | YT8821_UTP_EXT_DAC_IMID_CH_2_10_ORG, 0x6464 },
	{ YT8821_UTP_EXT_DAC_IMID_CH_0_1_CTRL_REG, YT8821_UTP_EXT_DAC_IMID_CH_1_10_ORG | YT8821_UTP_EXT_DAC_IMID_CH_0_10_ORG, 0x6464 },
	{ YT8821_UTP_EXT_DAC_IMSB_CH_2_3_CTRL_REG, YT8821_UTP_EXT_DAC_IMSB_CH_3_10_ORG | YT8821_UTP_EXT_DAC_IMSB_CH_2_10_ORG, 0x6464 },
	{ YT8821_UTP_EXT_DAC_IMSB_CH_0_1_CTRL_REG, YT8821_UTP_EXT_DAC_IMSB_CH_1_10_ORG | YT8821_UTP_EXT_DAC_IMSB_CH_0_10_ORG, 0x6464 },
};

static int yt8821_utp_init(u32 phy_id) {
	int ret, i;
	u16 save;

	ret = yt8821_select_utp_space(phy_id);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(yt8821_utp_cfg_pre); i++) {
		ret = ytphy_modify_ext(phy_id, yt8821_utp_cfg_pre[i].reg, yt8821_utp_cfg_pre[i].mask, yt8821_utp_cfg_pre[i].set);
		if (ret < 0)
			return ret;
	}

	ret = ytphy_read_ext(phy_id, YT8821_UTP_EXT_PI_CTRL_REG);
	if (ret < 0)
		return ret;
	save = (u16)ret;

	ret = ytphy_modify_ext(phy_id, YT8821_UTP_EXT_PI_CTRL_REG, YT8821_UTP_EXT_PI_TX_CLK_SEL_AFE | YT8821_UTP_EXT_PI_RX_CLK_3_SEL_AFE |
			       YT8821_UTP_EXT_PI_RX_CLK_2_SEL_AFE | YT8821_UTP_EXT_PI_RX_CLK_1_SEL_AFE | YT8821_UTP_EXT_PI_RX_CLK_0_SEL_AFE, 0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phy_id, YT8821_UTP_EXT_PI_CTRL_REG, save);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(yt8821_utp_cfg_post); i++) {
		ret = ytphy_modify_ext(phy_id, yt8821_utp_cfg_post[i].reg, yt8821_utp_cfg_post[i].mask, yt8821_utp_cfg_post[i].set);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int yt8821_auto_sleep_config(u32 phy_id, int disable) {
	int ret;

	ret = yt8821_select_utp_space(phy_id);
	if (ret < 0)
		return ret;

	return ytphy_modify_ext(phy_id, YT8531_EXTREG_SLEEP_CONTROL1_REG, YT8531_ESC1R_SLEEP_SW, disable ? YT8531_ESC1R_SLEEP_SW : 0);
}

static u8 yt8821_phy_get_link_status(u32 dev_id, u32 phy_id) {
	u16 phy_data;

	yt8821_select_utp_space(phy_id);

	phy_data = (u16)yt8821_mdio_read(phy_id, YTPHY_SPECIFIC_STATUS_REG);
	if (phy_data & YTPHY_LINK_UP)
		return 0;

	return 1;
}

static u32 yt8821_phy_get_speed(u32 dev_id, u32 phy_id, fal_port_speed_t *speed) {
	u16 phy_data;
	int speed_mode;

	yt8821_select_utp_space(phy_id);

	phy_data = (u16)yt8821_mdio_read(phy_id, YTPHY_SPECIFIC_STATUS_REG);
	speed_mode = phy_data & YTPHY_SPEED_MASK;

	switch (speed_mode) {
	case YTPHY_SPEED_2500M:
		*speed = FAL_SPEED_2500;
		break;
	case YTPHY_SPEED_1000M:
		*speed = FAL_SPEED_1000;
		break;
	case YTPHY_SPEED_100M:
		*speed = FAL_SPEED_100;
		break;
	case YTPHY_SPEED_10M:
		*speed = FAL_SPEED_10;
		break;
	default:
		*speed = FAL_SPEED_1000;
		break;
	}
	return 0;
}

static u32 yt8821_phy_get_duplex(u32 dev_id, u32 phy_id, fal_port_duplex_t *duplex) {
	u16 phy_data;

	yt8821_select_utp_space(phy_id);

	phy_data = (u16)yt8821_mdio_read(phy_id, YTPHY_SPECIFIC_STATUS_REG);
	if (phy_data & YTPHY_DUPLEX_MASK)
		*duplex = FAL_FULL_DUPLEX;
	else
		*duplex = FAL_HALF_DUPLEX;

	return 0;
}

int ipq_yt8821_phy_init(struct phy_ops **ops, u32 phy_id) {
	int ret;
	struct phy_ops *yt8821_ops;

	yt8821_ops = (struct phy_ops *)malloc(sizeof(struct phy_ops));
	if (!yt8821_ops)
		return -ENOMEM;
	yt8821_ops->phy_get_link_status = yt8821_phy_get_link_status;
	yt8821_ops->phy_get_speed = yt8821_phy_get_speed;
	yt8821_ops->phy_get_duplex = yt8821_phy_get_duplex;
	*ops = yt8821_ops;

	printf("PHY ID1: 0x%x\n", yt8821_mdio_read(phy_id, QCA_PHY_ID1));
	printf("PHY ID2: 0x%x\n", yt8821_mdio_read(phy_id, QCA_PHY_ID2));

	ret = ytphy_modify_ext(phy_id, YT8531_CHIP_CONFIG_REG, YTPHY_CCR_MODE_SEL_MASK, FIELD_PREP(YTPHY_CCR_MODE_SEL_MASK, YT8821_CHIP_MODE_AUTO_BX2500_SGMII));
	if (ret < 0)
		return ret;

	ret = yt8821_serdes_init(phy_id);
	if (ret < 0)
		return ret;

	ret = yt8821_utp_init(phy_id);
	if (ret < 0)
		return ret;

	ret = yt8821_auto_sleep_config(phy_id, 1);
	if (ret < 0)
		return ret;

	return 0;
}