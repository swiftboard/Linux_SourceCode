/*
 * sound\soc\sun4i\hdmiaudio\sun4i-hdmiaudio.c
 * (C) Copyright 2007-2011
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * chenpailin <chenpailin@reuuimllatech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <mach/clock.h>
#include <mach/sys_config.h>

#include <mach/hardware.h>
#include <asm/dma.h>
#include <mach/dma.h>

#include "sun4i-hdmipcm.h"
#include "sun4i-hdmiaudio.h"


static struct sw_dma_client sun4i_dma_client_out = {
	.name = "HDMIAUDIO PCM Stereo out"
};

static struct sw_dma_client sun4i_dma_client_in = {
	.name = "HDMIAUDIO PCM Stereo in"
};

static struct sun4i_dma_params sun4i_hdmiaudio_pcm_stereo_out = {
	.client		=	&sun4i_dma_client_out,
	.channel	=	DMACH_HDMIAUDIO,
	.dma_addr 	=	0,
	.dma_size 	=   4,               /* dma transfer 32bits */
};

static struct sun4i_dma_params sun4i_hdmiaudio_pcm_stereo_in = {
	.client		=	&sun4i_dma_client_in,
	.channel	=	DMACH_HDMIAUDIO,
	.dma_addr 	=	SUN4I_HDMIAUDIOBASE + SUN4I_HDMIAUDIORXFIFO,
	.dma_size 	=   4,               /* dma transfer 32bits */
};

struct sun4i_hdmiaudio_info sun4i_hdmiaudio;

void sun4i_snd_txctrl_hdmiaudio(struct snd_pcm_substream *substream, int on)
{
	hdmi_para.channel_num = substream->runtime->channels;
	g_hdmi_func.hdmi_set_audio_para(&hdmi_para);


}

void sun4i_snd_rxctrl_hdmiaudio(struct snd_pcm_substream *substream, int on)
{
}

static int sun4i_hdmiaudio_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	return 0;
}

static int sun4i_hdmiaudio_hw_params(struct snd_pcm_substream *substream,
																struct snd_pcm_hw_params *params,
																struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sun4i_dma_params *dma_data;
	
	/* play or record */
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &sun4i_hdmiaudio_pcm_stereo_out;
	else
		dma_data = &sun4i_hdmiaudio_pcm_stereo_in;

	snd_soc_dai_set_dma_data(rtd->cpu_dai, substream, dma_data);
	
	return 0;
}

static int sun4i_hdmiaudio_trigger(struct snd_pcm_substream *substream,
                              int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sun4i_dma_params *dma_data = 
					snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				sun4i_snd_rxctrl_hdmiaudio(substream, 1);
			} else {
				sun4i_snd_txctrl_hdmiaudio(substream, 1);
			}
		sw_dma_ctrl(dma_data->channel, SW_DMAOP_STARTED);
		break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				sun4i_snd_rxctrl_hdmiaudio(substream, 0);
			} else {
			  sun4i_snd_txctrl_hdmiaudio(substream, 0);
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

//freq:   1: 22.5792MHz   0: 24.576MHz  
static int sun4i_hdmiaudio_set_sysclk(struct snd_soc_dai *cpu_dai, int clk_id, 
                                 unsigned int freq, int dir)
{
	return 0;
}

static int sun4i_hdmiaudio_set_clkdiv(struct snd_soc_dai *cpu_dai, int div_id, int div)
{
	return 0;
}

u32 sun4i_hdmiaudio_get_clockrate(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sun4i_hdmiaudio_get_clockrate);

static int sun4i_hdmiaudio_dai_probe(struct snd_soc_dai *dai)
{			
	return 0;
}
static int sun4i_hdmiaudio_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static int sun4i_hdmiaudio_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int sun4i_hdmiaudio_resume(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

#define SUN4I_I2S_RATES (SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT)
static struct snd_soc_dai_ops sun4i_hdmiaudio_dai_ops = {
	.trigger 		= sun4i_hdmiaudio_trigger,
	.hw_params 	= sun4i_hdmiaudio_hw_params,
	.set_fmt 		= sun4i_hdmiaudio_set_fmt,
	.set_clkdiv = sun4i_hdmiaudio_set_clkdiv,
	.set_sysclk = sun4i_hdmiaudio_set_sysclk, 
};
static struct snd_soc_dai_driver sun4i_hdmiaudio_dai = {
	.probe 		= sun4i_hdmiaudio_dai_probe,
	.suspend 	= sun4i_hdmiaudio_suspend,
	.resume 	= sun4i_hdmiaudio_resume,
	.remove 	= sun4i_hdmiaudio_dai_remove,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SUN4I_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SUN4I_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,},
	.symmetric_rates = 1,
	.ops = &sun4i_hdmiaudio_dai_ops,
};		

static int __devinit sun4i_hdmiaudio_dev_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = snd_soc_register_dai(&pdev->dev, &sun4i_hdmiaudio_dai);
	
	return 0;
}

static int __devexit sun4i_hdmiaudio_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_device sun4i_hdmiaudio_device = {
	.name = "sun4i-hdmiaudio",
};

static struct platform_driver sun4i_hdmiaudio_driver = {
	.probe = sun4i_hdmiaudio_dev_probe,
	.remove = __devexit_p(sun4i_hdmiaudio_dev_remove),
	.driver = {
		.name = "sun4i-hdmiaudio",
		.owner = THIS_MODULE,
	},	
};

static int __init sun4i_hdmiaudio_init(void)
{
	int err = 0;
		
	if((err = platform_device_register(&sun4i_hdmiaudio_device))<0)
		return err;

	if ((err = platform_driver_register(&sun4i_hdmiaudio_driver)) < 0)
		return err;
			
	return 0;
}
module_init(sun4i_hdmiaudio_init);

static void __exit sun4i_hdmiaudio_exit(void)
{	
	platform_driver_unregister(&sun4i_hdmiaudio_driver);
}
module_exit(sun4i_hdmiaudio_exit);


/* Module information */
MODULE_AUTHOR("REUUIMLLA");
MODULE_DESCRIPTION("sun4i hdmiaudio SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: sun4i-hdmiaudio");
