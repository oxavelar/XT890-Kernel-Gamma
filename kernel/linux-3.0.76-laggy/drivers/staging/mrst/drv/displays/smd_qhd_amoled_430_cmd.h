#ifndef SMD_QHD_AMOLED_430_CMD_H
#define SMD_QHD_AMOLED_430_CMD_H

#include <drm/drmP.h>
#include "mdfld_output.h"

int smd_qhd_amoled_430_gamma_dbg_dump(struct seq_file *m, void *data);
int smd_qhd_amoled_430_gamma_dbg_dump_csv(struct seq_file *m, void *data);
void smd_qhd_amoled_430_cmd_init(struct drm_device *dev,
				struct panel_funcs *p_funcs);

#endif
