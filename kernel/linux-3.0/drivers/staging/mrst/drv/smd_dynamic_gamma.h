#include <linux/types.h>
#include <linux/seq_file.h>

#define RAW_MTP_SIZE 24
#define RAW_GAMMA_SIZE 26

#define NUM_VOLT_PTS 7
#define NUM_COLORS   3
#define NUM_NIT_LVLS 30
#define NUM_GRAY_LVLS 256

int smd_dynamic_gamma_calc(uint32_t v0, uint8_t preamble_1, uint8_t preamble_2,
			uint8_t raw_mtp[RAW_MTP_SIZE],
			uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE]);

void smd_dynamic_gamma_dbg_dump(uint8_t raw_mtp[RAW_MTP_SIZE],
				uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE],
				struct seq_file *m, void *data, bool csv);
