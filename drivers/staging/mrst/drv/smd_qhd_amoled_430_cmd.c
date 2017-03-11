#include "displays/smd_qhd_amoled_430_cmd.h"
#include "mdfld_mot_cmn.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_pkg_sender.h"
#include "smd_dynamic_gamma.h"

extern bool kexec_in_progress;

static u8 etc_cond_cmd_1[] = { 0xf0, 0x5a, 0x5a};
static u8 etc_cond_cmd_2[] = { 0xf1, 0x5a, 0x5a};
static u8 etc_cond_cmd_3[] = { 0xfc, 0x5a, 0x5a};
static u8 panel_cond_set[] = { 0xF8, 0x27, 0x27, 0x08,
			       0x08, 0x4E, 0xAA, 0x5E,
			       0x8A, 0x10, 0x3F, 0x10,
			       0x10, 0x00};
static u8 etc_cond2_cmd_1[] = { 0xF6, 0x00, 0x84, 0x09};

static u8 etc_cond2_cmd_2[]   = { 0xB0, 0x09};
static u8 etc_cond2_cmd_3[]   = { 0xD5, 0x64};
static u8 etc_cond2_cmd_4[]   = { 0xb0, 0x0b};
static u8 etc_cond2_cmd_5[]   = { 0xd5, 0xa4, 0x7e, 0x20};
static u8 etc_cond2_cmd_6[]   = { 0xb0, 0x08};
static u8 etc_cond2_cmd_7[]   = { 0xfd, 0xf8};
static u8 etc_cond2_cmd_8[]   = { 0xb0, 0x04};
static u8 etc_cond2_cmd_9[]   = { 0xf2, 0x4d};
static u8 etc_cond2_cmd_10[]  = { 0xb0, 0x05};
static u8 etc_cond2_cmd_11[]  = { 0xfd, 0x1f};
static u8 etc_cond2_cmd_12[]  = { 0xB1, 0x01, 0x00, 0x16};
static u8 exit_sleep_mode_cmd[] = { exit_sleep_mode};
static u8 mem_win2_cmd_1[] = { set_tear_on, 0x00};
static u8 mem_win2_cmd_2[] = { 0x2a, 0x00, 0x1e, 0x02,
				0x39};
static u8 mem_win2_cmd_3[] = { 0x2b, 0x00, 0x00, 0x03,
				0xbf};
static u8 mem_win2_cmd_4[] = {0xd1, 0x8a};
static u8 acl_cmd[] = { 0xc1, 0x47, 0x53, 0x13,
			0x53, 0x00, 0x00, 0x01,
			0xdf, 0x00, 0x00, 0x03,
			0x1f, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x02,
			0x03, 0x07, 0x0e, 0x14,
			0x1c, 0x24, 0x2d, 0x2d,
			0x00};

static struct dsi_cmd_entry init_seq_1[] = {
	{CMD_MCS_LONG, sizeof(etc_cond_cmd_1), etc_cond_cmd_1},
	{CMD_MCS_LONG, sizeof(etc_cond_cmd_2), etc_cond_cmd_2},
	{CMD_MCS_LONG, sizeof(etc_cond_cmd_3), etc_cond_cmd_3}
};

static struct dsi_cmd_entry init_seq_2[] = {
	{CMD_MCS_LONG, sizeof(panel_cond_set), panel_cond_set},
	{CMD_MCS_LONG, sizeof(etc_cond2_cmd_1), etc_cond2_cmd_1},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_2), etc_cond2_cmd_2},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_3), etc_cond2_cmd_3},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_4), etc_cond2_cmd_4},
	{CMD_MCS_LONG, sizeof(etc_cond2_cmd_5), etc_cond2_cmd_5},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_6), etc_cond2_cmd_6},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_7), etc_cond2_cmd_7},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_8), etc_cond2_cmd_8},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_9), etc_cond2_cmd_9},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_10), etc_cond2_cmd_10},
	{CMD_MCS_SHORT1, sizeof(etc_cond2_cmd_11), etc_cond2_cmd_11},
	{CMD_MCS_LONG, sizeof(etc_cond2_cmd_12), etc_cond2_cmd_12},
};

static struct dsi_cmd_entry init_seq_3[] = {
	{CMD_MCS_SHORT0, sizeof(exit_sleep_mode_cmd), exit_sleep_mode_cmd},
	{CMD_SLEEP_MS, 120, NULL},
	{CMD_MCS_SHORT1, sizeof(mem_win2_cmd_1), mem_win2_cmd_1},
	{CMD_MCS_LONG,   sizeof(mem_win2_cmd_2), mem_win2_cmd_2},
	{CMD_MCS_LONG,   sizeof(mem_win2_cmd_3), mem_win2_cmd_3},
	{CMD_MCS_SHORT1, sizeof(mem_win2_cmd_4), mem_win2_cmd_4},
	{CMD_MCS_LONG,   sizeof(acl_cmd), acl_cmd}
};

/* Dyanmic Gamma data */
#define V0 4500000

static uint8_t raw_mtp[RAW_MTP_SIZE];
static uint8_t gamma_settings[NUM_NIT_LVLS][RAW_GAMMA_SIZE];
static u16 input_gamma[NUM_VOLT_PTS][NUM_COLORS] = {
	{0x51, 0x39, 0x55},
	{0xb0, 0xc7, 0xa0},
	{0xb0, 0xc5, 0xb8},
	{0xc2, 0xcb, 0xc1},
	{0x94, 0xa0, 0x8f},
	{0xad, 0xb3, 0xa6},
	{0x00e0, 0x00d7, 0x0108}

};

static int smd_qhd_amoled_cmd_set_brightness(
	struct mdfld_dsi_config *dsi_config,
	int level);

static struct board_power_props smi_p1_board = {
	.reset_gpio = 96 + 32, /* GPIO_CORE_32 */
	.num_voltage_reg = 2,
	.reg = {
		/* VDDIO */
		[0] = {
			.gpio = 96 + 75, /* GPIO_CORE_75 */
			.gpio_en_val = 1,
		},
		/* VBATT */
		[1] = {
			.gpio = 78, /* GPIO_AON_78 */
			.gpio_en_val = 1,
		}
	}
};

static struct panel_power_props smd_qhd_amoled_cmd_power_props = {
	.reset_gpio_en_val = 0,
	.reset_post_delay = 15,
	.reg_apply_post_delay = 30,
	.board = &smi_p1_board,
};

int smd_qhd_amoled_cmd_acl_update(struct mdfld_dsi_config *dsi_config)
{
	int err;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int data = ((mdfld_mot_cmn_acl_mode_get() == 1) ? 1 : 0);
	err = mdfld_dsi_send_mcs_short_hs(sender, 0xc0, data, 1,
					MDFLD_DSI_SEND_PACKAGE);
	if (err)
		DRM_ERROR("Failed to set panel to ACL mode %d\n", data);
	else
		PSB_DEBUG_ENTRY("Panel ACL mode set to %d\n", data);

	return err;
}

static
void smd_qhd_amoled_cmd_dsi_cntrl_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	PSB_DEBUG_ENTRY("\n");

	dsi_config->lane_count = 2;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_2_2;

	/* This is for 400 mhz. Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;

	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xffffffff;
	hw_ctx->hs_tx_timeout = 0xffffff;
	hw_ctx->lp_rx_timeout = 0xffffff;
	hw_ctx->turn_around_timeout = 0x14;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->high_low_switch_count = 0x18;
	hw_ctx->init_count = 0xf0;
	hw_ctx->eot_disable = 0x3a;
	hw_ctx->lp_byteclk = 0x6;
	hw_ctx->clk_lane_switch_time_cnt = 0x18000b;
	hw_ctx->dphy_param = 0x160d3610;
	hw_ctx->dbi_bw_ctrl = 0x820;
	hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE;
	hw_ctx->mipi |= dsi_config->lane_config;
	hw_ctx->dsi_func_prg = (0xa000 | dsi_config->lane_count);

	/* Turn on HW TE and TE delay */
	hw_ctx->mipi |= TE_TRIGGER_GPIO_PIN | TE_USE_COUNTER_DELAY;
	hw_ctx->te_delay = 0xffff;

	mdfld_dsi_pkg_sender_set_cmd_packet_delay(sender, 50);

	/* Enable TE here due to smooth transition feature */
	REG_WRITE(regs->mipi_reg, hw_ctx->mipi);
	REG_WRITE(regs->te_delay_reg, hw_ctx->te_delay);
}

static struct drm_display_mode *smd_qhd_amoled_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 540;
	mode->vdisplay = 960;

	/* HFP = 90, HSYNC = 10, HBP = 20 */
	mode->hsync_start = mode->hdisplay + 90;
	mode->hsync_end = mode->hsync_start + 10;
	mode->htotal = mode->hsync_end + 20;
	/* VFP = 4, VSYNC = 2, VBP = 4 */
	mode->vsync_start = mode->vdisplay + 4;
	mode->vsync_end = mode->vsync_start + 2;
	mode->vtotal = mode->vsync_end + 4;
	mode->vrefresh = 60;
	mode->clock = mode->vrefresh * (mode->vtotal + 1) *
		(mode->htotal + 1) / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static int smd_qhd_amoled_cmd_bl_power_on(struct mdfld_dsi_config *dsi_config)
{
	int level;
	struct backlight_device *psb_bl = psb_get_backlight_device();

	if (!psb_bl)
		level = BRIGHTNESS_MAX_LEVEL;
	else
		level = psb_bl->props.brightness;

	return smd_qhd_amoled_cmd_set_brightness(dsi_config, level);
}

static void smd_qhd_amoled_cmd_get_mtp_offset(
	struct mdfld_dsi_config *dsi_config,
	uint8_t raw_mtp[RAW_MTP_SIZE])
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int r;
	u8 raw_data[RAW_MTP_SIZE];

	r = mdfld_dsi_read_mcs_hs(sender, 0xd3, raw_data, RAW_MTP_SIZE);
	if (r != RAW_MTP_SIZE) {
		DRM_ERROR("Failed to read MTP offset, r = %d\n", r);
		memset(raw_mtp, 0, RAW_MTP_SIZE);
	} else {
		memcpy(raw_mtp, raw_data, RAW_MTP_SIZE);
	}
}

static void smd_qhd_amoled_cmd_get_mtp_set4(struct mdfld_dsi_config *dsi_config,
					u8 *elvss, u8 *white_pt_adj)
{
	static bool is_cached;
	static u8 cached_elvss = 0x08; /* init to default value per spec */
	static u8 cached_white_pt_adj = 0xff;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int r = 0;
	u8 reg_data[2];
	u8 ver;

	if (!is_cached) {
		r = mdfld_dsi_read_mcs_hs(sender, 0xd4, reg_data, 2);
		if (r != 2) {
			DRM_ERROR("Failed to read MTP_SET4, r = %d\n", r);
		} else {
			cached_elvss = reg_data[0] & 0x3f;
			if (mdfld_mot_cmn_get_panel_cntrl_ver(sender, &ver))
				DRM_ERROR("Failed to get panel controller "
					"version\n");
			else {
				if (ver >= 1)
					cached_white_pt_adj =
						(reg_data[1] & 0xf0) >> 4;

				printk(KERN_ALERT"Read MTP_SET4, elvss = 0x%02x,"
					"white_pt = 0x%02x\n",
					cached_elvss, cached_white_pt_adj);
				is_cached = true;
			}
		}
	}

	if (elvss)
		*elvss = cached_elvss;

	if (white_pt_adj)
		*white_pt_adj = cached_white_pt_adj;
}

static int __smd_qhd_amoled_cmd_dsi_power_on(
	struct mdfld_dsi_config *dsi_config)
{
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 etc_cond2_cmd_13[]  = { 0xB2, 0x00, 0x00, 0x00, 0x00 };
	u8 elvss;
	u8 ver;
	u8 cmd = 0;
	int err = 0;
	int retry;

	PSB_DEBUG_ENTRY("Start AMOLED command mode init sequence\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	err = mdfld_mot_cmd_mot_panel_power_apply(
		&smd_qhd_amoled_cmd_power_props);
	if (err)
		return err;

	mdfld_mot_cmn_get_panel_manufacture(sender, &ver);
	mdfld_mot_cmn_get_panel_cntrl_ver(sender, &ver);
	mdfld_mot_cmn_get_panel_cntrl_drv_ver(sender, &ver);

	err = mdfld_mot_cmn_send_cmd_list(dsi_config, sender,
					init_seq_1,
					ARRAY_SIZE(init_seq_1));
	if (err)
		return err;

	err = smd_qhd_amoled_cmd_bl_power_on(dsi_config);
	if (err)
		return err;


	err = mdfld_mot_cmn_send_cmd_list(dsi_config, sender,
					init_seq_2,
					ARRAY_SIZE(init_seq_2));
	if (err)
		return err;


	smd_qhd_amoled_cmd_get_mtp_set4(dsi_config, &elvss, NULL);
	if (mdfld_mot_cmn_elvss_tth_state_get() == 1)
		elvss += 0xf;

	etc_cond2_cmd_13[1] = elvss;
	etc_cond2_cmd_13[2] = elvss;
	etc_cond2_cmd_13[3] = elvss;
	etc_cond2_cmd_13[4] = elvss;
	err = mdfld_dsi_send_mcs_long_hs(sender, etc_cond2_cmd_13,
					sizeof(etc_cond2_cmd_13),
					MDFLD_DSI_SEND_PACKAGE);
	if (err)
		return err;

	err = mdfld_mot_cmn_send_cmd_list(dsi_config, sender,
					init_seq_3,
					ARRAY_SIZE(init_seq_3));
	if (err)
		return err;

	err = smd_qhd_amoled_cmd_acl_update(dsi_config);
	if (err)
		return err;

	/* Ensure counters do not match to prevent detection of
	   duplicate frame */
	atomic64_set(&sender->last_screen_update,
		(atomic64_read(&sender->te_seq) - 1));

	cmd = write_mem_start;
	err = mdfld_dsi_send_dcs(sender,
			cmd,
			NULL,
			0,
			CMD_DATA_SRC_PIPE,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("DCS 0x%x sent failed\n", cmd);
		return err;
	}

	/* Ensure frame is sent out before turning on display */
	retry = 20;
	while (retry) {
		if (!(REG_READ(sender->mipi_gen_fifo_stat_reg) & BIT27))
			break;
		else {
			usleep_range(1000, 1100);
			retry--;
		}
	}

	cmd = set_display_on;
	err = mdfld_dsi_send_dcs(sender,
			cmd,
			NULL,
			0,
			CMD_DATA_SRC_SYSTEM_MEM,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("DCS 0x%x sent failed\n", cmd);
		return err;
	}

	printk(KERN_INFO"Panel is on\n");

	return err;
}

static int __smd_qhd_amoled_cmd_dsi_power_off(
	struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 cmd = 0;
	int err = 0;

	PSB_DEBUG_ENTRY("Turn off AMOLED command mode panel...\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/*set display off*/
	cmd = set_display_off;
	err = mdfld_dsi_send_dcs(sender,
			cmd,
			NULL,
			0,
			CMD_DATA_SRC_SYSTEM_MEM,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("DCS 0x%x sent failed\n", cmd);
		return err;
	}

	/*enter sleep mode*/
	cmd = enter_sleep_mode;
	err = mdfld_dsi_send_dcs(sender,
			cmd,
			NULL,
			0,
			CMD_DATA_SRC_SYSTEM_MEM,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("DCS 0x%x sent failed\n", cmd);
		return err;
	}

	msleep(120);

	err = mdfld_mot_cmd_mot_panel_power_remove(
		&smd_qhd_amoled_cmd_power_props);

	printk(KERN_INFO"Panel is off\n");

	return err;
}


static int __smd_qhd_amoled_cmd_reboot_notifier(struct notifier_block *nb,
			unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	struct drm_psb_private *dev_priv = container_of(nb,
						struct drm_psb_private,
						dbi_panel_reboot_notifier);

	if (!dev_priv->dsi_init_done)
		goto end;

	/* If kexec happening, do not turn off to allow smooth BOS -> AOS
	   display transition */
	if (kexec_in_progress)
		goto end;

	mdfld_generic_dsi_dbi_set_power(&dev_priv->encoder0->base, false);

end:
	return ret;
}


static void smd_qhd_amoled_cmd_get_panel_info(int pipe, struct panel_info *pi)
{
	pi->width_mm = 53;
	pi->height_mm = 95;
}

static int smd_qhd_amoled_cmd_dsi_detect(struct mdfld_dsi_config *dsi_config)
{
	int status;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	u32 dpll_val, device_ready_val;
	int pipe = dsi_config->pipe;

	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		/*
		 * FIXME: WA to detect the panel connection status, and need to
		 * implement detection feature with get_power_mode DSI command.
		 */
		if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
					OSPM_UHB_FORCE_POWER_ON)) {
			DRM_ERROR("hw begin failed\n");
			return -EAGAIN;
		}

		dpll_val = REG_READ(regs->dpll_reg);
		device_ready_val = REG_READ(regs->device_ready_reg);
		if ((device_ready_val & DSI_DEVICE_READY) &&
		    (dpll_val & DPLL_VCO_ENABLE)) {
			dsi_config->dsi_hw_context.panel_on = true;
		} else {
			dsi_config->dsi_hw_context.panel_on = false;
			DRM_INFO("%s: panel is not detected!\n", __func__);
		}

		status = MDFLD_DSI_PANEL_CONNECTED;

		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	} else {
		DRM_INFO("%s: do NOT support dual panel\n", __func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static char *smd_qhd_amoled_cmd_get_gamma_settings(
	struct mdfld_dsi_config *dsi_config,
	int level)
{
	static bool gamma_calced;
	int index;

	if (!gamma_calced) {
		smd_qhd_amoled_cmd_get_mtp_offset(dsi_config, raw_mtp);
		smd_dynamic_gamma_calc(V0, 0xfa, 0x02, raw_mtp,
				input_gamma, gamma_settings);
		gamma_calced = true;
	}

	index = (level * NUM_NIT_LVLS) / BRIGHTNESS_MAX_LEVEL;
	if (index < 0)
		index = 0;
	else if (index >= NUM_NIT_LVLS)
		index = NUM_NIT_LVLS - 1;

	PSB_DEBUG_ENTRY("Set AMOLED brightness level %d (index %d)\n",
			level, index);
	return gamma_settings[index];
}

static int smd_qhd_amoled_cmd_set_brightness(
	struct mdfld_dsi_config *dsi_config, int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	char *gamma_ptr;
	int err;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	gamma_ptr = smd_qhd_amoled_cmd_get_gamma_settings(dsi_config,
							level);

	err = mdfld_dsi_send_mcs_long_hs(sender, gamma_ptr,
					RAW_GAMMA_SIZE,
					MDFLD_DSI_SEND_PACKAGE);

	if (err) {
		DRM_ERROR("Failed to send gamma values\n");
		goto func_exit;
	}

	err = mdfld_dsi_send_mcs_short_hs(sender, 0xfa, 0x03, 1,
					MDFLD_DSI_SEND_PACKAGE);
	if (err)
		DRM_ERROR("Failed to set gamma\n");

func_exit:
	return err;
}

static struct mdfld_mot_cmn_mot_panel_config amoled_dsi_dbi_mot_panel_config = {
	.acl_update = smd_qhd_amoled_cmd_acl_update,
	.acl_init_state = 0,
	.elvss_tth_supported = true,
};

int smd_qhd_amoled_430_gamma_dbg_dump(struct seq_file *m, void *data)
{
	smd_dynamic_gamma_dbg_dump(raw_mtp, gamma_settings, m, data, 0);
	return 0;
}

int smd_qhd_amoled_430_gamma_dbg_dump_csv(struct seq_file *m, void *data)
{
	smd_dynamic_gamma_dbg_dump(raw_mtp, gamma_settings, m, data, 1);
	return 0;
}

void smd_qhd_amoled_430_cmd_init(struct drm_device *dev,
				struct panel_funcs *p_funcs)
{

	PSB_DEBUG_ENTRY("\n");
	p_funcs->get_config_mode = smd_qhd_amoled_cmd_get_config_mode;
	p_funcs->get_panel_info = smd_qhd_amoled_cmd_get_panel_info;
	p_funcs->reset = NULL;
	p_funcs->dsi_controller_init = smd_qhd_amoled_cmd_dsi_cntrl_init;
	p_funcs->detect = smd_qhd_amoled_cmd_dsi_detect;
	p_funcs->set_brightness = smd_qhd_amoled_cmd_set_brightness;
	p_funcs->power_on = __smd_qhd_amoled_cmd_dsi_power_on;
	p_funcs->power_off = __smd_qhd_amoled_cmd_dsi_power_off;
	p_funcs->mot_panel = &amoled_dsi_dbi_mot_panel_config;
	p_funcs->reboot_notifier = __smd_qhd_amoled_cmd_reboot_notifier;
}
