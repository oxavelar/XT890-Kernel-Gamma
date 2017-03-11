#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_pkg_sender.h"

struct mdfld_mot_cmn_mot_panel_config {
	int (*acl_update)(struct mdfld_dsi_config *dsi_config);
	int acl_init_state;
	bool elvss_tth_supported;
};

struct dsi_cmd_entry {
	int type;
	int len;
	u8 *data;
};

#define CMD_MCS_LONG   0
#define CMD_MCS_SHORT0 1
#define CMD_MCS_SHORT1 2
#define CMD_SLEEP_MS   3

int mdfld_mot_cmn_send_cmd_list(struct mdfld_dsi_config *dsi_config,
				struct mdfld_dsi_pkg_sender *sender,
				struct dsi_cmd_entry *list,
				int num_entry);

int mdfld_mot_cmn_get_panel_manufacture(struct mdfld_dsi_pkg_sender *sender,
					u8 *ver);
int mdfld_mot_cmn_get_panel_cntrl_ver(struct mdfld_dsi_pkg_sender *sender,
				u8 *ver);
int mdfld_mot_cmn_get_panel_cntrl_drv_ver(struct mdfld_dsi_pkg_sender *sender,
					u8 *ver);
int mdfld_mot_cmn_acl_mode_get(void);
int mdfld_mot_cmn_elvss_tth_state_get(void);
int mdfld_mot_cmn_mot_panel_init(struct drm_device *dev,
				int pipe, struct mdfld_dsi_config *config,
				struct panel_funcs *p_funcs);

#define MOT_CMN_PANEL_MAX_VOLT_REGS 3

struct board_voltage_reg {
	/* todo: add regulator */
	int gpio; /* GPIO to enable regulator, -1 if not used */
	int gpio_en_val; /* GPIO state to enable */
};

struct board_power_props {
	int reset_gpio; /* GPIO to reset panel */
	int num_voltage_reg; /* Number of regulators for display */
	struct board_voltage_reg reg[MOT_CMN_PANEL_MAX_VOLT_REGS];
};

struct panel_power_props {
	int reset_gpio_en_val; /* GPIO state to put panel into reset */
	int reset_post_delay; /* msec delay after leaving reset mode */
	int reg_apply_post_delay; /* msec delay after turning on power */
	struct board_power_props *board;
};

int mdfld_mot_cmd_mot_panel_power_apply(struct panel_power_props *props);
int mdfld_mot_cmd_mot_panel_power_remove(struct panel_power_props *props);
