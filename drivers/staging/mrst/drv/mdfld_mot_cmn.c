#include "mdfld_mot_cmn.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_dbi_dsr.h"

#define ACL_NOT_SUPPORTED -1
#define ELVSS_TTH_NOT_SUPPORTED -1

static int acl_mode_state = ACL_NOT_SUPPORTED;
static int elvss_tth_state = ELVSS_TTH_NOT_SUPPORTED;

static struct panel_funcs *dsi_config_to_p_func(
	struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_encoder *encoder;
	struct mdfld_dsi_dpi_output *dpi_output;
	struct mdfld_dsi_dbi_output *dbi_output;
	struct panel_funcs *p_funcs = NULL;

	if (!dsi_config)
		return NULL;

	encoder = dsi_config->encoders[dsi_config->type];
	if (!encoder)
		return NULL;

	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
		dbi_output = MDFLD_DSI_DBI_OUTPUT(encoder);
		p_funcs = dbi_output ? dbi_output->p_funcs : NULL;
	} else if (dsi_config->type == MDFLD_DSI_ENCODER_DPI) {
		dpi_output = MDFLD_DSI_DPI_OUTPUT(encoder);
		p_funcs = dpi_output ? dpi_output->p_funcs : NULL;
	}

	return p_funcs;
}

static ssize_t panel_acl_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int state = mdfld_mot_cmn_acl_mode_get();

	if (state == ACL_NOT_SUPPORTED)
		state = -EINVAL;

	PSB_DEBUG_ENTRY("ACL mode = %d\n", state);
	return snprintf(buf, PAGE_SIZE, "%d\n", state);
}

static ssize_t panel_acl_store(struct device *dev,
			struct device_attribute *attr,
			char *buf, size_t count)
{
	int r = 0;
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct panel_funcs *p_funcs;
	int old_state = mdfld_mot_cmn_acl_mode_get();
	unsigned long new_state;

	if (old_state == ACL_NOT_SUPPORTED) {
		DRM_ERROR("Tried to set ACL, not supported\n");
		r = -EINVAL;
		goto err;
	}

	r = strict_strtoul(buf, 0, &new_state);
	if ((r) || ((new_state != 0) && (new_state != 1))) {
		DRM_ERROR("Invalid ACL value = %lu\n", new_state);
		r = -EINVAL;
		goto err;
	}

	if (!ddev || !ddev->dev_private) {
		DRM_ERROR("Invalid device\n");
		r = -ENODEV;
		goto err;
	}

	dev_priv = ddev->dev_private;
	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config) {
		DRM_ERROR("No dsi config when setting ACL\n");
		r = -ENODEV;
		goto err;
	}

	p_funcs = dsi_config_to_p_func(dsi_config);
	if (!p_funcs) {
		DRM_ERROR("No p_funcs when setting ACL\n");
		r = -ENODEV;
		goto err;
	} else if (new_state != old_state) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
						OSPM_UHB_FORCE_POWER_ON)) {
			mutex_lock(&dsi_config->context_lock);
			acl_mode_state = new_state;

			if (dsi_config->dsi_hw_context.panel_on) {
				mdfld_dsi_dsr_forbid_locked(dsi_config);
				r = p_funcs->mot_panel->acl_update(dsi_config);
				mdfld_dsi_dsr_allow_locked(dsi_config);
			}

			mutex_unlock(&dsi_config->context_lock);
			PSB_DEBUG_ENTRY("Updated ACL state to %d\n", new_state);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			r = -EAGAIN;
			DRM_ERROR("Failed to enable display island\n");
		}
	}

err:
	return r ? r : count;
}

static DEVICE_ATTR(acl_mode, S_IRUGO | S_IWGRP,
		panel_acl_show, panel_acl_store);

static ssize_t panel_elvss_tth_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int state = mdfld_mot_cmn_elvss_tth_state_get();

	if (state == ELVSS_TTH_NOT_SUPPORTED)
		state = -EINVAL;

	PSB_DEBUG_ENTRY("ELVSS TTH state = %d\n", state);
	return snprintf(buf, PAGE_SIZE, "%d\n", state);
}

static ssize_t panel_elvss_tth_store(struct device *dev,
			struct device_attribute *attr,
			char *buf, size_t count)
{
	int r = 0;
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct drm_psb_private *dev_priv;
	struct mdfld_dsi_config *dsi_config = NULL;
	struct panel_funcs *p_funcs;
	int old_state = mdfld_mot_cmn_elvss_tth_state_get();
	unsigned long new_state;

	if (old_state == ELVSS_TTH_NOT_SUPPORTED) {
		DRM_ERROR("Tried to set ELVSS TTH, not supported\n");
		r = -EINVAL;
		goto err;
	}

	r = strict_strtoul(buf, 0, &new_state);
	if ((r) || ((new_state != 0) && (new_state != 1))) {
		DRM_ERROR("Invalid ELVSS TTH value = %lu\n", new_state);
		r = -EINVAL;
		goto err;
	}

	if (!ddev || !ddev->dev_private) {
		DRM_ERROR("Invalid device\n");
		r = -ENODEV;
		goto err;
	}

	dev_priv = ddev->dev_private;
	dsi_config = dev_priv->dsi_configs[0];
	if (!dsi_config) {
		DRM_ERROR("No dsi config when setting ELVSS TTH\n");
		r = -ENODEV;
		goto err;
	}

	p_funcs = dsi_config_to_p_func(dsi_config);
	if (!p_funcs) {
		DRM_ERROR("No p_funcs when setting ELVSS TTH\n");
		r = -ENODEV;
		goto err;
	} else if (new_state != old_state) {
		mutex_lock(&dsi_config->context_lock);
		elvss_tth_state = new_state;
		mutex_unlock(&dsi_config->context_lock);
		PSB_DEBUG_ENTRY("Updated ELVSS TTH state to %d\n", new_state);
	}

err:
	return r ? r : count;
}

static DEVICE_ATTR(elvss_tth_status, S_IRUGO | S_IWGRP,
		panel_elvss_tth_show, panel_elvss_tth_store);

static int mot_panel_power_initial_config(struct panel_power_props *props)
{
	static bool configured;
	int r;
	int i;
	int num_regs_handled = 0;

	if (!props || !props->board)
		return -EINVAL;

	if (configured)
		return 0;

	if (props->board->reset_gpio != -1) {
		r = gpio_request(props->board->reset_gpio, "panel-reset");
		if (r) {
			DRM_ERROR("Failed to request panel reset gpio %d\n",
				props->board->reset_gpio);
			return r;
		}
		gpio_direction_output(props->board->reset_gpio,
				!props->reset_gpio_en_val);
	}

	for (i = 0; i < props->board->num_voltage_reg; i++) {
		if (props->board->reg[i].gpio != -1) {
			r = gpio_request(props->board->reg[i].gpio,
					"panel-reg-enable");
			if (r) {
				DRM_ERROR("Failed to request panel reg %d "
					"enable gpio %d\n",
					i, props->board->reg[i].gpio);
				num_regs_handled = i;
				goto reg_gpio_err;
			}
			gpio_direction_output(props->board->reg[i].gpio,
					props->board->reg[i].gpio_en_val);
		}
	}

	configured = true;
	return 0;

reg_gpio_err:
	if (props->board->reset_gpio != -1)
		gpio_free(props->board->reset_gpio);

	for (i = 0; i < num_regs_handled; i++) {
		if (props->board->reg[i].gpio != -1)
			gpio_free(props->board->reg[i].gpio);
	}
	return r;
}

int mdfld_mot_cmn_send_cmd_list(struct mdfld_dsi_config *dsi_config,
				struct mdfld_dsi_pkg_sender *sender,
				struct dsi_cmd_entry *list, int num_entry)
{
	struct dsi_cmd_entry *entry;
	u8 *mcs_short_data;
	int i;
	int err = 0;

	for (i = 0; i < num_entry; i++) {
		entry = &list[i];
		PSB_DEBUG_INIT("Sending command # %d, type = %d\n",
			i, entry->type);

		if (entry->type == CMD_MCS_LONG) {
			err = mdfld_dsi_send_mcs_long_hs(sender,
							entry->data,
							entry->len,
							MDFLD_DSI_SEND_PACKAGE);
		} else if ((entry->type == CMD_MCS_SHORT1) &&
			(entry->len == 2)) {
			mcs_short_data = entry->data;
			err = mdfld_dsi_send_mcs_short_hs(sender,
							entry->data[0],
							entry->data[1],
							1,
							MDFLD_DSI_SEND_PACKAGE);
		} else if (entry->type == CMD_MCS_SHORT0) {
			mcs_short_data = entry->data;
			err = mdfld_dsi_send_mcs_short_hs(sender,
							entry->data[0],
							0,
							0,
							MDFLD_DSI_SEND_PACKAGE);
		} else if (entry->type == CMD_SLEEP_MS) {
			msleep(entry->len);
		} else {
			DRM_ERROR("Invalid command entry type %d\n",
				entry->type);
			err = -EINVAL;
		}

		if (err) {
			DRM_ERROR("Failed sending command entry # %d\n", i);
			break;
		}
	}

	return err;
}

int mdfld_mot_cmn_get_panel_manufacture(struct mdfld_dsi_pkg_sender *sender,
					u8 *ver)
{
	static u8 cached_ver = 0xff;
	int err;
	u8 read_data;

	if (cached_ver == 0xff) {
		err = mdfld_dsi_read_mcs_hs(sender, 0xda, &read_data, 1);
		if (err != 1) {
			DRM_ERROR("Failed to read panel manufacture, "
				"err = %d\n", err);
			return (err < 0) ? err : -EFAULT;
		}
		cached_ver = read_data;
	}
	*ver = cached_ver;
	PSB_DEBUG_ENTRY("Panel manufacture id = 0x%02x\n", cached_ver);
	return 0;
}

int mdfld_mot_cmn_get_panel_cntrl_ver(struct mdfld_dsi_pkg_sender *sender,
				u8 *ver)
{
	static u8 cached_ver = 0xff;
	int err;
	u8 read_data;

	if (cached_ver == 0xff) {
		err = mdfld_dsi_read_mcs_hs(sender, 0xdb, &read_data, 1);
		if (err != 1) {
			DRM_ERROR("Failed to read panel controller version, "
				"err = %d\n", err);
			return (err < 0) ? err : -EFAULT;
		}
		cached_ver = read_data;
	}
	*ver = cached_ver;
	PSB_DEBUG_ENTRY("Panel controller version = 0x%02x\n", cached_ver);
	return 0;
}

int mdfld_mot_cmn_get_panel_cntrl_drv_ver(struct mdfld_dsi_pkg_sender *sender,
					u8 *ver)
{
	static u8 cached_ver = 0xff;
	int err;
	u8 read_data;

	if (cached_ver == 0xff) {
		err = mdfld_dsi_read_mcs_hs(sender, 0xdc, &read_data, 1);
		if (err != 1) {
			DRM_ERROR("Failed to read panel controller driver"
				" version, err = %d\n", err);
			return (err < 0) ? err : -EFAULT;
		}
		cached_ver = read_data;
	}

	*ver = cached_ver;
	PSB_DEBUG_ENTRY("Panel controller driver version = 0x%02x\n",
			cached_ver);
	return 0;
}

int mdfld_mot_cmn_acl_mode_get(void)
{
	return acl_mode_state;
}

int mdfld_mot_cmn_elvss_tth_state_get(void)
{
	return elvss_tth_state;
}

int mdfld_mot_cmn_mot_panel_init(struct drm_device *dev,
				int pipe, struct mdfld_dsi_config *config,
				struct panel_funcs *p_funcs)
{
	int r = 0;

	if (!p_funcs->mot_panel || pipe != 0)
		return 0;

	if (p_funcs->mot_panel->acl_update) {
		PSB_DEBUG_INIT("ACL mode supported\n");
		acl_mode_state = p_funcs->mot_panel->acl_init_state;
		r = device_create_file(dev->dev, &dev_attr_acl_mode);
		if (r < 0)
			DRM_ERROR("Failed to create ACL entry, r = %d\n", r);
	}

	if (p_funcs->mot_panel->elvss_tth_supported) {
		PSB_DEBUG_INIT("ELVSS TTH supported\n");
		elvss_tth_state = 0;
		r = device_create_file(dev->dev, &dev_attr_elvss_tth_status);
		if (r < 0)
			DRM_ERROR("Failed to create ELVSS TTH entry, r = %d\n",
				r);
	}

	return r;
}

int mdfld_mot_cmd_mot_panel_power_apply(struct panel_power_props *props)
{
	int r;
	int i;

	if (!props || !props->board)
		return -EINVAL;

	r = mot_panel_power_initial_config(props);
	if (!r) {
		PSB_DEBUG_INIT("Apply panel power start\n");
		/* Make sure panel in reset */
		if (props->board->reset_gpio)
			gpio_set_value(props->board->reset_gpio,
				props->reset_gpio_en_val);

		/* Enable all regulators */
		for (i = 0; i < props->board->num_voltage_reg; i++) {
			if (props->board->reg[i].gpio != -1)
				gpio_set_value(props->board->reg[i].gpio,
					props->board->reg[i].gpio_en_val);
		}

		msleep(props->reg_apply_post_delay);

		/* Bring panel out of reset */
		if (props->board->reset_gpio) {
			gpio_set_value(props->board->reset_gpio,
				!props->reset_gpio_en_val);
			msleep(props->reset_post_delay);
		}
		PSB_DEBUG_INIT("Apply panel power complete\n");
	}

	return r;
}

int mdfld_mot_cmd_mot_panel_power_remove(struct panel_power_props *props)
{
	int r;
	int i;

	if (!props || !props->board)
		return -EINVAL;

	r = mot_panel_power_initial_config(props);
	if (!r) {
		PSB_DEBUG_INIT("Remove panel power start\n");
		/* Disable all regulators */
		for (i = 0; i < props->board->num_voltage_reg; i++) {
			if (props->board->reg[i].gpio != -1)
				gpio_set_value(props->board->reg[i].gpio,
					!props->board->reg[i].gpio_en_val);
		}

		/* Make sure panel in reset */
		if (props->board->reset_gpio)
			gpio_set_value(props->board->reset_gpio,
				props->reset_gpio_en_val);
		PSB_DEBUG_INIT("Remove panel power complete\n");
	}

	return r;
}
