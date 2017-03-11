#include <linux/kernel.h>
#include <linux/time.h>
#include "smd_dynamic_gamma.h"

/* #define VERBOSE_LOGGING */

#ifdef VERBOSE_LOGGING
	#define VDBG_P(fmt, ...) \
		printk(KERN_INFO ""fmt, ##__VA_ARGS__);
#else
	#define VDBG_P(fmt, ...)
#endif

#define ERR_P(fmt, ...) \
	printk(KERN_ERR ""fmt, ##__VA_ARGS__);

/* Voltage point index values */
#define V1   0
#define V15  1
#define V35  2
#define V59  3
#define V87  4
#define V171 5
#define V255 6

#define RED   0
#define GREEN 1
#define BLUE  2

#define NUM_GRAY_LVL_RANGE 6

static uint8_t gray_lvl_index[NUM_NIT_LVLS][NUM_VOLT_PTS] = {
	{1, 3, 7, 13, 19, 36, 54},     /* 10 nits */
	{1, 4, 10, 17, 25, 50, 74},    /* 20 nits */
	{1, 5, 12, 21, 31, 60, 90},    /* 30 nits */
	{1, 6, 14, 24, 35, 68, 102},   /* 40 nits */
	{1, 7, 15, 26, 39, 76, 113},   /* 50 nits */
	{1, 7, 17, 28, 42, 82, 123},   /* 60 nits */
	{1, 8, 18, 30, 45, 88, 132},   /* 70 nits */
	{1, 8, 19, 32, 48, 94, 140},   /* 80 nits */
	{1, 9, 20, 34, 50, 99, 148},  /* 90 nits */
	{1, 9, 21, 36, 53, 104, 155},  /* 100 nits */
	{1, 9, 22, 37, 55, 108, 162}, /* 110 nits */
	{1, 10, 23, 39, 57, 113, 168}, /* 120 nits */
	{1, 10, 24, 40, 59, 117, 174}, /* 130 nits */
	{1, 11, 25, 42, 62, 121, 180}, /* 140 nits */
	{1, 11, 26, 43, 63, 125, 186}, /* 150 nits */
	{1, 11, 26, 44, 65, 128, 192}, /* 160 nits */
	{1, 12, 27, 46, 67, 132, 197}, /* 170 nits */
	{1, 12, 28, 47, 69, 136, 202}, /* 180 nits */
	{1, 12, 28, 48, 71, 139, 207}, /* 190 nits */
	{1, 12, 29, 49, 72, 142, 212}, /* 200 nits */
	{1, 13, 30, 50, 74, 145, 217}, /* 210 nits */
	{1, 13, 30, 51, 76, 149, 221}, /* 220 nits */
	{1, 13, 31, 52, 77, 152, 226}, /* 230 nits */
	{1, 14, 32, 53, 79, 155, 230}, /* 240 nits */
	{1, 14, 32, 54, 80, 157, 235}, /* 250 nits */
	{1, 14, 33, 55, 82, 160, 239}, /* 260 nits */
	{1, 14, 33, 56, 83, 163, 243}, /* 270 nits */
	{1, 15, 34, 57, 84, 166, 247}, /* 280 nits */
	{1, 15, 34, 58, 86, 168, 251}, /* 290 nits */
	{1, 15, 35, 59, 87, 171, 255}  /* 300 nits */
};

/* Gray level index values not to calculate, and the voltage
   point value to use */
const uint8_t gray_lvl_non_calc[][2] = {
	{1, V1},
	{15, V15},
	{35, V35},
	{59, V59},
	{87, V87},
	{171, V171},
	{255, V255}
};

/* Gray level calculation range start and end points */
const uint8_t gray_lvl_range[NUM_GRAY_LVL_RANGE][2] = {
	{2, 15},
	{16, 35},
	{36, 59},
	{60, 87},
	{88, 171},
	{172, 255}
};

/* Voltage points used to calculate gray level ranges */
const uint8_t gray_lvl_v_pt[NUM_GRAY_LVL_RANGE][2] = {
	{V1, V15},
	{V15, V35},
	{V35, V59},
	{V59, V87},
	{V87, V171},
	{V171, V255}
};

/* Denominators to use for each gray level range */
const uint8_t gray_lvl_denom[NUM_GRAY_LVL_RANGE] = {
	52, 70, 24, 28, 84, 84
};

/* Numerators to use for V1 <-> V15 rage */
const uint8_t v1_v15_numerators[] = {
	47, 42, 37, 32, 27, 23, 19, 15, 12, 9, 6, 4, 2
};

/* Numerators to use for V15 <-> V35 rage */
const uint8_t v15_v35_numerators[] = {
	66, 62, 58, 54, 50, 46, 42, 38, 34, 30, 27, 24, 21,
	18, 15, 12, 9, 6, 3
};

/* Indicates if gray level calculation uses computed or hardcode numerator */
const uint8_t * const gray_lvl_numer_ptr[NUM_GRAY_LVL_RANGE] = {
	v1_v15_numerators,
	v15_v35_numerators,
	NULL,
	NULL,
	NULL,
	NULL
};

uint32_t full_gray_lvl_volt[NUM_GRAY_LVLS][NUM_COLORS];

/* Parse raw MTP data from panel into signed values */
static void parse_raw_mtp(uint8_t raw_mtp[RAW_MTP_SIZE],
			int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS])
{
	int color;
	int volt;
	uint16_t temp;
	int16_t mtp_val;
	uint8_t *raw_ptr = raw_mtp;

	for (color = 0; color < NUM_COLORS; color++) {
		for (volt = 0; volt < NUM_VOLT_PTS; volt++) {
			if (volt != 6) {
				if (0x80 & *raw_ptr)
					mtp_val = -(((~*raw_ptr) + 1) & 0xFF);
				else
					mtp_val = *raw_ptr;

				VDBG_P("Parse MTP: raw value = 0x%04x, "
					"final = %d\n",
					*raw_ptr, mtp_val);
			} else {
				temp = *raw_ptr << 8;
				raw_ptr++;
				temp |= *raw_ptr;
				if (temp & 0x100)
					mtp_val = -(((~temp) + 1) & 0x1FF);
				else
					mtp_val = temp;

				VDBG_P("Parse MTP: raw value = 0x%04x, "
					"final = %d\n",
					temp, mtp_val);
			}
			raw_ptr++;
			mtp_table[volt][color] = mtp_val;
		}
	}
}

/* Applies MTP values to gamma settings */
static int apply_mtp_to_gamma(int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS],
			uint16_t pre_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint16_t post_gamma[NUM_VOLT_PTS][NUM_COLORS],
			bool add)
{
	int16_t temp;
	int color;
	int i;
	int r = 0;

	for (color = 0; color < NUM_COLORS; color++) {
		for (i = 0; i < NUM_VOLT_PTS; i++) {
			if (add)
				temp = pre_gamma[i][color] +
					mtp_table[i][color];
			else
				temp = pre_gamma[i][color] -
					mtp_table[i][color];

			if ((i != 6) && ((temp < 0) || (temp > 255))) {
				ERR_P("ERROR: Converted gamma value out of "
					"range, value = %d\n",
					temp);
				r = -EINVAL;
				temp = (temp < 0) ? 0 : 255;
			} else if ((i == 6) && ((temp < 0) || (temp > 511))) {
				ERR_P("ERROR: Converted gamma value out of "
					"range, value = %d\n",
					temp);
				r = -EINVAL;
				temp = (temp < 0) ? 0 : 511;
			}

			post_gamma[i][color] = (uint16_t) temp;

			VDBG_P("Apply MTP: MTP = %d, 0x%04x (%d) -> 0x%04x "
				"(%d)\n",
				mtp_table[i][color], pre_gamma[i][color],
				pre_gamma[i][color], post_gamma[i][color],
				post_gamma[i][color]);
		}
	}

	return r;
}

/* Converts gamma values to voltage level points */
static void convert_gamma_to_volt_pts(uint32_t v0_val,
				uint16_t g[NUM_VOLT_PTS][NUM_COLORS],
				uint32_t v[NUM_VOLT_PTS][NUM_COLORS])
{
	int color = 0;

	for (color = 0; color < NUM_COLORS; color++) {
		v[V1][color] = v0_val -
			DIV_ROUND_CLOSEST(((v0_val * (g[V1][color] + 5))),
					600);
		v[V255][color] = v0_val -
			DIV_ROUND_CLOSEST(((v0_val * (g[V255][color] + 100))),
					600);
		v[V171][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V255][color]) *
					(g[V171][color] + 65)), 320);
		v[V87][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V171][color]) *
					(g[V87][color] + 65)), 320);
		v[V59][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V87][color]) *
					(g[V59][color] + 65)), 320);
		v[V35][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V59][color]) *
					(g[V35][color] + 65)), 320);
		v[V15][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V35][color]) *
					(g[V15][color] + 20)), 320);
	}

#ifdef VERBOSE_LOGGING
	{
		int i;
		for (i = 0; i < 7; i++) {
			VDBG_P("Gamma -> Voltage Point: "
				"R = %d, G = %d, B = %d\n",
				v[i][0], v[i][1], v[i][2]);
		}
	}
#endif

}

/* Calculates voltage values for all gray levels */
static void calc_gray_lvl_volt(uint32_t v0_val,
			uint32_t in_v[NUM_VOLT_PTS][NUM_COLORS],
			uint32_t gray_v[NUM_GRAY_LVLS][NUM_COLORS])
{
	int i;
	int j;
	uint8_t numer;
	uint8_t denom;
	const uint8_t *numer_ptr = NULL;
	int gray_level;
	uint32_t in_v_btm[NUM_COLORS];
	uint32_t in_v_top[NUM_COLORS];
	uint32_t in_v_diff[NUM_COLORS];


	/* Level 0 is always V0 */
	gray_v[0][RED] = v0_val;
	gray_v[0][GREEN] = v0_val;
	gray_v[0][BLUE] = v0_val;

	/* Set all of the non-calculated gray levels */
	for (i = 0; i < ARRAY_SIZE(gray_lvl_non_calc); i++) {
		gray_v[gray_lvl_non_calc[i][0]][RED] =
			in_v[gray_lvl_non_calc[i][1]][RED];
		gray_v[gray_lvl_non_calc[i][0]][GREEN] =
			in_v[gray_lvl_non_calc[i][1]][GREEN];
		gray_v[gray_lvl_non_calc[i][0]][BLUE] =
			in_v[gray_lvl_non_calc[i][1]][BLUE];
	}

	for (i = 0; i < ARRAY_SIZE(gray_lvl_range); i++) {
		denom = gray_lvl_denom[i];

		/* Determine if numerator is hardcoded or calculated */
		if (gray_lvl_numer_ptr[i] == NULL) {
			numer = denom - 1;
		} else {
			numer_ptr = gray_lvl_numer_ptr[i];
			numer = numer_ptr[0];
		}

		for (j = 0; j < NUM_COLORS; j++) {
			in_v_top[j] = in_v[gray_lvl_v_pt[i][0]][j];
			in_v_btm[j] = in_v[gray_lvl_v_pt[i][1]][j];
			in_v_diff[j] = in_v_top[j] - in_v_btm[j];
		}

		for (gray_level = gray_lvl_range[i][0];
		     gray_level < gray_lvl_range[i][1];
		     gray_level++) {

			gray_v[gray_level][RED] =
				in_v_btm[RED] +
				DIV_ROUND_CLOSEST((in_v_diff[RED] * numer),
						denom);

			gray_v[gray_level][GREEN] =
				in_v_btm[GREEN] +
				DIV_ROUND_CLOSEST((in_v_diff[GREEN] * numer),
						denom);

			gray_v[gray_level][BLUE] =
				in_v_btm[BLUE] +
				DIV_ROUND_CLOSEST((in_v_diff[BLUE] * numer),
						denom);

			if (gray_lvl_numer_ptr[i] == NULL)
				numer--;
			else
				numer = *(++numer_ptr);
		}
	}

#ifdef VERBOSE_LOGGING
	{
		int i;
		for (i = 0; i < NUM_GRAY_LVLS; i++) {
			VDBG_P("Calc gray level %d: Voltage "
				"R = %d, G = %d, B = %d\n",
				i, gray_v[i][RED],
				gray_v[i][GREEN], gray_v[i][BLUE]);
		}
	}
#endif

}

static void populate_out_gamma(uint32_t v0_val,
			uint8_t preamble_1, uint8_t preamble_2,
			int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS],
			uint32_t gray_v[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_g[NUM_NIT_LVLS][RAW_GAMMA_SIZE])
{
	uint8_t nit_index;
	uint8_t v_pt;
	uint32_t temp_v[NUM_VOLT_PTS][NUM_COLORS];
	uint8_t color_index;
	uint16_t temp_g[NUM_VOLT_PTS][NUM_COLORS];
	uint16_t final_g[NUM_VOLT_PTS][NUM_COLORS];
	uint8_t *out_ptr = NULL;

	for (nit_index = 0; nit_index < NUM_NIT_LVLS; nit_index++) {
		/* Use the desired gray level values for the voltage points */
		for (v_pt = 0; v_pt < NUM_VOLT_PTS; v_pt++) {
			temp_v[v_pt][RED] =
				gray_v[gray_lvl_index[nit_index][v_pt]][RED];
			temp_v[v_pt][GREEN] =
				gray_v[gray_lvl_index[nit_index][v_pt]][GREEN];
			temp_v[v_pt][BLUE] =
				gray_v[gray_lvl_index[nit_index][v_pt]][BLUE];
		}

		/* Translate voltage value to gamma value */
		for (color_index = 0; color_index < NUM_COLORS; color_index++) {
			temp_g[V1][color_index] =
				(((v0_val - temp_v[V1][color_index]) * 600)
					/ v0_val) - 5;

			temp_g[V255][color_index] =
				(((v0_val - temp_v[V255][color_index]) * 600)
					/ v0_val) - 100;

			temp_g[V171][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V171][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V255][color_index])) - 65;

			temp_g[V87][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V87][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V171][color_index])) - 65;

			temp_g[V59][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V59][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V87][color_index])) - 65;

			temp_g[V35][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V35][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V59][color_index])) - 65;

			temp_g[V15][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V15][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V35][color_index])) - 20;
		}

#ifdef VERBOSE_LOGGING
		{
			int i;
			for (i = 0; i < 7; i++) {
				VDBG_P("%d nits: Voltage (Gamma) R = %d (%d), "
					"G = %d (%d), B = %d (%d)\n",
					(10 + (nit_index * 10)), temp_v[i][RED],
					temp_g[i][RED],
					temp_v[i][GREEN], temp_g[i][GREEN],
					temp_v[i][BLUE], temp_g[i][BLUE]);
			}
		}
#endif

		/* Remove MTP offset from value */
		apply_mtp_to_gamma(mtp_table, temp_g, final_g, false);

		out_g[nit_index][0] = preamble_1;
		out_g[nit_index][1] = preamble_2;

		out_ptr = &out_g[nit_index][2];
		for (v_pt = 0; v_pt < NUM_VOLT_PTS; v_pt++) {
			if (v_pt != 6) {
				out_ptr[(v_pt * NUM_COLORS) + 0] =
					(uint8_t) final_g[v_pt][0];
				out_ptr[(v_pt * NUM_COLORS) + 1] =
					(uint8_t) final_g[v_pt][1];
				out_ptr[(v_pt * NUM_COLORS) + 2] =
					(uint8_t) final_g[v_pt][2];
			} else {
				out_ptr[(v_pt * NUM_COLORS) + 0] =
					(uint8_t) ((final_g[v_pt][0] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 1] =
					(uint8_t) (final_g[v_pt][0] & 0xFF);
				out_ptr[(v_pt * NUM_COLORS) + 2] =
					(uint8_t) ((final_g[v_pt][1] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 3] =
					(uint8_t) (final_g[v_pt][1] & 0xFF);
				out_ptr[(v_pt * NUM_COLORS) + 4] =
					(uint8_t) ((final_g[v_pt][2] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 5] =
					(uint8_t) (final_g[v_pt][2] & 0xFF);
			}
		}
	}
}

void smd_dynamic_gamma_dbg_dump(uint8_t raw_mtp[RAW_MTP_SIZE],
				uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE],
				struct seq_file *m, void *data, bool csv)
{
	int row;
	int col;

	seq_printf(m, "Raw MTP Data:%c", csv ? ',' : ' ');
	for (col = 0; col < RAW_MTP_SIZE; col++)
		seq_printf(m, "0x%02x%c", raw_mtp[col], csv ? ',' : ' ');
	seq_printf(m, "\n\n");

	seq_printf(m, "Final Gamma Data:\n");
	for (row = 0; row < NUM_NIT_LVLS; row++) {
		seq_printf(m, "%d nits:%c", 10 + (row * 10), csv ? ',' : ' ');
		for (col = 0; col < RAW_GAMMA_SIZE; col++) {
			seq_printf(m, "0x%02x%c",
				out_gamma[row][col], csv ? ',' : ' ');
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nFinal Gamma Data DDC Format:\n");
	for (row = NUM_NIT_LVLS - 1; row != -1; row--) {
		seq_printf(m, "%d nits:%c", 10 + (row * 10), csv ? ',' : ' ');
		for (col = 0; col < RAW_GAMMA_SIZE; col++) {
			seq_printf(m, "%02X%c",
				out_gamma[row][col], csv ? ',' : ' ');
		}
		seq_printf(m, "\n");
	}
}

int smd_dynamic_gamma_calc(uint32_t v0_val, uint8_t preamble_1,
			uint8_t preamble_2,
			uint8_t raw_mtp[RAW_MTP_SIZE],
			uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE])
{
	int16_t mtp_offset[NUM_VOLT_PTS][NUM_COLORS];
	uint16_t in_gamma_mtp[NUM_VOLT_PTS][NUM_COLORS];
	uint32_t in_gamma_volt_pts[NUM_VOLT_PTS][NUM_COLORS];

	parse_raw_mtp(raw_mtp, mtp_offset);

	if (apply_mtp_to_gamma(mtp_offset, in_gamma, in_gamma_mtp, true) != 0) {
		ERR_P("Failed apply MTP offset, use 0'd values\n");
		memset(mtp_offset, 0, sizeof(mtp_offset));
		apply_mtp_to_gamma(mtp_offset, in_gamma, in_gamma_mtp, true);
	}

	convert_gamma_to_volt_pts(v0_val, in_gamma_mtp, in_gamma_volt_pts);

	calc_gray_lvl_volt(v0_val, in_gamma_volt_pts, full_gray_lvl_volt);

	populate_out_gamma(v0_val, preamble_1, preamble_2,
			mtp_offset, full_gray_lvl_volt, out_gamma);

#ifdef VERBOSE_LOGGING
	{
		uint8_t nit_index;
		for (nit_index = 0; nit_index < NUM_NIT_LVLS; nit_index++) {
			VDBG_P("%d nits:\n", (nit_index * 10) + 10);
			print_hex_dump(KERN_INFO, "gamma data = ",
				DUMP_PREFIX_NONE,
				32, 1, out_gamma[nit_index],
				RAW_GAMMA_SIZE, false);
		}
	}
#endif

	return 0;
}
