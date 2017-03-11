#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "ov5640.h"


#define SENSOR_NAME "OV5640"



/**
 * ov5640_reg_read - Read a value from a register in an ov5640 sensor device
 * @client: i2c driver client structure
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an ov5640 sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5640_reg_read(struct i2c_client *client,  u16 reg, u8 *val)
{
    int ret;
    u8 data[2] = {0};
    struct i2c_msg msg = {
            .addr	= client->addr,
            .flags	= 0,
            .len	= 2,
            .buf	= data,
    };

    data[0] = (u8)(reg >> 8);
    data[1] = (u8)(reg & 0xff);

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0)
        goto err;

    msg.flags = I2C_M_RD|I2C_CLIENT_SCCB;
    msg.len = 1;
    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0)
        goto err;

    *val = data[0];
    return 0;

    err:
    dev_err(&client->dev, "Failed reading register 0x%02x!\n", reg);
    return ret;
}

static int ov5640_i2c_read_twobyte(struct i2c_client *client,
                                   u16 subaddr, u16 *data)
{
    int err;
    unsigned char buf[2];
    struct i2c_msg msg[2];
    buf[0]=0;
    buf[1]=0;

    cpu_to_be16s(&subaddr);

    msg[0].addr = client->addr;
    msg[0].flags = I2C_CLIENT_SCCB;
    msg[0].len = 2;
    msg[0].buf = (u8 *)&subaddr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD|I2C_CLIENT_SCCB;
    msg[1].len = 2;
    msg[1].buf = buf;

    err = i2c_transfer(client->adapter, msg, 2);
    if (unlikely(err != 2)) {
        dev_err(&client->dev,
                "%s: register read fail (%d)\n", __func__,err);
        return -EIO;
    }

    *data = ((buf[0] << 8) | buf[1]);

    return 0;
}


/**
 * Write a value to a register in ov5640 sensor device.
 * @client: i2c driver client structure.
 * @reg: Address of the register to read value from.
 * @val: Value to be written to a specific register.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5640_reg_write(struct i2c_client *client, u32 reg, u8 val)
{
    struct i2c_msg msg[1];
    u8 *array = (u8*)&reg;
    u8 wbuf[3];
    int ret;

    if (!client->adapter) {
        cam_err("Could not find adapter!\n");
        return -ENODEV;
    }

    msg->addr = client->addr;
    msg->flags = client->flags;
    msg->len = 3;
    msg->buf = wbuf;
    wbuf[0] = array[1];
    wbuf[1] = array[0];
    wbuf[2] = val;

    ret = i2c_transfer(client->adapter, msg, 1);
    if (ret < 0) {
        cam_err("i2c treansfer fail(%d)", ret);
        return ret;
    }

    return 0;

}

/**
 * Initialize a list of ov5640 registers.
 * The list of registers is terminated by the pair of values
 * @client: i2c driver client structure.
 * @reglist[]: List of address of the registers to write data.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5640_reg_writes(struct i2c_client *client,
                             const struct ov5640_reg reglist[],
                             int size)
{
    int err = 0, i;

    for (i = 0; i < size; i++) {
        if(reglist[i].reg == 0xFFFF){
            if(reglist[i].val != 0xFF){
                mdelay(reglist[i].val);
            }else{
                break;
            }
        }else{
            err = ov5640_reg_write(client, reglist[i].reg,
                                   reglist[i].val);
        }
        if (err)
            return err;
    }
    return 0;
}

static int ov5640_reg_set(struct i2c_client *client, u16 reg, u8 val)
{
    int ret;
    u8 tmpval = 0;

    ret = ov5640_reg_read(client, reg, &tmpval);
    if (ret)
        return ret;

    return ov5640_reg_write(client, reg, tmpval | val);
}

static int ov5640_reg_clr(struct i2c_client *client, u16 reg, u8 val)
{
    int ret;
    u8 tmpval = 0;

    ret = ov5640_reg_read(client, reg, &tmpval);
    if (ret)
        return ret;

    return ov5640_reg_write(client, reg, tmpval & ~val);
}

static unsigned long ov5640_get_pclk(struct v4l2_subdev *sd)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    unsigned long xvclk, vco, mipi_pclk;

    xvclk = ov5640->xvclk;

    vco = (xvclk / ov5640->clk_cfg.sc_pll_prediv) *
          ov5640->clk_cfg.sc_pll_mult;

    mipi_pclk = vco /
                ov5640->clk_cfg.sysclk_div /
                ov5640->clk_cfg.mipi_div;

    return mipi_pclk;
}


static int ov5640_config_timing(struct v4l2_subdev *sd)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    struct i2c_client *client = ov5640->client;;

    int ret, i;
    u8 val;

    i = ov5640_find_framesize(ov5640->format.width, ov5640->format.height);
    ret = ov5640_reg_write(client,
                           0x3800,
                           (timing_cfg[i].x_addr_start & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3801,
                           timing_cfg[i].x_addr_start & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3802,
                           (timing_cfg[i].y_addr_start & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3803,
                           timing_cfg[i].y_addr_start & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3804,
                           (timing_cfg[i].x_addr_end & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3805,
                           timing_cfg[i].x_addr_end & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3806,
                           (timing_cfg[i].y_addr_end & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3807,
                           timing_cfg[i].y_addr_end & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3808,
                           (timing_cfg[i].h_output_size & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3809,
                           timing_cfg[i].h_output_size & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380A,
                           (timing_cfg[i].v_output_size & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380B,
                           timing_cfg[i].v_output_size & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380C,
                           (timing_cfg[i].h_total_size & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380D,
                           timing_cfg[i].h_total_size & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380E,
                           (timing_cfg[i].v_total_size & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x380F,
                           timing_cfg[i].v_total_size & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3810,
                           (timing_cfg[i].isp_h_offset & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3811,
                           timing_cfg[i].isp_h_offset & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3812,
                           (timing_cfg[i].isp_v_offset & 0xFF00) >> 8);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3813,
                           timing_cfg[i].isp_v_offset & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3814,
                           ((timing_cfg[i].h_odd_ss_inc & 0xF) << 4) |
                           (timing_cfg[i].h_even_ss_inc & 0xF));
    if (ret)
        return ret;

    ret = ov5640_reg_write(client,
                           0x3815,
                           ((timing_cfg[i].v_odd_ss_inc & 0xF) << 4) |
                           (timing_cfg[i].v_even_ss_inc & 0xF));

    ov5640_reg_read(client, OV5640_TIMING_REG21, &val);
    if (timing_cfg[i].out_mode_sel & OV5640_BINNING_MASK)
        val |= OV5640_BINNING_MASK;
    else
        val &= ~(OV5640_BINNING_MASK);

    if (0)
        val |= OV5640_HFLIP_MASK;
    else
        val &= ~(OV5640_HFLIP_MASK);

    ret = ov5640_reg_write(client, OV5640_TIMING_REG21, val);
    if (ret)
        return ret;


    ret = ov5640_reg_write(client,
                           0x3108, timing_cfg[i].sclk_dividers & 0xFF);
    if (ret)
        return ret;

    ret = ov5640_reg_write(client, 0x3035, timing_cfg[i].sys_mipi_clk & 0xFF);
    if (ret)
        return ret;
/*
    ov5640_reg_writes(client, ov5640_fps_30,
                      ARRAY_SIZE(ov5640_fps_30));
*/
    return ret;
}


static int sensor_ov5640_s_power(struct v4l2_subdev *sd, int on)
{
    int ret= 0;

    return ret;
}


static int sensor_ov5640_init(struct v4l2_subdev *sd)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    struct i2c_client *client = ov5640->client;
    cam_err("%s\n", __func__);

    dev_err(&client->dev, "OV5640 init\n");
    int ret = 0;
    u8 revision = 0;
    u8 msb;
    u8 lsb;

    ret = sensor_ov5640_s_power(sd, 1);
    if (ret < 0) {
        dev_err(&client->dev, "OV5640 power up failed\n");
        return ret;
    }

    ret = ov5640_reg_read(client, 0x302A, &revision);
    if (ret) {
        dev_err(&client->dev, "Failure to detect OV5640 chip\n");
        goto out;
    }

    revision &= 0xF;

    dev_info(&client->dev, "Detected a OV5640 chip, revision %x\n",
             revision);

    /* SW Reset */
    ret = ov5640_reg_set(client, 0x3008, 0x80);
    if (ret)
        goto out;

    msleep(2);

    ret = ov5640_reg_clr(client, 0x3008, 0x80);
    if (ret)
        goto out;

    /* SW Powerdown */
    ret = ov5640_reg_set(client, 0x3008, 0x40);
    if (ret)
        goto out;

    /* Check chip id */
    ret = ov5640_reg_read(client, 0x300A, &msb);
    if (!ret)
        ret = ov5640_reg_read(client, 0x300B, &lsb);
    if (!ret) {
        ov5640->id = (msb << 8) | lsb;
        if (ov5640->id == 0x5640)
            dev_info(&client->dev, "Sensor ID: %04X\n", ov5640->id);
        else
            dev_err(&client->dev, "Sensor detection failed (%04X, %d)\n",
                    ov5640->id, ret);
    }

    ret = ov5640_reg_writes(client, configscript_common1,
                            ARRAY_SIZE(configscript_common1));
    if (ret)
        goto out;

    ret = ov5640_reg_writes(client, configscript_common2,
                            ARRAY_SIZE(configscript_common2));
    if (ret)
        goto out;


    cam_info("%s\n",__func__);

out:
    sensor_ov5640_s_power(sd, 0);
    return ret;


}


static int sensor_ov5640_s_parm(struct v4l2_subdev *sd,
                         struct v4l2_streamparm *param)
{
    int ret = 0;
    struct v4l2_captureparm *cp;
    struct v4l2_fract *tpf;
    u64 framerate;

    BUG_ON(!sd);
    BUG_ON(!param);

    cp = &param->parm.capture;
    tpf = &cp->timeperframe;

    if (!tpf->numerator) {
        cam_err("numerator is 0");
        ret = -EINVAL;
        goto p_err;
    }

    framerate = tpf->denominator;
/*
    ret = sensor_ov5640_s_duration(sd, framerate);
    if (ret) {
        cam_err("s_duration is fail(%d)", ret);
        goto p_err;
    }
*/
    p_err:
    return ret;
}


static int sensor_ov5640_enum_framesizes(struct v4l2_subdev *subdev,
                                  struct v4l2_subdev_fh *fh,
                                  struct v4l2_subdev_frame_size_enum *fse)
{
    cam_info("%s %d %d\n", __func__,fse->index,fse->code);
    if ((fse->index >= OV5640_SIZE_LAST) ||
        (fse->code != MEDIA_BUS_FMT_UYVY8_2X8  &&
         fse->code != MEDIA_BUS_FMT_YUYV8_2X8))
        return -EINVAL;
    cam_info("%s\n", __func__);
    fse->min_width = ov5640_frmsizes[fse->index].width;
    fse->max_width = fse->min_width;
    fse->min_height = ov5640_frmsizes[fse->index].height;
    fse->max_height = fse->min_height;

    return 0;
}


static struct v4l2_mbus_framefmt *
__ov5640_get_pad_format(struct ov5640_s *ov5640, struct v4l2_subdev_fh *fh,
                        unsigned int pad, enum v4l2_subdev_format_whence which)
{
    switch (which) {
        case V4L2_SUBDEV_FORMAT_TRY:
            return v4l2_subdev_get_try_format(fh, pad,0);
        case V4L2_SUBDEV_FORMAT_ACTIVE:
            return &ov5640->format;
        default:
            return NULL;
    }
}

static int sensor_ov5640_get_fmt(struct v4l2_subdev *sd,
                          struct v4l2_subdev_fh *fh,
                          struct v4l2_subdev_format *format)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    cam_info("%s\n", __func__);

    format->format = *__ov5640_get_pad_format(ov5640, fh, format->pad,
                                              format->which);

    return 0;
}

static int sensor_ov5640_set_fmt(struct v4l2_subdev *sd,
                         struct v4l2_subdev_fh *fh,
                         struct v4l2_subdev_format *format)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    struct v4l2_mbus_framefmt *__format;

    __format = __ov5640_get_pad_format(ov5640, fh, format->pad,
                                       format->which);

    ov5640->pixel_rate->cur.val = (s32)(ov5640_get_pclk(sd) / 16);

    return 0;
}


static int sensor_ov5640_enum_mbus_code(struct v4l2_subdev *sd,
                                 struct v4l2_subdev_fh *fh,
                                 struct v4l2_subdev_mbus_code_enum *code)
{
    cam_info("%s\n", __func__);
    if (code->index >= 2)
        return -EINVAL;

    switch (code->index) {
        case 0:
            code->code = MEDIA_BUS_FMT_UYVY8_2X8 ;
            break;
        case 1:
            code->code = MEDIA_BUS_FMT_YUYV8_2X8 ;
            break;
    }

    return 0;
}




static int sensor_ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    struct i2c_client *client = ov5640->client;
    int ret = 0;

    if (enable) {
        sensor_ov5640_s_power(&ov5640->subdev, 1);
        u8 fmtreg = 0, fmtmuxreg = 0;
        int i;

        switch ((u32)ov5640->format.code) {
            case MEDIA_BUS_FMT_UYVY8_2X8:
                fmtreg = 0x32;
                fmtmuxreg = 0;
                break;
            case MEDIA_BUS_FMT_YUYV8_2X8:
                fmtreg = 0x30;
                fmtmuxreg = 0;
                break;
            default:
                /* This shouldn't happen */
                ret = -EINVAL;
                return ret;
        }

        ret = ov5640_reg_write(client, 0x4300, fmtreg);
        if (ret)
            return ret;

        ret = ov5640_reg_write(client, 0x501F, fmtmuxreg);
        if (ret)
            return ret;

        ret = ov5640_config_timing(sd);
        if (ret)
            return ret;
        // ov5640_reg_write(client, 0x4202, 0x00);


        i = ov5640_find_framesize(ov5640->format.width, ov5640->format.height);
        cam_info("%s: %dx%d (%d)\n", __func__,ov5640->format.width, ov5640->format.height,i);
        if ((i == OV5640_SIZE_QVGA) ||
            (i == OV5640_SIZE_VGA) ||
            (i == OV5640_SIZE_720P)) {
            //ret = ov5640_reg_write(client, 0x3108,
            //                       (i == OV5640_SIZE_720P) ? 0x1 : 0);
            //if (ret){
            //    return ret;
            //}
            //ret = ov5640_reg_set(client, 0x5001, 0x20);
        } else {
            //ret = ov5640_reg_clr(client, 0x5001, 0x20);
            //if (ret)
            //    return ret;
            //ret = ov5640_reg_write(client, 0x3108, 0x2);
        }

        ret = ov5640_reg_clr(client, 0x3008, 0x40);
        if (ret)
            goto out;

    } else {

        u8 tmpreg = 0;

        ret = ov5640_reg_read(client, 0x3008, &tmpreg);
        if (ret)
            goto out;

        ret = ov5640_reg_write(client, 0x3008, tmpreg | 0x40);
        if (ret)
            goto out;

        sensor_ov5640_s_power(&ov5640->subdev, 0);
    }

    out:
    return ret;
}


static int sensor_ov5640_s_format(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    ov5640->format.width = fmt->width;
    ov5640->format.height = fmt->height;

    //ov5640->format.code = fmt->code;

    return 0;
}

static int sensor_ov5640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
    struct ov5640_s *ov5640 = to_ov5640(sd);
    int err = 0;

    cam_info("%s\n", __func__);

    switch (ctrl->id) {
/*TODO
        case V4L2_CID_CAMERA_WHITE_BALANCE:
            break;
        case V4L2_CID_CAMERA_CONTRAST:
            break;
        case V4L2_CID_CAMERA_SATURATION:
            break;
        case V4L2_CID_CAMERA_SHARPNESS:
            break;
        case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
            break;
        case V4L2_CID_CAM_DATE_INFO_YEAR:
        case V4L2_CID_CAM_DATE_INFO_MONTH:
        case V4L2_CID_CAM_DATE_INFO_DATE:
        case V4L2_CID_CAM_SENSOR_VER:
        */
        default:
            break;
    }

    return err;
}


static int sensor_ov5640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{

    struct ov5640_s *ov5640  = to_ov5640(sd);
    int value = ctrl->value;

    cam_info("%s\n", __func__);

    switch (ctrl->id) {
        /*TODO
        case V4L2_CID_SCENEMODE:

            break;
        case V4L2_CID_CAM_BRIGHTNESS:

            break;
        case V4L2_CID_WHITE_BALANCE_PRESET:

            break;
        case V4L2_CID_CAMERA_EFFECT:

            break;
        case V4L2_CID_CAM_CONTRAST:

            break;
        case V4L2_CID_CAM_SATURATION:

            break;
        case V4L2_CID_CAM_SHARPNESS:

            break;
        case V4L2_CID_CAM_ISO:

            break;
        case V4L2_CID_CAPTURE:

            break;
        case V4L2_CID_CAM_OBJECT_POSITION_X:

            break;
        case V4L2_CID_CAM_OBJECT_POSITION_Y:

            break;
        case V4L2_CID_CAM_SINGLE_AUTO_FOCUS:
        case V4L2_CID_CAM_SET_AUTO_FOCUS:

            break;
        case V4L2_CID_FOCUS_MODE:
        case V4L2_CID_JPEG_QUALITY:
        case V4L2_CID_CAM_FLASH_MODE:
            break;
            */
        default:
        cam_err("[%s] Unidentified ID (%X)\n", __func__, ctrl->id);
            break;
    }

    return 0;
}



static int sensor_ov5640_link_setup(struct media_entity *entity,
                             const struct media_pad *local,
                             const struct media_pad *remote, u32 flags)
{
    cam_err("%s\n", __func__);

    return 0;
}


static int sensor_ov5640_subdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
    cam_info("%s\n", __func__);

    return 0;
}

static int sensor_ov5640_subdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
    cam_info("%s\n", __func__);

    return 0;
}

static int sensor_ov5640_subdev_registered(struct v4l2_subdev *sd)
{
    cam_info("%s\n", __func__);

    return 0;
}

static void sensor_ov5640_subdev_unregistered(struct v4l2_subdev *sd)
{
    cam_info("%s\n", __func__);
}



static int sensor_ov5640_s_fmt(struct v4l2_subdev *sd,
                            struct v4l2_subdev_pad_config *cfg,
                            struct v4l2_subdev_format *fmt)
{
    struct v4l2_mbus_framefmt *mf = &fmt->format;

    return sensor_ov5640_s_format(sd, mf);
}

static struct v4l2_subdev_pad_ops pad_ops = {
        .set_fmt		= sensor_ov5640_s_fmt,
};

static const struct v4l2_subdev_core_ops core_ops = {
        .init		= sensor_ov5640_init,
        .s_ctrl		= sensor_ov5640_s_ctrl,
        .g_ctrl		= sensor_ov5640_g_ctrl,
       /* TODO
        .get_fmt        = sensor_ov5640_get_fmt,
        .set_fmt        = sensor_ov5640_set_fmt,
        */
};

static const struct v4l2_subdev_video_ops video_ops = {
        .s_stream = sensor_ov5640_s_stream,
        .s_parm = sensor_ov5640_s_parm,
        .s_mbus_fmt = sensor_ov5640_s_format,

};

static const struct v4l2_subdev_ops subdev_ops = {
        .core = &core_ops,
        .video = &video_ops,
        .pad	= &pad_ops,
};

static const struct v4l2_subdev_internal_ops internal_ops = {
        .open = sensor_ov5640_subdev_open,
        .close = sensor_ov5640_subdev_close,
        .registered = sensor_ov5640_subdev_registered,
        .unregistered = sensor_ov5640_subdev_unregistered,
};

static const struct media_entity_operations media_ops = {
        .link_setup = sensor_ov5640_link_setup,
};


int sensor_ov5640_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
    int ret = 0;
    struct v4l2_subdev *subdev_module;
    struct ov5640_s *ov5640_state;
    cam_err("%s\n", __func__);
    ov5640_state = devm_kzalloc(&client->dev, sizeof(struct ov5640_s),
                          GFP_KERNEL);

    if (!ov5640_state) {
        ret = -ENOMEM;
        goto p_err;
    }

    subdev_module = &ov5640_state->subdev;
    snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE,
             "%s", SENSOR_NAME);

    v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);

    ov5640_state->pad.flags = MEDIA_PAD_FL_SOURCE;

    subdev_module->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    subdev_module->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
    subdev_module->internal_ops = &internal_ops;
    subdev_module->entity.ops = &media_ops;

    ret = media_entity_init(&subdev_module->entity, 1,
                            &ov5640_state->pad, 0);
    if (ret < 0) {
        cam_err("failed\n");
        return ret;
    }

    ov5640_state->xvclk= 24000000;
    ov5640_state->format.code = MEDIA_BUS_FMT_UYVY8_2X8;/*V4L2_MBUS_FMT_YUYV8_2X8;*/
    ov5640_state->format.width = ov5640_frmsizes[OV5640_SIZE_5MP].width;
    ov5640_state->format.height = ov5640_frmsizes[OV5640_SIZE_5MP].height;
    ov5640_state->format.field = V4L2_FIELD_NONE;
    ov5640_state->format.colorspace = V4L2_COLORSPACE_JPEG;

    ov5640_state->clk_cfg.sc_pll_prediv = 2;
    ov5640_state->clk_cfg.sc_pll_rdiv = 1;
    ov5640_state->clk_cfg.sc_pll_mult = 0x40 ;
    ov5640_state->clk_cfg.sysclk_div = 1;
    ov5640_state->clk_cfg.mipi_div = 1;

    //sd = &ov5640_state->subdev;
    ov5640_state->client = client;
    ov5640_state->client->flags |= I2C_CLIENT_SCCB;

p_err:
    cam_info("(%d)\n", ret);
    return ret;

}

static int sensor_ov5640_remove(struct i2c_client *client)
{
	int ret = 0;
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
    cam_err("%s\n", __func__);

	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	return ret;
}

static const struct i2c_device_id sensor_ov5640_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_ov5640_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= sensor_ov5640_probe,
	.remove	= sensor_ov5640_remove,
	.id_table = sensor_ov5640_idt
};

module_i2c_driver(sensor_ov5640_driver);

MODULE_AUTHOR("Danilo Sia <danilo.sia@kalpa.it>");
MODULE_DESCRIPTION("Sensor ov5640 driver");
MODULE_LICENSE("GPL v2");
