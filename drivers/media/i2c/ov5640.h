#ifndef OV5640_H
#define OV5640_H


#define TAG_NAME	"[OV5640] "
#define cam_err(fmt, ...)	\
	pr_err(TAG_NAME fmt, ##__VA_ARGS__)
#define cam_warn(fmt, ...)	\
	pr_warn(TAG_NAME fmt, ##__VA_ARGS__)
#define cam_info(fmt, ...)	\
	pr_info(TAG_NAME fmt, ##__VA_ARGS__)
#if defined(CONFIG_CAM_DEBUG)
#define cam_dbg(fmt, ...)	\
	pr_debug(TAG_NAME fmt, ##__VA_ARGS__)
#else
#define cam_dbg(fmt, ...)
#endif


/* active pixel array size */
#define OV5640_SENSOR_SIZE_X	2592
#define OV5640_SENSOR_SIZE_Y	1944

#define OV5640_MAX_WIDTH	2560
#define OV5640_MAX_HEIGHT	1920

/* default sizes */
#define OV5640_DEFAULT_WIDTH	1280
#define OV5640_DEFAULT_HEIGHT	760

/* minimum extra blanking */
#define BLANKING_EXTRA_WIDTH		16
#define BLANKING_EXTRA_HEIGHT		4

/*
 * the sensor's autoexposure is buggy when setting total_height low.
 * It tries to expose longer than 1 frame period without taking care of it
 * and this leads to weird output. So we set 1000 lines as minimum.
 */
#define BLANKING_MIN_HEIGHT		1000


#define DEFAULT_SENSOR_WIDTH						1280
#define DEFAULT_SENSOR_HEIGHT						OV5640_MAX_HEIGHT

#define DEFAULT_SENSOR_CODE		MEDIA_BUS_FMT_YUYV8_2X8
#define SENSOR_MEMSIZE	(DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT)


/* OV5642 registers */
#define REG_CHIP_ID_HIGH		0x300a
#define REG_CHIP_ID_LOW			0x300b

#define REG_WINDOW_START_X_HIGH		0x3800
#define REG_WINDOW_START_X_LOW		0x3801
#define REG_WINDOW_START_Y_HIGH		0x3802
#define REG_WINDOW_START_Y_LOW		0x3803
#define REG_WINDOW_WIDTH_HIGH		0x3804
#define REG_WINDOW_WIDTH_LOW		0x3805
#define REG_WINDOW_HEIGHT_HIGH		0x3806
#define REG_WINDOW_HEIGHT_LOW		0x3807
#define REG_OUT_WIDTH_HIGH		0x3808
#define REG_OUT_WIDTH_LOW		0x3809
#define REG_OUT_HEIGHT_HIGH		0x380a
#define REG_OUT_HEIGHT_LOW		0x380b
#define REG_OUT_TOTAL_WIDTH_HIGH	0x380c
#define REG_OUT_TOTAL_WIDTH_LOW		0x380d
#define REG_OUT_TOTAL_HEIGHT_HIGH	0x380e
#define REG_OUT_TOTAL_HEIGHT_LOW	0x380f
#define REG_OUTPUT_FORMAT		0x4300
#define REG_ISP_CTRL_01			0x5001
#define REG_AVG_WINDOW_END_X_HIGH	0x5682
#define REG_AVG_WINDOW_END_X_LOW	0x5683
#define REG_AVG_WINDOW_END_Y_HIGH	0x5686
#define REG_AVG_WINDOW_END_Y_LOW	0x5687

#define REG_DLY 0xffff

#define OV5640_BINNING_MASK		0x01
#define OV5640_TIMING_REG20		0x3820
#define OV5640_TIMING_REG21		0x3821
#define OV5640_HFLIP_MASK		0x06
#define OV5640_VFLIP_MASK		0x06


int sensor_ov5640_probe(struct i2c_client *client,
						const struct i2c_device_id *id);



enum ov5640_frame_rate {
	ov5640_default_fps,
	ov5640_7_fps,
	ov5640_15_fps,
	ov5640_30_fps,
	ov5640_60_fps,
	ov5640_90_fps,
	ov5640_120_fps,
	ov5640_max_fps
};


struct ov5640_timing_cfg {
	u16 x_addr_start;
	u16 y_addr_start;
	u16 x_addr_end;
	u16 y_addr_end;
	u16 h_output_size;
	u16 v_output_size;
	u16 h_total_size;
	u16 v_total_size;
	u16 isp_h_offset;
	u16 isp_v_offset;
	u8 h_odd_ss_inc;
	u8 h_even_ss_inc;
	u8 v_odd_ss_inc;
	u8 v_even_ss_inc;
	u8 out_mode_sel;
	u8 sclk_dividers;
	u8 sys_mipi_clk;
	u8 framerate;
};

struct ov5640_clk_cfg {
	u8 sc_pll_prediv;
	u8 sc_pll_rdiv;
	u8 sc_pll_mult;
	u8 sysclk_div;
	u8 mipi_div;
};

enum ov5640_size {
	OV5640_SIZE_QVGA,
	OV5640_SIZE_VGA,
	OV5640_SIZE_720P,
	OV5640_SIZE_1280x960,
	OV5640_SIZE_UXGA,
	OV5640_SIZE_1080P,
	OV5640_SIZE_QXGA,
	OV5640_SIZE_5MP,
	OV5640_SIZE_LAST,
};

static const struct v4l2_frmsize_discrete ov5640_frmsizes[OV5640_SIZE_LAST] = {
		{320, 240},
		{640, 480},
		{1280, 720},
		{1280, 960},
		{1600, 1200},
		{1929, 1080},
		{2048, 1536},
		{2560, 1920},
};



/* Find a frame size in an array */
static int ov5640_find_framesize(u32 width, u32 height)
{
	int i;

	for (i = 0; i < OV5640_SIZE_LAST; i++) {
		if ((ov5640_frmsizes[i].width >= width) &&
			(ov5640_frmsizes[i].height >= height))
			break;
	}

	/* If not found, select biggest */
	if (i >= OV5640_SIZE_LAST)
		i = OV5640_SIZE_LAST - 1;
	return i;
}


struct ov5640_s {
	unsigned short id;
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *pixel_rate;
	/* HW control */
	unsigned long xvclk;

	struct i2c_client	*client;
	/* System Clock config */
	struct ov5640_clk_cfg clk_cfg;
};

static inline struct ov5640_s *to_ov5640
		(struct v4l2_subdev *subdev)
{
	struct ov5640_s *state =
	container_of(subdev, struct ov5640_s, subdev);
	return state;
}

static inline struct i2c_client *to_client
		(struct v4l2_subdev *subdev)
{
	return (struct i2c_client *)v4l2_get_subdevdata(subdev);
}
/**
 * struct ov5640_reg - ov5640 register format
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * @length: length of the register
 *
 * Define a structure for OV5640 register initialization values
 */
struct ov5640_reg {
	u16	reg;
	u8	val;
};


//#define ENABLE_COLOR_PATTERN
static const struct ov5640_reg configscript_common1[] = {
/* System Control */
		{0x3103, 0x11},
		{0x3008, 0x82},               /* Reset [7] PowerDn [6] */
		{0xFFFF, 3},          /* Sleep 3ms */
		{0x3103, 0x03},		/* PLL Clock Select */

		/* IO Config */
		{0x3017, 0x00},		/* IO [3:0] D9-D6 (MIPI MD1-D9:D8 MC-D7:D6) */
		{0x3018, 0x00},		/* IO [7:2] D5-D0 (MIPI MD0-D5:D4)
				   [1]GPIO1 [0]GPIO0 (MIPI MD2/MC/MD1) */

		/* MIPI Control */
		{0x4800, 0x20},
		{0x3034, 0x18},
		{0x300e, 0x45},		/* MIPI Control  Dual Lane */
		/* CLKS = Src=13Mhz:  676Mbps 8-bit */
		{0x3037, 0x12},		/* PLL Pre-Div [0:3], /2=6.5Mhz
				   PLL Root Div [4] /1=676Mhz */
		{0x3036, 0x58},		/* PLL Mult 4~252 0:7  0x68=104=676Mhz */
		/* PLL ADCLK */
		{0x303d, 0x30},		/*PreDivSp [5:4] /2=6.5Mhz */
		{0x303b, 0x21},		/*DivCntsb [4:0] *30=195Mhz */
		/*???? */
		{0x302e, 0x08},		/*undocumented */
		/*Format control */
		{0x501f, 0x00},		/*ISP Format */
		{0x3821, 0x00},
		/*JPG Control */
		//{0x4713, 0x02},		/*JPG Mode Select */
		/*JPG Quality? */

		{0x4407, 0x04},
		{0x440e, 0x00},
		/*VFIFO Control */
		{0x460b, 0x35},		/*???? */
		{0x460c, 0x22},		/*PCLK Divider Manual */
		/*???? */
		{0x3630, 0x2e},
		{0x3632, 0xe2},
		{0x3633, 0x23},
		/*???? */
		{0x3704, 0xa0},
		{0x3703, 0x5a},
		{0x3715, 0x78},
		{0x3717, 0x01},
		{0x370b, 0x60},
		{0x3705, 0x1a},
		{0x3905, 0x02},
		{0x3906, 0x10},
		{0x3901, 0x0a},
		{0x3731, 0x12},
		/*VCM Control */

		{0x3600, 0x08},
		{0x3601, 0x33},
		{0x3604, 0x02},
		{0x3605, 0x8a},

		/*???? */
		{0x302d, 0x60},
		{0x3620, 0x52},
		{0x371b, 0x20},
		{0x471c, 0x50},
		/*AEC Controls */
		{0x3a13, 0x43},
		{0x3a18, 0x00},
		{0x3a19, 0xf8},
		/*???? */
		{0x3635, 0x1c},
		{0x3634, 0x40},
		{0x3622, 0x01},
		/*50/60Hz Detector */
		{0x3c01, 0x34},
		{0x3c04, 0x28},
		{0x3c05, 0x98},
		{0x3c06, 0x00},

		{0x3c07, 0x08},
		{0x3c08, 0x00},
		{0x3c09, 0x1c},
		{0x3c0a, 0x9c},
		{0x3c0b, 0x40},

		{0x3814, 0x11},		/*X incr */
		{0x3815, 0x11},		/*Y incr */
		/* Sensor Timing control  2624 x 1952 --> 2560 x 1920 */
		{0x3800, 0x00},		/*X start */
		{0x3801, 0x20},
		{0x3802, 0x00},		/*Y start */
		{0x3803, 0x10},
		{0x3804, 0x0a},		/*X end */
		{0x3805, 0x1f},
		{0x3806, 0x07},		/*Y end */
		{0x3807, 0x8f},
		/* Output size */
		{0x3808, 0x0a},		/*output X  2592 */
		{0x3809, 0x20},
		{0x380a, 0x07},		/*output Y  1944 */
		{0x380b, 0x98},
		/* Total size (+blanking) */
		{0x380c, 0x08},		/*Total X  2844 */
		{0x380d, 0x98},
		{0x380e, 0x05},		/*Total Y  1968 */
		{0x380f, 0x00},
		/* ISP Windowing size  2560 x 1920 --> 2560 x 1920 */
		{0x3810, 0x00},		/*ISP X offset = 0 */
		{0x3811, 0x00},
		{0x3812, 0x00},		/*ISP Y offset = 0 */
		{0x3813, 0x00},
		/*???? */
		{0x3618, 0x00},
		{0x3612, 0x29},
		{0x3708, 0x62},
		{0x3709, 0x52},
		{0x370c, 0x03},
		/*AEC/AGC */
		{0x3a02, 0x03},
		{0x3a03, 0xd8},
		{0x3a08, 0x01},
		{0x3a09, 0x27},
		{0x3a0a, 0x00},
		{0x3a0b, 0xf6},
		{0x3a0e, 0x03},
		{0x3a0d, 0x04},
		{0x3a14, 0x03},
		{0x3a15, 0xd8},
		/*BLC Control */
		{0x4001, 0x01},
		{0x4004, 0x02},

		/*ISP Control */
		{0x5000, 0xa7},
		{0x5001, 0xE3}, //23		/*isp scale down  Special Effects */
		/*AWB Control */
		{0x5180, 0xff},
		{0x5181, 0x56},
		{0x5182, 0x00},
		{0x5183, 0x14},
		{0x5184, 0x25},
		{0x5185, 0x24},
		{0x5186, 0x10},
		{0x5187, 0x14},
		{0x5188, 0x10},
		{0x5189, 0x81},
		{0x518a, 0x5a},
		{0x518b, 0xb6},
		{0x518c, 0xa9},
		{0x518d, 0x4c},
		{0x518e, 0x34},
		{0x518f, 0x60},
		{0x5190, 0x48},
		{0x5191, 0xf8},
		{0x5192, 0x04},
		{0x5193, 0x70},
		{0x5194, 0xf0},
		{0x5195, 0xf0},
		{0x5196, 0x03},
		{0x5197, 0x01},
		{0x5198, 0x04},
		{0x5199, 0x9b},
		{0x519a, 0x04},
		{0x519b, 0x00},
		{0x519c, 0x09},
		{0x519d, 0x1e},
		{0x519e, 0x38},
		/*CCM Control */
#if 0
{0x5381, 0x26},
        {0x5382, 0x4e},
        {0x5383, 0x0c},
        {0x5384, 0x12},
        {0x5385, 0x70},
        {0x5386, 0x82},
        {0x5387, 0x6e},
        {0x5388, 0x58},
        {0x5389, 0x14},
        {0x538b, 0x98},
        {0x538a, 0x01},
#else
		{0x5381, 0x20},
		{0x5382, 0x64},
		{0x5383, 0x08},
		{0x5384, 0x30},
		{0x5385, 0x90},
		{0x5386, 0xc0},
		{0x5387, 0xa0},
		{0x5388, 0x98},
		{0x5389, 0x08},
		{0x538b, 0x98},
		{0x538a, 0x01},
#endif
		/*CIP Control */

		{0x5300, 0x08},
		{0x5301, 0x30},
		{0x5302, 0x10},
		{0x5303, 0x00},
		{0x5304, 0x08},
		{0x5305, 0x30},
		{0x5306, 0x08},
		{0x5307, 0x16},
		{0x5309, 0x08},
		{0x530a, 0x30},
		{0x530b, 0x04},
		{0x530c, 0x06},

		/*Gamma Control */

		{0x5480, 0x01},
		{0x5481, 0x08},
		{0x5482, 0x14},
		{0x5483, 0x28},
		{0x5484, 0x51},
		{0x5485, 0x65},
		{0x5486, 0x71},
		{0x5487, 0x7d},
		{0x5488, 0x87},
		{0x5489, 0x91},
		{0x548a, 0x9a},
		{0x548b, 0xaa},
		{0x548c, 0xb8},
		{0x548d, 0xcd},
		{0x548e, 0xdd},
		{0x548f, 0xea},
		{0x5490, 0x1d},

		/*SDE Control */

		{0x5580, 0x02},
		{0x5583, 0x40},
		{0x5584, 0x10},
		{0x5589, 0x10},
		{0x558a, 0x00},
		{0x558b, 0xf8},

		/* LSC OV5640LENCsetting */

		{0x5800, 0x32},
		{0x5801, 0x1d},
		{0x5802, 0x19},
		{0x5803, 0x18},
		{0x5804, 0x1d},
		{0x5805, 0x38},
		{0x5806, 0x12},
		{0x5807, 0x0a},
		{0x5808, 0x07},
		{0x5809, 0x07},
		{0x580a, 0x0b},
		{0x580b, 0x0f},
		{0x580c, 0x0e},
		{0x580d, 0x05},
		{0x580e, 0x01},
		{0x580f, 0x00},
		{0x5810, 0x03},
		{0x5811, 0x0a},
		{0x5812, 0x0c},
		{0x5813, 0x04},
		{0x5814, 0x00},
		{0x5815, 0x00},
		{0x5816, 0x03},
		{0x5817, 0x0a},
		{0x5818, 0x12},
		{0x5819, 0x09},
		{0x581a, 0x06},
		{0x581b, 0x05},
		{0x581c, 0x09},
		{0x581d, 0x12},
		{0x581e, 0x32},
		{0x581f, 0x18},
		{0x5820, 0x14},
		{0x5821, 0x13},
		{0x5822, 0x17},
		{0x5823, 0x2d},
		{0x5824, 0x28},
		{0x5825, 0x2a},
		{0x5826, 0x28},
		{0x5827, 0x28},
		{0x5828, 0x2a},
		{0x5829, 0x28},
		{0x582a, 0x25},
		{0x582b, 0x24},
		{0x582c, 0x24},
		{0x582d, 0x08},
		{0x582e, 0x26},
		{0x582f, 0x42},
		{0x5830, 0x40},
		{0x5831, 0x42},
		{0x5832, 0x06},
		{0x5833, 0x26},
		{0x5834, 0x26},
		{0x5835, 0x44},
		{0x5836, 0x24},
		{0x5837, 0x2a},
		{0x5838, 0x4a},
		{0x5839, 0x2a},
		{0x583a, 0x0c},
		{0x583b, 0x2c},
		{0x583c, 0x28},
		{0x583d, 0xce},

		{0x5025, 0x00},

		/*AEC Controls */

		{0x3a0f, 0x30},
		{0x3a10, 0x28},
		{0x3a1b, 0x30},
		{0x3a1e, 0x26},
		{0x3a11, 0x60},
		{0x3a1f, 0x14},

#ifdef ENABLE_COLOR_PATTERN
{0x503d, 0x80},		/* Solid Colour Bars */
#if 0
	{0x503d, 0x80},		/* Solid Colour Bars */
	{0x503d, 0x81},		/* Gradual change @ vertical mode 1 */
	{0x503d, 0x82},		/* Gradual change horizontal */
	{0x503d, 0x83},		/* Gradual change @ vertical mode 2 */
#endif
#endif

		{0xFFFF, 0x00},
};

static const struct ov5640_reg configscript_common2[] = {
		/* System Clock Div */

		{0x3035, 0x11},		/*SystemClkDiv 7:4, /1=728Mhz
				   MIPI Sclk Div 3:0, /1=728Mhz */
		/*System/IO pad Control */
		{0x3000, 0x00},		/*Resets */
		{0x3002, 0x1c},
		{0x3004, 0xff},		/*Clocks */
		{0x3006, 0xc3},
		{0x3007, 0xff},		/*Clocks */

		{0x3820, 0x40},
		{0x3821, 0x06},

		/*Format control */
		{0x4300, 0x32},		/*Output Format[7:4] Sequence[3:0] (UVYV) */
		/*MIPI Control */
		{0x4837, 0x10},
		{0x480a, 0x00},		/*mipi order */
		{0x4740, 0x21},

		/*PCLK Divider */
		{0x3824, 0x00},		/*Scale Divider [4:0] */
		{0x3008, 0x42},		/*stop sensor streaming */

		{0xFFFF, 0x00}
};


static const struct ov5640_timing_cfg timing_cfg[OV5640_SIZE_LAST] = {
		[OV5640_SIZE_QVGA] = {
				.x_addr_start = 0,
				.y_addr_start = 0,
				.x_addr_end = 2623,
				.y_addr_end = 1951,
				.h_output_size = 320,
				.v_output_size = 240,
				.h_total_size = 2844,
				.v_total_size = 1968,
				.isp_h_offset = 16,
				.isp_v_offset = 6,
				.h_odd_ss_inc = 1,
				.h_even_ss_inc = 1,
				.v_odd_ss_inc = 1,
				.v_even_ss_inc = 1,
				.out_mode_sel	= 0x01,
				.sclk_dividers	= 0x01,
				.sys_mipi_clk	= 0x11,
				.framerate = ov5640_default_fps,
		},
		[OV5640_SIZE_VGA] = {
				.x_addr_start = 0,
				.y_addr_start = 0,
				.x_addr_end = 2623,
				.y_addr_end = 1951,
				.h_output_size = 640,
				.v_output_size = 480,
				.h_total_size = 2844,
				.v_total_size = 1968,
				.isp_h_offset = 16,
				.isp_v_offset = 6,
				.h_odd_ss_inc = 1,
				.h_even_ss_inc = 1,
				.v_odd_ss_inc = 1,
				.v_even_ss_inc = 1,
				.out_mode_sel	= 0x01,
				.sclk_dividers	= 0x01,
				.sys_mipi_clk	= 0x11,
				.framerate = ov5640_default_fps,
		},
		[OV5640_SIZE_720P] = {
				.x_addr_start = 336,
				.y_addr_start = 434,
				.x_addr_end = 2287,
				.y_addr_end = 1522,
				.h_output_size = 1280,
				.v_output_size = 720,
				.h_total_size = 2500,
				.v_total_size = 1120,
				.isp_h_offset = 16,
				.isp_v_offset = 4,
				.h_odd_ss_inc = 1,
				.h_even_ss_inc = 1,
				.v_odd_ss_inc = 1,
				.v_even_ss_inc = 1,
				.out_mode_sel	= 0x01,
				.sclk_dividers	= 0x01,
				.sys_mipi_clk	= 0x11,


				.framerate = ov5640_default_fps,
		},
		[OV5640_SIZE_1080P] = {
				.x_addr_start = 336,
				.y_addr_start = 434,
				.x_addr_end = 2287,
				.y_addr_end = 1522,
				.h_output_size = 1920,
				.v_output_size = 1080,
				.h_total_size = 2500,
				.v_total_size = 1120,
				.isp_h_offset = 16,
				.isp_v_offset = 4,
				.h_odd_ss_inc = 1,
				.h_even_ss_inc = 1,
				.v_odd_ss_inc = 1,
				.v_even_ss_inc = 1,
				.out_mode_sel    = 0x00,
				.sclk_dividers    = 0x02,
				.sys_mipi_clk    = 0x12,
				.framerate = ov5640_default_fps,
		},
		[OV5640_SIZE_5MP] = {
				.x_addr_start = 0,
				.y_addr_start = 0,
				.x_addr_end = 2623,
				.y_addr_end = 1951,
				.h_output_size = 2592,
				.v_output_size = 1944,
				.h_total_size = 2844,
				.v_total_size = 1968,
				.isp_h_offset = 16,
				.isp_v_offset = 6,
				.h_odd_ss_inc = 1,
				.h_even_ss_inc = 1,
				.v_odd_ss_inc = 1,
				.v_even_ss_inc = 1,
				.out_mode_sel	= 0x00,
				.sclk_dividers	= 0x02,
				.sys_mipi_clk	= 0x12,
				.framerate = ov5640_default_fps,
		},
};




#define V4L2_CID_CAM_CONTRAST           (V4L2_CID_CAMERA_CLASS_BASE+42)
enum v4l2_contrast {
	V4L2_CONTRAST_AUTO,
	V4L2_CONTRAST_MINUS_4,
	V4L2_CONTRAST_MINUS_3,
	V4L2_CONTRAST_MINUS_2,
	V4L2_CONTRAST_MINUS_1,
	V4L2_CONTRAST_DEFAULT,
	V4L2_CONTRAST_PLUS_1,
	V4L2_CONTRAST_PLUS_2,
	V4L2_CONTRAST_PLUS_3,
	V4L2_CONTRAST_PLUS_4,
	V4L2_CONTRAST_MAX
};

#define V4L2_CID_CAM_SATURATION         (V4L2_CID_CAMERA_CLASS_BASE+43)
enum v4l2_saturation {
	V4L2_SATURATION_MINUS_3,
	V4L2_SATURATION_MINUS_2,
	V4L2_SATURATION_MINUS_1,
	V4L2_SATURATION_DEFAULT,
	V4L2_SATURATION_PLUS_1,
	V4L2_SATURATION_PLUS_2,
	V4L2_SATURATION_PLUS_3,
	V4L2_SATURATION_MAX
};

#define V4L2_CID_CAM_SHARPNESS          (V4L2_CID_CAMERA_CLASS_BASE+44)
enum v4l2_sharpness {
	V4L2_SHARPNESS_MINUS_2,
	V4L2_SHARPNESS_MINUS_1,
	V4L2_SHARPNESS_DEFAULT,
	V4L2_SHARPNESS_PLUS_1,
	V4L2_SHARPNESS_PLUS_2,
	V4L2_SHARPNESS_MAX,
};

#define V4L2_CID_CAM_ISO    (V4L2_CID_CAMERA_CLASS_BASE+22)
enum v4l2_iso {
	V4L2_ISO_AUTO,
	V4L2_ISO_50,
	V4L2_ISO_100,
	V4L2_ISO_200,
	V4L2_ISO_400,
	V4L2_ISO_800,
	V4L2_ISO_1600,
	V4L2_ISO_MAX
};

#define V4L2_CID_CAPTURE			(V4L2_CID_CAMERA_CLASS_BASE+46)

#define V4L2_CID_CAM_FLASH_MODE			(V4L2_CID_CAMERA_CLASS_BASE+62)
enum v4l2_cam_flash_mode {
	V4L2_FLASH_MODE_BASE,
	V4L2_FLASH_MODE_OFF,
	V4L2_FLASH_MODE_AUTO,
	V4L2_FLASH_MODE_ON,
	V4L2_FLASH_MODE_TORCH,
	V4L2_FLASH_MODE_MAX,
};

#define V4L2_CID_JPEG_QUALITY			(V4L2_CID_CAMERA_CLASS_BASE+55)

#define V4L2_CID_CAM_OBJECT_POSITION_X		(V4L2_CID_CAMERA_CLASS_BASE+50)
#define V4L2_CID_CAM_OBJECT_POSITION_Y		(V4L2_CID_CAMERA_CLASS_BASE+51)

#define V4L2_CID_CAM_SET_AUTO_FOCUS		(V4L2_CID_CAMERA_CLASS_BASE+48)
enum v4l2_cam_auto_focus {
	V4L2_AUTO_FOCUS_OFF = 0,
	V4L2_AUTO_FOCUS_ON,
	V4L2_AUTO_FOCUS_MAX,
};

#define V4L2_CID_CAM_SINGLE_AUTO_FOCUS		(V4L2_CID_CAMERA_CLASS_BASE+63)

#define V4L2_CID_CAM_AUTO_FOCUS_RESULT		(V4L2_CID_CAMERA_CLASS_BASE+54)
enum v4l2_cam_af_status {
	V4L2_CAMERA_AF_STATUS_IN_PROGRESS = 0,
	V4L2_CAMERA_AF_STATUS_SUCCESS,
	V4L2_CAMERA_AF_STATUS_FAIL,
	V4L2_CAMERA_AF_STATUS_1ST_SUCCESS,
	V4L2_CAMERA_AF_STATUS_MAX,
};

#define V4L2_CID_CAM_BRIGHTNESS			(V4L2_CID_CAMERA_CLASS_BASE+45)
enum v4l2_brightness {
	V4L2_BRIGHTNESS_MINUS_2,
	V4L2_BRIGHTNESS_MINUS_1,
	V4L2_BRIGHTNESS_DEFAULT,
	V4L2_BRIGHTNESS_PLUS_1,
	V4L2_BRIGHTNESS_PLUS_2,
	V4L2_BRIGHTNESS_MAX,
};







#endif
