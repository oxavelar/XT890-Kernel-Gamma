/*
 * include/linux/qtouch_obp_ts.h - platform/protocol data for Quantum touch IC
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009-2010 Motorola, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Derived from the Motorola OBP touch driver.
 *
 */

#ifndef _LINUX_QTOUCH_OBP_TS_H
#define _LINUX_QTOUCH_OBP_TS_H

#define QTOUCH_TS_NAME "mxt224e"

#define QTM_OBP_ID_INFO_ADDR		0

#define QTM_OBP_BOOT_CMD_MASK		0xC0
#define QTM_OBP_BOOT_VERSION_MASK	0x3F
#define QTM_OBP_BOOT_WAIT_FOR_DATA	0x80
#define QTM_OBP_BOOT_WAIT_ON_BOOT_CMD	0xC0
#define QTM_OBP_BOOT_CRC_CHECK		0x02
#define QTM_OBP_BOOT_CRC_FAIL		0x03
#define QTM_OBP_BOOT_CRC_PASSED		0x04

#define QTM_OBP_SLEEP_WAIT_FOR_BOOT	100
#define QTM_OBP_SLEEP_WAIT_FOR_RESET	500
#define QTM_OBP_SLEEP_WAIT_FOR_BACKUP	500
#define QTM_OBP_SLEEP_WAIT_FOR_CKSUM	1000
#define QTM_OBP_SLEEP_RESET_HOLD	20
#define QTM_OBP_SLEEP_WAIT_FOR_HW_RESET	40
#define QTM_OBP_SLEEP_WAIT_FOR_DIAG_DATA	5

#define QTM_OBP_FAMILY_XMEGAT	0x81
#define QTM_OBP_NUM_DIAG_READ_RETRIES	10
#define QTM_OBP_DIAG_BUFFER_SIZE	128
#define QTM_OBP_DIAG_NUM_X_LINES	20
#define QTM_OBP_DIAG_CALIB_WAIT_MS	1500

enum {
	QTM_OBJ_RESERVED0		= 0,
	QTM_OBJ_RESERVED1		= 1,
	QTM_OBJ_DBG_DELTAS		= 2,
	QTM_OBJ_DBG_REFS		= 3,
	QTM_OBJ_DBG_SIGS		= 4,
	QTM_OBJ_GEN_MSG_PROC		= 5,
	QTM_OBJ_GEN_CMD_PROC		= 6,
	QTM_OBJ_GEN_PWR_CONF		= 7,
	QTM_OBJ_GEN_ACQUIRE_CONF	= 8,
	QTM_OBJ_TOUCH_MULTI		= 9,
	QTM_OBJ_TOUCH_SINGLE		= 10,
	QTM_OBJ_TOUCH_XSLIDER		= 11,
	QTM_OBJ_TOUCH_SLIDER		= 12,
	QTM_OBJ_TOUCH_XWHEEL		= 13,
	QTM_OBJ_TOUCH_YWHEEL		= 14,
	QTM_OBJ_TOUCH_KEYARRAY		= 15,
	QTM_OBJ_PROCG_SIG_FILTER	= 16,
	QTM_OBJ_PROCI_LINEAR_TBL	= 17,
	QTM_OBJ_SPT_COM_CONFIG		= 18,
	QTM_OBJ_SPT_GPIO_PWM		= 19,
	QTM_OBJ_PROCI_GRIPFACESUPPRESSION = 20,
	QTM_OBJ_RESERVED3		= 21,
	QTM_OBJ_PROCG_NOISE_SUPPRESSION	= 22,
	QTM_OBJ_TOUCH_PROXIMITY		= 23,
	QTM_OBJ_PROCI_ONE_TOUCH_GESTURE_PROC = 24,
	QTM_OBJ_SPT_SELF_TEST		= 25,
	QTM_OBJ_DEBUG_CTE_RANGE		= 26,
	QTM_OBJ_PROCI_TWO_TOUCH_GESTURE_PROC = 27,
	QTM_OBJ_SPT_CTE_CONFIG		= 28,
	QTM_OBJ_NOISESUPPRESSION_1	= 36,
	QTM_OBJ_DBG_DIAG	= 37,
	QTM_OBJ_SPT_USERDATA	= 38,
	QTM_OBJ_PROCI_GRIP_SUPPRESSION2	= 40,
	QTM_OBJ_PROCI_TOUCH_SUPPRESSION2	= 42,
	QTM_OBJ_SPT_MESSAGECOUNT	= 44,
	QTM_OBJ_SPT_CTE_CONFIG2	= 46,
	QTM_OBJ_PROCI_STYLUS	= 47,
	QTM_OBJ_PROCG_NOISE_SUPPRESSION2	= 48,

	/* Max number of objects currently defined */
	QTM_OBP_MAX_OBJECT_NUM = QTM_OBJ_PROCG_NOISE_SUPPRESSION2 + 1,
};

/* OBP structures as defined by the wire protocol. */

/* Note: Not all the structures below need an explicit packed attribute since
 * many of them just contain uint8_t's. However, the protocol is defined in
 * such a way that the structures may expand in the future, with
 * potential multi-byte fields. Thus, we will mark them all as packed to
 * minimize silly bugs in the future.
 */

/* part of the info block */
struct qtm_id_info {
	uint8_t			family_id;
	uint8_t			variant_id;
	uint8_t			version;
	uint8_t			build;
	uint8_t			matrix_x_size;
	uint8_t			matrix_y_size;
	uint8_t			num_objs;
} __attribute__ ((packed));

/* an entry in the ote table */
struct qtm_obj_entry {
	uint8_t			type;
	uint16_t		addr;
	uint8_t			size;
	uint8_t			num_inst;
	uint8_t			num_rids;
} __attribute__ ((packed));


/*******************************/
/*********** messages **********/
/*******************************/

/* generic message received from the message_processor object. size/buffer
 * defined at runtime after reading the info block */
struct qtm_obj_message {
	uint8_t			report_id;
	uint8_t			msg[0];
} __attribute__ ((packed));

/* status message sent by the command processor - T6 */
#define QTM_CMD_PROC_STATUS_RESET	(1 << 7)
#define QTM_CMD_PROC_STATUS_OFL		(1 << 6)
#define QTM_CMD_PROC_STATUS_SIGERR	(1 << 5)
#define QTM_CMD_PROC_STATUS_CAL		(1 << 4)
#define QTM_CMD_PROC_STATUS_CFGERR	(1 << 3)
struct qtm_cmd_proc_msg {
	uint8_t			report_id;
	uint8_t			status;
	uint8_t		checksum[3];
} __attribute__ ((packed));

/* status message sent by the mutlitouch touch object - T9*/
#define QTM_TOUCH_MULTI_STATUS_TOUCH		(1 << 7)
#define QTM_TOUCH_MULTI_STATUS_PRESS		(1 << 6)
#define QTM_TOUCH_MULTI_STATUS_RELEASE		(1 << 5)
#define QTM_TOUCH_MULTI_STATUS_MOVE		(1 << 4)
#define QTM_TOUCH_MULTI_STATUS_VECTOR		(1 << 3)
#define QTM_TOUCH_MULTI_STATUS_AMPLITUDE	(1 << 2)
struct qtm_touch_multi_msg {
	uint8_t			report_id;
	uint8_t			status;
	uint8_t			xpos_msb;
	uint8_t			ypos_msb;
	uint8_t			xypos_lsb;
	uint8_t			touch_area;
	uint8_t			touch_amp;
	uint8_t			touch_vect;
} __attribute__ ((packed));

/* status message sent by the keyarray touch object - T15 */
#define QTM_TOUCH_KEYARRAY_STATUS_TOUCH		(1 << 7)
struct qtm_touch_keyarray_msg {
	uint8_t			report_id;
	uint8_t			status;
	uint32_t		keystate;
} __attribute__ ((packed));



/*******************************/
/**** configuration objects ****/
/*******************************/

/* GEN_COMMANDPROCESSOR_T6 */
struct qtm_gen_cmd_proc {
	uint8_t			reset;
	uint8_t			backupnv;
	uint8_t			calibrate;
	uint8_t			reportall;
	uint8_t			debugctrl;
	uint8_t			diagnostic;
} __attribute__ ((packed));

/* GEN_POWERCONFIG_T7 */
struct qtm_gen_power_cfg {
	uint8_t			idle_acq_int;      /* in ms */
	uint8_t			active_acq_int;    /* in ms */
	uint8_t			active_idle_to;    /* in 200ms */
} __attribute__ ((packed));

/* GEN_ACQUIRECONFIG_T8 */
struct qtm_gen_acquire_cfg {
	uint8_t			charge_time;       /* in 250ns */
	uint8_t			atouch_drift;      /* in 200ms */
	uint8_t			touch_drift;       /* in 200ms */
	uint8_t			drift_susp;        /* in 200ms */
	uint8_t			touch_autocal;     /* in 200ms */
	uint8_t			sync;
	uint8_t			atch_cal_suspend_time;
	uint8_t			atch_cal_suspend_thres;
	uint8_t			atch_force_cal_thres;
	uint8_t			atch_force_cal_ratio;	
} __attribute__ ((packed));

/* TOUCH_MULTITOUCHSCREEN_T9 */
struct qtm_touch_multi_cfg {
	uint8_t			ctrl;
	uint8_t			x_origin;
	uint8_t			y_origin;
	uint8_t			x_size;
	uint8_t			y_size;
	uint8_t			aks_cfg;
	uint8_t			burst_len;
	uint8_t			tch_det_thr;
	uint8_t			tch_det_int;
	uint8_t			orient;
	uint8_t			mrg_to;
	uint8_t			mov_hyst_init;
	uint8_t			mov_hyst_next;
	uint8_t			mov_filter;
	uint8_t			num_touch;
	uint8_t			merge_hyst;
	uint8_t			merge_thresh;
	uint8_t			amp_hyst;
	uint16_t		x_res;
	uint16_t		y_res;
	uint8_t			x_low_clip;
	uint8_t			x_high_clip;
	uint8_t			y_low_clip;
	uint8_t			y_high_clip;
	uint8_t			x_edge_ctrl;
	uint8_t			x_edge_dist;
	uint8_t			y_edge_ctrl;
	uint8_t			y_edge_dist;
	uint8_t			jumplimit;
	uint8_t			tch_thr_hyst;
	uint8_t			xpitch;
	uint8_t			ypitch;	
} __attribute__ ((packed));

/* TOUCH_KEYARRAY_T15 */
struct qtm_touch_keyarray_cfg {
	uint8_t			ctrl;
	uint8_t			x_origin;
	uint8_t			y_origin;
	uint8_t			x_size;
	uint8_t			y_size;
	uint8_t			aks_cfg;
	uint8_t			burst_len;
	uint8_t			tch_det_thr;
	uint8_t			tch_det_int;
	uint8_t			rsvd1;
	uint8_t			rsvd2;
} __attribute__ ((packed));

/* PROCG_SIGNALFILTER_T16 */
struct qtm_procg_sig_filter_cfg {
	uint8_t			slew;
	uint8_t			median;
	uint8_t			iir;
} __attribute__ ((packed));

/* PROCI_LINEARIZATIONTABLE_T17 */
struct qtm_proci_linear_tbl_cfg {
	uint8_t			ctrl;
	uint16_t		x_offset;
	uint8_t			x_segment[16];
	uint16_t		y_offset;
	uint8_t			y_segment[16];
} __attribute__ ((packed));

/* SPT_COMMSCONFIG _T18 */
struct spt_comms_config_cfg {
	uint8_t			ctrl;
	uint8_t			command;
} __attribute__ ((packed));

/* SPT_GPIOPWM_T19*/
struct qtm_spt_gpio_pwm_cfg {
	uint8_t			ctrl;
	uint8_t			report_mask;
	uint8_t			pin_direction;
	uint8_t			internal_pullup;
	uint8_t			output_value;
	uint8_t			wake_on_change;
	uint8_t			pwm_enable;
	uint8_t			pwm_period;
	uint8_t			duty_cycle_0;
	uint8_t			duty_cycle_1;
	uint8_t			duty_cycle_2;
	uint8_t			duty_cycle_3;
	uint8_t			trigger_0;
	uint8_t			trigger_1;
	uint8_t			trigger_2;
	uint8_t			trigger_3;
} __attribute__ ((packed));

/* PROCI_GRIPFACESUPPRESSION_T20 */
struct qtm_proci_grip_suppression_cfg {
	uint8_t			ctrl;
	uint8_t			xlogrip;
	uint8_t			xhigrip;
	uint8_t			ylogrip;
	uint8_t			yhigrip;
	uint8_t			maxtchs;
	uint8_t			reserve0;
	uint8_t			szthr1;
	uint8_t			szthr2;
	uint8_t			shpthr1;
	uint8_t			shpthr2;
	uint8_t			supextto;
} __attribute__ ((packed));

/* PROCG_NOISESUPPRESSION_T22 */
struct qtm_procg_noise_suppression_cfg {
	uint8_t			ctrl;
	uint8_t			outlier_filter_len;
	uint8_t			reserve0;
	uint16_t		gcaf_upper_limit;
	uint16_t		gcaf_lower_limit;
	uint8_t			gcaf_low_count;
	uint8_t			noise_threshold;
	uint8_t			reserve1;
	uint8_t			freq_hop_scale;
	uint8_t			burst_freq_0;
	uint8_t			burst_freq_1;
	uint8_t			burst_freq_2;
	uint8_t			burst_freq_3;
	uint8_t			burst_freq_4;
	uint8_t			idle_gcaf_valid;
} __attribute__ ((packed));

/* TOUCH_PROXIMITY_T23 */
struct qtm_touch_proximity_cfg {
	uint8_t			ctrl;
	uint8_t			x_origin;
	uint8_t			y_origin;
	uint8_t			x_size;
	uint8_t			y_size;
	uint8_t			reserve0;
	uint8_t			blen;
	uint16_t		tch_thresh;
	uint8_t			tch_detect_int;
	uint8_t			average;
	uint16_t		rate;
	uint16_t		mvdthr;
} __attribute__ ((packed));

/* PROCI_ONETOUCHGESTUREPROCESSOR_T24 */
struct qtm_proci_one_touch_gesture_proc_cfg {
	uint8_t			ctrl;
	uint8_t			reserve0;
	uint16_t		gesture_enable;
	uint8_t			pres_proc;
	uint8_t			tap_time_out;
	uint8_t			flick_time_out;
	uint8_t			drag_time_out;
	uint8_t			short_press_time_out;
	uint8_t			long_press_time_out;
	uint8_t			repeat_press_time_out;
	uint16_t		flick_threshold;
	uint16_t		drag_threshold;
	uint16_t		tap_threshold;
	uint16_t		throw_threshold;
} __attribute__ ((packed));

/* SPT_SELFTEST_T25 */
struct qtm_spt_self_test_cfg {
	uint8_t			ctrl;
	uint8_t			command;
	uint16_t		high_signal_limit_0;
	uint16_t		low_signal_limit_0;
	uint16_t		high_signal_limit_1;
	uint16_t		low_signal_limit_1;
	uint16_t		high_signal_limit_2;
	uint16_t		low_signal_limit_2;
} __attribute__ ((packed));

/* PROCI_TWOTOUCHGESTUREPROCESSOR_T27 */
struct qtm_proci_two_touch_gesture_proc_cfg {
	uint8_t			ctrl;
	uint8_t			reserved0;
	uint8_t			reserved1;
	uint8_t			gesture_enable;
	uint8_t			rotate_threshold;
	uint16_t		zoom_threshold;
} __attribute__ ((packed));

/* SPT_CTECONFIG_T28 */
struct qtm_spt_cte_config_cfg {
	uint8_t			ctrl;
	uint8_t			command;
	uint8_t			mode;
	uint8_t			idle_gcaf_depth;
	uint8_t			active_gcaf_depth;
	uint8_t			voltage;
} __attribute__ ((packed));

/* QTM_OBJ_NOISESUPPRESSION_1 */
struct qtm_proci_noise1_suppression_cfg {
	uint8_t			ctrl;
	uint8_t			version;
	uint8_t			atch_thr;
	uint8_t			duty_cycle;
	uint8_t			drift_thr;
	uint8_t			clamp_thr;
	uint8_t			diff_thr;
	uint8_t			adjustment;
	uint16_t		average;
	uint8_t			temp;
	uint8_t			offset[168];
	uint8_t			bad_chan[11];
	uint8_t			x_short;
} __attribute__ ((packed));

/* QTM_OBJ_SPT_USERDATA T38 */
struct qtm_spt_userdata {
	uint8_t			data[8];
} __attribute__ ((packed));

/* QTM_OBJ_PROCI_GRIP_SUPPRESION2 T40 */
struct qtm_proci_grip_suppresion2_cfg {
	uint8_t			ctrl;
	uint8_t			xlogrip;
	uint8_t			xhigrip;
	uint8_t			ylogrip;
	uint8_t			yhigrip;
} __attribute__ ((packed));

/* QTM_OBJ_PROCI_TOUCH_SUPPRESION2 T42 */
struct qtm_proci_touch_suppression2_cfg {
	uint8_t			ctrl;
	uint8_t			appr_thres;
	uint8_t			max_appr_area_thres;
	uint8_t			max_tch_area_thres;
	uint8_t			suppr_str;
	uint8_t			suppr_timeout;
	uint8_t			max_touches;
} __attribute__ ((packed));

/* QTM_OBJ_SPT_CTE_CONFIG2 T46 */
struct qtm_spt_cte_config2_cfg {
	uint8_t			ctrl;
	uint8_t			mode;
	uint8_t			idle_sync_per_x;
	uint8_t			active_sync_per_x;
	uint8_t			adc_per_sync;
	uint8_t			pulse_per_adc;
	uint8_t			x_slew;
} __attribute__ ((packed));

/* QTM_OBJ_PROCI_STYLUS T47 */
struct qtm_proci_stylus_cfg {
	uint8_t			ctrl;
	uint8_t			cont_min;
	uint8_t			cont_max;
	uint8_t			stability;
	uint8_t			max_tch_area;
	uint8_t			amp_thres;
	uint8_t			shape;
	uint8_t			hover_suppr;
	uint8_t			conf_thres;
} __attribute__ ((packed));

/* QTM_OBJ_PROCG_NOISE_SUPPRESSION2 T48 */
struct qtm_procg_noise_suppression2_cfg {
	uint8_t			ctrl;
	uint8_t			cfg;
	uint8_t			cal_cfg;
	uint8_t			base_freq;
	uint8_t			freq0;
	uint8_t			freq1;
	uint8_t			freq2;
	uint8_t			freq3;
	uint8_t			mf_freq_2nd;
	uint8_t			mf_freq_3rd;
	uint8_t			nl_gain;
	uint8_t			nl_thres;
	uint8_t			gc_limit;
	uint8_t			gc_act_inv_thres;
	uint8_t			gc_idle_inv_thres;
	uint8_t			gc_thres_lsb;
	uint8_t			gc_thres_msb;
	uint8_t			gc_max_adc_per_x;
} __attribute__ ((packed));

/*******************************/
/******** platform data ********/
/*******************************/

struct vkey {
	int     code;
	int     center_x;
	int     center_y;
	int     width;
	int     height;
};

struct virt_keys {
	int			count;
	struct vkey		*keys;
};

struct qtouch_key {
	uint8_t				channel;
	int				code;
	int				x_coord;
	int				y_coord;
};

struct qtouch_key_array {
	struct qtm_touch_keyarray_cfg	*cfg;
	struct qtouch_key		*keys;
	int				num_keys;
};

#define QTOUCH_FLIP_X		(1 << 0)
#define QTOUCH_FLIP_Y		(1 << 1)
#define QTOUCH_SWAP_XY		(1 << 2)
#define QTOUCH_USE_MULTITOUCH	(1 << 3)
#define QTOUCH_USE_KEYARRAY	(1 << 4)
#define QTOUCH_CFG_BACKUPNV	(1 << 5)
#define QTOUCH_EEPROM_CHECKSUM  (1 << 6)

struct qtouch_ts_configure_data {
	uint32_t		flags;
	unsigned long		irqflags;

	uint32_t		abs_min_x;
	uint32_t		abs_max_x;
	uint32_t		abs_min_y;
	uint32_t		abs_max_y;
	uint32_t		abs_min_p;
	uint32_t		abs_max_p;
	uint32_t		abs_min_w;
	uint32_t		abs_max_w;
	uint32_t		x_delta;
	uint32_t		y_delta;
	uint32_t		nv_checksum;

	uint32_t		fuzz_x;
	uint32_t		fuzz_y;
	uint32_t		fuzz_p;
	uint32_t		fuzz_w;

	uint8_t			boot_i2c_addr;

	/* TODO: allow multiple key arrays */
	struct qtouch_key_array			key_array;	

	/* object configuration information from board */
	struct qtm_gen_power_cfg			power_cfg;
	struct qtm_gen_acquire_cfg			acquire_cfg;
	struct qtm_touch_multi_cfg			multi_touch_cfg;
	struct qtm_procg_sig_filter_cfg			sig_filter_cfg;
	struct qtm_proci_linear_tbl_cfg			linear_tbl_cfg;
	struct spt_comms_config_cfg			comms_config_cfg;
	struct qtm_spt_gpio_pwm_cfg			gpio_pwm_cfg;
	struct qtm_proci_grip_suppression_cfg		grip_suppression_cfg;
	struct qtm_procg_noise_suppression_cfg		noise_suppression_cfg;
	struct qtm_touch_proximity_cfg			touch_proximity_cfg;
	struct qtm_proci_one_touch_gesture_proc_cfg	one_touch_gesture_proc_cfg;
	struct qtm_spt_self_test_cfg			self_test_cfg;
	struct qtm_proci_two_touch_gesture_proc_cfg	two_touch_gesture_proc_cfg;
	struct qtm_spt_cte_config_cfg			cte_config_cfg;
	struct qtm_proci_noise1_suppression_cfg		noise1_suppression_cfg;
	struct qtm_spt_userdata		userdata;
	struct qtm_proci_grip_suppresion2_cfg	grip_suppression2_cfg;
	struct qtm_proci_touch_suppression2_cfg	touch_suppression2_cfg;
	struct qtm_spt_cte_config2_cfg		cte_config2_cfg;
	struct qtm_proci_stylus_cfg		stylus_cfg;
	struct qtm_procg_noise_suppression2_cfg	noise_suppression2_cfg;

	struct virt_keys	vkeys;	
};

struct touch_fw_entry {
	uint8_t		*image;
	uint32_t	size;
	uint8_t		family_id;
	uint8_t		variant_id;
	uint8_t		fw_version;
	uint8_t		fw_build;
	uint8_t		boot_version;
	uint8_t		base_fw_version;
};

/**
 * struct mxt_platform_data - includes platform specific information
 * related to Atmel maXTouch touchscreen controller.
 *
 * @numtouch:           Number of simultaneous touches supported
 * @init_platform_hw(): Initialization function, which can for example
 *                      trigger a hardware reset by toggling a GPIO pin
 * @exit_platform_hw(): Function to run when the driver is unloaded.
 * @valid_interrupt():  Function that checks the validity of the interrupt -
 *                      function that check the validity of a interrupt (by
 *                      reading the changeline interrupt pin and checking that
 *                      it really is low for example).
 * @max_x:              Reported X range
 * @max_y:              Reported Y range
 */

struct qtouch_ts_platform_data {
	u8    numtouch;	/* Number of touches to report	*/
	void  (*init_platform_hw)(void);
	void  (*exit_platform_hw)(void);
	int   max_x;    /* The default reported X range   */
	int   max_y;    /* The default reported Y range   */
	int   orientation; /* The default orientation */
	u8    (*valid_interrupt) (void);
	u8    (*read_chg) (void);

	u16   reset;
	u16   irq;

};


#endif /* _LINUX_QTOUCH_OBP_TS_H */

