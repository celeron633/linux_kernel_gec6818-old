/*
 * alc5623.c  --  alc562[123] ALSA Soc Audio driver
 *
 * Copyright 2008 Realtek Microelectronics
 * Author: flove <flove@realtek.com> Ethan <eku@marvell.com>
 *
 * Copyright 2010 Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 *
 * Based on WM8753.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/alc5623.h>

#include "alc5623.h"

/*
#define pr_debug				printk
*/

static struct i2c_client *i2c;
static int caps_charge = 2000;
module_param(caps_charge, int, 0);
MODULE_PARM_DESC(caps_charge, "ALC5623 cap charge time (msecs)");
static int alc5623_set_bias_level(struct snd_soc_codec *codec,enum snd_soc_bias_level level);
/* codec private data */
struct alc5623_priv {
	enum snd_soc_control_type control_type;
	u8 id;
	unsigned int sysclk;
	u16 reg_cache[ALC5623_VENDOR_ID2+2];
	unsigned int add_ctrl;
	unsigned int jack_det_ctrl;
};

static void alc5623_fill_cache(struct snd_soc_codec *codec)
{
	int i, step = codec->driver->reg_cache_step;
	u16 *cache = codec->reg_cache;
    pr_debug("%s .....%d.......\n",__func__,__LINE__);
	/* not really efficient ... */
	codec->cache_bypass = 1;
	for (i = 0 ; i < codec->driver->reg_cache_size ; i += step)
		cache[i] = i2c_smbus_read_word_data(i2c, i);
	codec->cache_bypass = 0;
}

static inline int alc5623_reset(struct snd_soc_codec *codec)
{
    pr_debug("%s .....%d.......\n",__func__,__LINE__);
	return i2c_smbus_write_word_data(i2c, ALC5623_RESET, 0);
}

static int amp_mixer_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	/* to power-on/off class-d amp generators/speaker */
	/* need to write to 'index-46h' register :        */
	/* so write index num (here 0x46) to reg 0x6a     */
	/* and then 0xffff/0 to reg 0x6c                  */
  /*  printk("%s .....%d.......\n",__func__,__LINE__);
	i2c_smbus_write_word_data(i2c, ALC5623_HID_CTRL_INDEX, 0x46);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		i2c_smbus_write_word_data(i2c, ALC5623_HID_CTRL_DATA, 0xFFFF);
		break;
	case SND_SOC_DAPM_POST_PMD:
		i2c_smbus_write_word_data(i2c, ALC5623_HID_CTRL_DATA, 0);
		break;
	}*/

	return 0;
}

/*
 * ALC5623 Controls
 */

static const DECLARE_TLV_DB_SCALE(vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_rec_tlv, -1650, 150, 0);
static const unsigned int boost_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};
static const DECLARE_TLV_DB_SCALE(dig_tlv, 0, 600, 0);

static const struct snd_kcontrol_new alc5621_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Speaker Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Speaker Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Headphone Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Headphone Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new alc5622_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Speaker Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Speaker Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Line Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Line Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new alc5623_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Line Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Line Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Headphone Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Headphone Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new alc5623_snd_controls[] = {
	SOC_DOUBLE_TLV("Auxout Playback Volume",
			ALC5623_MONO_AUX_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Auxout Playback Switch",
			ALC5623_MONO_AUX_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("PCM Playback Volume",
			ALC5623_STEREO_DAC_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("AuxI Capture Volume",
			ALC5623_AUXIN_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("LineIn Capture Volume",
			ALC5623_LINE_IN_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_SINGLE_TLV("Mic1 Capture Volume",
			ALC5623_MIC_VOL, 8, 31, 1, vol_tlv),
	SOC_SINGLE_TLV("Mic2 Capture Volume",
			ALC5623_MIC_VOL, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("Rec Capture Volume",
			ALC5623_ADC_REC_GAIN, 7, 0, 31, 0, adc_rec_tlv),
	SOC_SINGLE_TLV("Mic 1 Boost Volume",
			ALC5623_MIC_CTRL, 10, 2, 0, boost_tlv),
	SOC_SINGLE_TLV("Mic 2 Boost Volume",
			ALC5623_MIC_CTRL, 8, 2, 0, boost_tlv),
	SOC_SINGLE_TLV("Digital Boost Volume",
			ALC5623_ADD_CTRL_REG, 4, 3, 0, dig_tlv),
};

/*
 * DAPM Controls
 */
static const struct snd_kcontrol_new alc5623_hp_mixer_controls[] = {
SOC_DAPM_SINGLE("LI2HP Playback Switch", ALC5623_LINE_IN_VOL, 15, 1, 1),
SOC_DAPM_SINGLE("AUXI2HP Playback Switch", ALC5623_AUXIN_VOL, 15, 1, 1),
SOC_DAPM_SINGLE("MIC12HP Playback Switch", ALC5623_MIC_ROUTING_CTRL, 15, 1, 1),
SOC_DAPM_SINGLE("MIC22HP Playback Switch", ALC5623_MIC_ROUTING_CTRL, 7, 1, 1),
SOC_DAPM_SINGLE("DAC2HP Playback Switch", ALC5623_STEREO_DAC_VOL, 15, 1, 1),
};

static const struct snd_kcontrol_new alc5623_hpl_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2HP_L Playback Switch", ALC5623_ADC_REC_GAIN, 15, 1, 1),
};

static const struct snd_kcontrol_new alc5623_hpr_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2HP_R Playback Switch", ALC5623_ADC_REC_GAIN, 14, 1, 1),
};

static const struct snd_kcontrol_new alc5623_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2MONO_L Playback Switch", ALC5623_ADC_REC_GAIN, 13, 1, 1),
SOC_DAPM_SINGLE("ADC2MONO_R Playback Switch", ALC5623_ADC_REC_GAIN, 12, 1, 1),
SOC_DAPM_SINGLE("LI2MONO Playback Switch", ALC5623_LINE_IN_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("AUXI2MONO Playback Switch", ALC5623_AUXIN_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC12MONO Playback Switch", ALC5623_MIC_ROUTING_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC22MONO Playback Switch", ALC5623_MIC_ROUTING_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("DAC2MONO Playback Switch", ALC5623_STEREO_DAC_VOL, 13, 1, 1),
};

static const struct snd_kcontrol_new alc5623_speaker_mixer_controls[] = {
SOC_DAPM_SINGLE("LI2SPK Playback Switch", ALC5623_LINE_IN_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("AUXI2SPK Playback Switch", ALC5623_AUXIN_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("MIC12SPK Playback Switch", ALC5623_MIC_ROUTING_CTRL, 14, 1, 1),
SOC_DAPM_SINGLE("MIC22SPK Playback Switch", ALC5623_MIC_ROUTING_CTRL, 6, 1, 1),
SOC_DAPM_SINGLE("DAC2SPK Playback Switch", ALC5623_STEREO_DAC_VOL, 14, 1, 1),
};

/* Left Record Mixer */
static const struct snd_kcontrol_new alc5623_captureL_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5623_ADC_REC_MIXER, 14, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5623_ADC_REC_MIXER, 13, 1, 1),
SOC_DAPM_SINGLE("LineInL Capture Switch", ALC5623_ADC_REC_MIXER, 12, 1, 1),
SOC_DAPM_SINGLE("Left AuxI Capture Switch", ALC5623_ADC_REC_MIXER, 11, 1, 1),
SOC_DAPM_SINGLE("HPMixerL Capture Switch", ALC5623_ADC_REC_MIXER, 10, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch", ALC5623_ADC_REC_MIXER, 9, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch", ALC5623_ADC_REC_MIXER, 8, 1, 1),
};

/* Right Record Mixer */
static const struct snd_kcontrol_new alc5623_captureR_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5623_ADC_REC_MIXER, 6, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5623_ADC_REC_MIXER, 5, 1, 1),
SOC_DAPM_SINGLE("LineInR Capture Switch", ALC5623_ADC_REC_MIXER, 4, 1, 1),
SOC_DAPM_SINGLE("Right AuxI Capture Switch", ALC5623_ADC_REC_MIXER, 3, 1, 1),
SOC_DAPM_SINGLE("HPMixerR Capture Switch", ALC5623_ADC_REC_MIXER, 2, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch", ALC5623_ADC_REC_MIXER, 1, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch", ALC5623_ADC_REC_MIXER, 0, 1, 1),
};

static const char *alc5623_spk_n_sour_sel[] = {
		"RN/-R", "RP/+R", "LN/-R", "Vmid" };
static const char *alc5623_hpl_out_input_sel[] = {
		"Vmid", "HP Left Mix"};
static const char *alc5623_hpr_out_input_sel[] = {
		"Vmid", "HP Right Mix"};
static const char *alc5623_spkout_input_sel[] = {
		"Vmid", "HPOut Mix", "Speaker Mix", "Mono Mix"};
static const char *alc5623_aux_out_input_sel[] = {
		"Vmid", "HPOut Mix", "Speaker Mix", "Mono Mix"};

/* auxout output mux */
static const struct soc_enum alc5623_aux_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 6, 4, alc5623_aux_out_input_sel);
static const struct snd_kcontrol_new alc5623_auxout_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_aux_out_input_enum);

/* speaker output mux */
static const struct soc_enum alc5623_spkout_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 10, 4, alc5623_spkout_input_sel);
static const struct snd_kcontrol_new alc5623_spkout_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_spkout_input_enum);

/* headphone left output mux */
static const struct soc_enum alc5623_hpl_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 9, 2, alc5623_hpl_out_input_sel);
static const struct snd_kcontrol_new alc5623_hpl_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_hpl_out_input_enum);

/* headphone right output mux */
static const struct soc_enum alc5623_hpr_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 8, 2, alc5623_hpr_out_input_sel);
static const struct snd_kcontrol_new alc5623_hpr_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_hpr_out_input_enum);

/* speaker output N select */
static const struct soc_enum alc5623_spk_n_sour_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 14, 4, alc5623_spk_n_sour_sel);
static const struct snd_kcontrol_new alc5623_spkoutn_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_spk_n_sour_enum);

static const struct snd_soc_dapm_widget alc5623_dapm_widgets[] = {
/* Muxes */
SND_SOC_DAPM_MUX("AuxOut Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_auxout_mux_controls),
SND_SOC_DAPM_MUX("SpeakerOut Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_spkout_mux_controls),
SND_SOC_DAPM_MUX("Left Headphone Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_hpl_out_mux_controls),
SND_SOC_DAPM_MUX("Right Headphone Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_hpr_out_mux_controls),
SND_SOC_DAPM_MUX("SpeakerOut N Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_spkoutn_mux_controls),

/* output mixers */
SND_SOC_DAPM_MIXER("HP Mix", SND_SOC_NOPM, 0, 0,
	&alc5623_hp_mixer_controls[0],
	ARRAY_SIZE(alc5623_hp_mixer_controls)),
SND_SOC_DAPM_MIXER("HPR Mix", ALC5623_PWR_MANAG_ADD2, 4, 0,
	&alc5623_hpr_mixer_controls[0],
	ARRAY_SIZE(alc5623_hpr_mixer_controls)),
SND_SOC_DAPM_MIXER("HPL Mix", ALC5623_PWR_MANAG_ADD2, 5, 0,
	&alc5623_hpl_mixer_controls[0],
	ARRAY_SIZE(alc5623_hpl_mixer_controls)),
SND_SOC_DAPM_MIXER("HPOut Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Mono Mix", ALC5623_PWR_MANAG_ADD2, 2, 0,
	&alc5623_mono_mixer_controls[0],
	ARRAY_SIZE(alc5623_mono_mixer_controls)),
SND_SOC_DAPM_MIXER("Speaker Mix", ALC5623_PWR_MANAG_ADD2, 3, 0,
	&alc5623_speaker_mixer_controls[0],
	ARRAY_SIZE(alc5623_speaker_mixer_controls)),

/* input mixers */
SND_SOC_DAPM_MIXER("Left Capture Mix", ALC5623_PWR_MANAG_ADD2, 1, 0,
	&alc5623_captureL_mixer_controls[0],
	ARRAY_SIZE(alc5623_captureL_mixer_controls)),
SND_SOC_DAPM_MIXER("Right Capture Mix", ALC5623_PWR_MANAG_ADD2, 0, 0,
	&alc5623_captureR_mixer_controls[0],
	ARRAY_SIZE(alc5623_captureR_mixer_controls)),

SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback",
	ALC5623_PWR_MANAG_ADD2, 9, 0),
SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback",
	ALC5623_PWR_MANAG_ADD2, 8, 0),
SND_SOC_DAPM_MIXER("I2S Mix", ALC5623_PWR_MANAG_ADD1, 15, 0, NULL, 0),
SND_SOC_DAPM_MIXER("AuxI Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Line Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_ADC("Left ADC", "Left HiFi Capture",
	ALC5623_PWR_MANAG_ADD2, 7, 0),
SND_SOC_DAPM_ADC("Right ADC", "Right HiFi Capture",
	ALC5623_PWR_MANAG_ADD2, 6, 0),
SND_SOC_DAPM_PGA("Left Headphone", ALC5623_PWR_MANAG_ADD3, 10, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Headphone", ALC5623_PWR_MANAG_ADD3, 9, 0, NULL, 0),
SND_SOC_DAPM_PGA("SpeakerOut", ALC5623_PWR_MANAG_ADD3, 12, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left AuxOut", ALC5623_PWR_MANAG_ADD3, 14, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right AuxOut", ALC5623_PWR_MANAG_ADD3, 13, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left LineIn", ALC5623_PWR_MANAG_ADD3, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right LineIn", ALC5623_PWR_MANAG_ADD3, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left AuxI", ALC5623_PWR_MANAG_ADD3, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right AuxI", ALC5623_PWR_MANAG_ADD3, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC1 PGA", ALC5623_PWR_MANAG_ADD3, 3, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC2 PGA", ALC5623_PWR_MANAG_ADD3, 2, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC1 Pre Amp", ALC5623_PWR_MANAG_ADD3, 1, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC2 Pre Amp", ALC5623_PWR_MANAG_ADD3, 0, 0, NULL, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias1", ALC5623_PWR_MANAG_ADD1, 11, 0),

SND_SOC_DAPM_OUTPUT("AUXOUTL"),
SND_SOC_DAPM_OUTPUT("AUXOUTR"),
SND_SOC_DAPM_OUTPUT("HPL"),
SND_SOC_DAPM_OUTPUT("HPR"),
SND_SOC_DAPM_OUTPUT("SPKOUT"),
SND_SOC_DAPM_OUTPUT("SPKOUTN"),
SND_SOC_DAPM_INPUT("LINEINL"),
SND_SOC_DAPM_INPUT("LINEINR"),
SND_SOC_DAPM_INPUT("AUXINL"),
SND_SOC_DAPM_INPUT("AUXINR"),
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2"),
SND_SOC_DAPM_VMID("Vmid"),
};

static const char *alc5623_amp_names[] = {"AB Amp", "D Amp"};
static const struct soc_enum alc5623_amp_enum =
	SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 13, 2, alc5623_amp_names);
static const struct snd_kcontrol_new alc5623_amp_mux_controls =
	SOC_DAPM_ENUM("Route", alc5623_amp_enum);

static const struct snd_soc_dapm_widget alc5623_dapm_amp_widgets[] = {
SND_SOC_DAPM_PGA_E("D Amp", ALC5623_PWR_MANAG_ADD2, 14, 0, NULL, 0,
	amp_mixer_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_PGA("AB Amp", ALC5623_PWR_MANAG_ADD2, 15, 0, NULL, 0),
SND_SOC_DAPM_MUX("AB-D Amp Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_amp_mux_controls),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* virtual mixer - mixes left & right channels */
	{"I2S Mix", NULL,				"Left DAC"},
	{"I2S Mix", NULL,				"Right DAC"},
	{"Line Mix", NULL,				"Right LineIn"},
	{"Line Mix", NULL,				"Left LineIn"},
	{"AuxI Mix", NULL,				"Left AuxI"},
	{"AuxI Mix", NULL,				"Right AuxI"},
	{"AUXOUTL", NULL,				"Left AuxOut"},
	{"AUXOUTR", NULL,				"Right AuxOut"},

	/* HP mixer */
	{"HPL Mix", "ADC2HP_L Playback Switch",		"Left Capture Mix"},
	{"HPL Mix", NULL,				"HP Mix"},
	{"HPR Mix", "ADC2HP_R Playback Switch",		"Right Capture Mix"},
	{"HPR Mix", NULL,				"HP Mix"},
	{"HP Mix", "LI2HP Playback Switch",		"Line Mix"},
	{"HP Mix", "AUXI2HP Playback Switch",		"AuxI Mix"},
	{"HP Mix", "MIC12HP Playback Switch",		"MIC1 PGA"},
	{"HP Mix", "MIC22HP Playback Switch",		"MIC2 PGA"},
	{"HP Mix", "DAC2HP Playback Switch",		"I2S Mix"},

	/* speaker mixer */
	{"Speaker Mix", "LI2SPK Playback Switch",	"Line Mix"},
	{"Speaker Mix", "AUXI2SPK Playback Switch",	"AuxI Mix"},
	{"Speaker Mix", "MIC12SPK Playback Switch",	"MIC1 PGA"},
	{"Speaker Mix", "MIC22SPK Playback Switch",	"MIC2 PGA"},
	{"Speaker Mix", "DAC2SPK Playback Switch",	"I2S Mix"},

	/* mono mixer */
	{"Mono Mix", "ADC2MONO_L Playback Switch",	"Left Capture Mix"},
	{"Mono Mix", "ADC2MONO_R Playback Switch",	"Right Capture Mix"},
	{"Mono Mix", "LI2MONO Playback Switch",		"Line Mix"},
	{"Mono Mix", "AUXI2MONO Playback Switch",	"AuxI Mix"},
	{"Mono Mix", "MIC12MONO Playback Switch",	"MIC1 PGA"},
	{"Mono Mix", "MIC22MONO Playback Switch",	"MIC2 PGA"},
	{"Mono Mix", "DAC2MONO Playback Switch",	"I2S Mix"},

	/* Left record mixer */
	{"Left Capture Mix", "LineInL Capture Switch",	"LINEINL"},
	{"Left Capture Mix", "Left AuxI Capture Switch", "AUXINL"},
	{"Left Capture Mix", "Mic1 Capture Switch",	"MIC1 Pre Amp"},
	{"Left Capture Mix", "Mic2 Capture Switch",	"MIC2 Pre Amp"},
	{"Left Capture Mix", "HPMixerL Capture Switch", "HPL Mix"},
	{"Left Capture Mix", "SPKMixer Capture Switch", "Speaker Mix"},
	{"Left Capture Mix", "MonoMixer Capture Switch", "Mono Mix"},

	/*Right record mixer */
	{"Right Capture Mix", "LineInR Capture Switch",	"LINEINR"},
	{"Right Capture Mix", "Right AuxI Capture Switch",	"AUXINR"},
	{"Right Capture Mix", "Mic1 Capture Switch",	"MIC1 Pre Amp"},
	{"Right Capture Mix", "Mic2 Capture Switch",	"MIC2 Pre Amp"},
	{"Right Capture Mix", "HPMixerR Capture Switch", "HPR Mix"},
	{"Right Capture Mix", "SPKMixer Capture Switch", "Speaker Mix"},
	{"Right Capture Mix", "MonoMixer Capture Switch", "Mono Mix"},

	/* headphone left mux */
	{"Left Headphone Mux", "HP Left Mix",		"HPL Mix"},
	{"Left Headphone Mux", "Vmid",			"Vmid"},

	/* headphone right mux */
	{"Right Headphone Mux", "HP Right Mix",		"HPR Mix"},
	{"Right Headphone Mux", "Vmid",			"Vmid"},

	/* speaker out mux */
	{"SpeakerOut Mux", "Vmid",			"Vmid"},
	{"SpeakerOut Mux", "HPOut Mix",			"HPOut Mix"},
	{"SpeakerOut Mux", "Speaker Mix",		"Speaker Mix"},
	{"SpeakerOut Mux", "Mono Mix",			"Mono Mix"},

	/* Mono/Aux Out mux */
	{"AuxOut Mux", "Vmid",				"Vmid"},
	{"AuxOut Mux", "HPOut Mix",			"HPOut Mix"},
	{"AuxOut Mux", "Speaker Mix",			"Speaker Mix"},
	{"AuxOut Mux", "Mono Mix",			"Mono Mix"},

	/* output pga */
	{"HPL", NULL,					"Left Headphone"},
	{"Left Headphone", NULL,			"Left Headphone Mux"},
	{"HPR", NULL,					"Right Headphone"},
	{"Right Headphone", NULL,			"Right Headphone Mux"},
	{"Left AuxOut", NULL,				"AuxOut Mux"},
	{"Right AuxOut", NULL,				"AuxOut Mux"},

	/* input pga */
	{"Left LineIn", NULL,				"LINEINL"},
	{"Right LineIn", NULL,				"LINEINR"},
	{"Left AuxI", NULL,				"AUXINL"},
	{"Right AuxI", NULL,				"AUXINR"},
	{"MIC1 Pre Amp", NULL,				"MIC1"},
	{"MIC2 Pre Amp", NULL,				"MIC2"},
	{"MIC1 PGA", NULL,				"MIC1 Pre Amp"},
	{"MIC2 PGA", NULL,				"MIC2 Pre Amp"},

	/* left ADC */
	{"Left ADC", NULL,				"Left Capture Mix"},

	/* right ADC */
	{"Right ADC", NULL,				"Right Capture Mix"},

	{"SpeakerOut N Mux", "RN/-R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "RP/+R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "LN/-R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "Vmid",			"Vmid"},

	{"SPKOUT", NULL,				"SpeakerOut"},
	{"SPKOUTN", NULL,				"SpeakerOut N Mux"},
};

static const struct snd_soc_dapm_route intercon_spk[] = {
	{"SpeakerOut", NULL,				"SpeakerOut Mux"},
};

static const struct snd_soc_dapm_route intercon_amp_spk[] = {
	{"AB Amp", NULL,				"SpeakerOut Mux"},
	{"D Amp", NULL,					"SpeakerOut Mux"},
	{"AB-D Amp Mux", "AB Amp",			"AB Amp"},
	{"AB-D Amp Mux", "D Amp",			"D Amp"},
	{"SpeakerOut", NULL,				"AB-D Amp Mux"},
};

/* PLL divisors */
struct _pll_div {
	u32 pll_in;
	u32 pll_out;
	u16 regvalue;
};

/* Note : pll code from original alc5623 driver. Not sure of how good it is */
/* useful only for master mode */
static const struct _pll_div codec_master_pll_div[] = {

	{  2048000,  8192000,	0x0ea0},
	{  3686400,  8192000,	0x4e27},
	{ 12000000,  8192000,	0x456b},
	{ 13000000,  8192000,	0x495f},
	{ 13100000,  8192000,	0x0320},
	{  2048000,  11289600,	0xf637},
	{  3686400,  11289600,	0x2f22},
	{ 12000000,  11289600,	0x3e2f},
	{ 13000000,  11289600,	0x4d5b},
	{ 13100000,  11289600,	0x363b},
	{  2048000,  16384000,	0x1ea0},
	{  3686400,  16384000,	0x9e27},
	{ 12000000,  16384000,	0x452b},
	{ 13000000,  16384000,	0x542f},
	{ 13100000,  16384000,	0x03a0},
	{  2048000,  16934400,	0xe625},
	{  3686400,  16934400,	0x9126},
	{ 12000000,  16934400,	0x4d2c},
	{ 13000000,  16934400,	0x742f},
	{ 13100000,  16934400,	0x3c27},
	{  2048000,  22579200,	0x2aa0},
	{  3686400,  22579200,	0x2f20},
	{ 12000000,  22579200,	0x7e2f},
	{ 13000000,  22579200,	0x742f},
	{ 13100000,  22579200,	0x3c27},
	{  2048000,  24576000,	0x2ea0},
	{  3686400,  24576000,	0xee27},
	{ 12000000,  24576000,	0x2915},
	{ 13000000,  24576000,	0x772e},
	{ 13100000,  24576000,	0x0d20},
};

static const struct _pll_div codec_slave_pll_div[] = {

	{  1024000,  16384000,  0x3ea0},
	{  1411200,  22579200,	0x3ea0},
	{  1536000,  24576000,	0x3ea0},
	{  2048000,  16384000,  0x1ea0},
	{  2822400,  22579200,	0x1ea0},
	{  3072000,  24576000,	0x1ea0},

};

static int alc5623_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
/*	int i;
	struct snd_soc_codec *codec = codec_dai->codec;
	int gbl_clk = 0, pll_div = 0;
	u16 reg;
    printk("%s .....%d.......\n",__func__,__LINE__);
	if (pll_id < ALC5623_PLL_FR_MCLK || pll_id > ALC5623_PLL_FR_BCK)
		return -ENODEV;


	//snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD2,
	//			ALC5623_PWR_ADD2_PLL,
	//			0);
    i2c_smbus_write_word_data(i2c,ALC5623_PWR_MANAG_ADD2,i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD2)&~ALC5623_PWR_ADD2_PLL);


	reg = i2c_smbus_read_word_data(codec, ALC5623_DAI_CONTROL);
	if (reg & ALC5623_DAI_SDP_SLAVE_MODE)
		return 0;

	if (!freq_in || !freq_out)
		return 0;

	switch (pll_id) {
	case ALC5623_PLL_FR_MCLK:
		for (i = 0; i < ARRAY_SIZE(codec_master_pll_div); i++) {
			if (codec_master_pll_div[i].pll_in == freq_in
			   && codec_master_pll_div[i].pll_out == freq_out) {

				pll_div  = codec_master_pll_div[i].regvalue;
				break;
			}
		}
		break;
	case ALC5623_PLL_FR_BCK:
		for (i = 0; i < ARRAY_SIZE(codec_slave_pll_div); i++) {
			if (codec_slave_pll_div[i].pll_in == freq_in
			   && codec_slave_pll_div[i].pll_out == freq_out) {

				gbl_clk = ALC5623_GBL_CLK_PLL_SOUR_SEL_BITCLK;
				pll_div = codec_slave_pll_div[i].regvalue;
				break;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	if (!pll_div)
		return -EINVAL;

	i2c_smbus_write_word_data(i2c, ALC5623_GLOBAL_CLK_CTRL_REG, gbl_clk);
	i2c_smbus_write_word_data(i2c, ALC5623_PLL_CTRL, pll_div);
	//snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD2,
	//			ALC5623_PWR_ADD2_PLL,
	//			ALC5623_PWR_ADD2_PLL);

    i2c_smbus_write_word_data(i2c,ALC5623_PWR_MANAG_ADD2,i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD2)|ALC5623_PWR_ADD2_PLL);
	gbl_clk |= ALC5623_GBL_CLK_SYS_SOUR_SEL_PLL;
	i2c_smbus_write_word_data(i2c, ALC5623_GLOBAL_CLK_CTRL_REG, gbl_clk);
*/
	return 0;
}

struct _coeff_div {
	u16 fs;
	u16 regvalue;
};

/* codec hifi mclk (after PLL) clock divider coefficients */
/* values inspired from column BCLK=32Fs of Appendix A table */
static const struct _coeff_div coeff_div[] = {
	{256*8, 0x3a69},
	{384*8, 0x3c6b},
	{256*4, 0x2a69},
	{384*4, 0x2c6b},
	{256*2, 0x1a69},
	{384*2, 0x1c6b},
	{256*1, 0x0a69},
	{384*1, 0x0c6b},
};


static int init_regs[][2]=
{
    {0x02,0x0000},
    {0x04,0x0808},
    {0x14,0x3f3f},
    {0x0c,0x0808},
    {0x22,0x0101},
    {0x0a,0x0202},
    {0x62,0x880e},
    {0x12,0xff9f},
    {0x34,0x8000},
    {0x36,0x066f},
    {0x40,0x3410},
    #if 1
    {0x1c,0x8740},
    #else
    {0x1c,0xA740},
    {0x6a,0x0046},
    {0x6c,0xFFFF},
    #endif
    {0x5a,0x0000},
    //{0x3A,0x1d60},
    //{0x3C,0xf7ff},
    //{0x3E,0xf6ff},
    {0x02,0x0000},
    {0x04,0x0000},
    {0x06,0x0000},
    {0x0c,0x0000},
};

#define REG_INIT_NUM (sizeof(init_regs)/sizeof(init_regs[0]))

static int set_init_regs(struct snd_soc_codec *codec)
{
    int i=0,reg,ret;
    for (i=0;i<REG_INIT_NUM;i++) {
        //snd_soc_write(codec, init_regs[i][0], init_regs[i][1]);
        ret=i2c_smbus_write_word_data(i2c,(u8)init_regs[i][0], (u16)init_regs[i][1]);
        if (ret<0) {
            printk("i2c_smbus_write_word_data error %d\n",ret);
        }
        msleep(10);
        reg= i2c_smbus_read_word_data(i2c,(u8)init_regs[i][0]);
        printk("Read reg[0x%02x] = 0x%04x \n",init_regs[i][0],reg);


    }
    printk("%s over %d regs \n",__func__,REG_INIT_NUM);
    return 0;
}

static unsigned int ALC5625_read_IIC(struct i2c_client *client, unsigned char addr)
{
	u8 data[2] = {0};
	unsigned int value = 0;
	data[0] = addr;
	if(i2c_master_send(client, data, 1) == 1)
	{
		i2c_master_recv(client, data, 2);
		value = (data[0] << 8) | data[1];
		return value;
	}
	else
		printk("i2c read error\n");
	return 0;
}


static void ALC5625_write_IIC(struct i2c_client *client, unsigned char addr, unsigned int value)
{
	u8 data[3];
	data[0] = addr;
	data[1] = (value & 0xff00) >> 8;
	data[2] = value & 0x00ff;
	if(i2c_master_send(client, data, 3) != 3)
		printk("i2c write error\n");
}

static void ALC5625_write_IICMask(struct i2c_client *client, u8 Offset, u16 Data,u16 Mask)
{
	u16 CodecData;
    if(Mask!=0xffff) {
        CodecData = ALC5625_read_IIC(client,Offset);
        CodecData&=~Mask;
        CodecData|=Data&Mask;
        ALC5625_write_IIC(client,Offset,CodecData);
    }
    else{
        ALC5625_write_IIC(client,Offset,Data);
    }
}


typedef struct
{
    u8 CodecIndex;
    u16 wCodecValue;
}CodecRegister;

static CodecRegister Set_Codec_Reg_Init1[]=
{
	{RT5621_SPK_OUT_VOL			,0x0000},//default speaker to 0DB
	{RT5621_HP_OUT_VOL			,0x0808},//default HP to -12DB
	{RT5621_ADC_REC_MIXER		,0x3F3F},//default Record is MicIn
	{RT5621_STEREO_DAC_VOL		,0x0000},//default stereo DAC volume
	{RT5621_MIC_CTRL			,0x0101},//set boost to +20DB for Mic10 x0400


	{RT5621_LINE_IN_VOL			,0x0202},//set LineIN to HP(b15),SP(b14),MONO(b13) MIX

	{RT5621_EQ_CTRL			,0x880E},//EQ
	{RT5621_ADC_REC_GAIN			,0xFF9F},//ADC Gain

	{RT5621_AUDIO_INTERFACE		,0x8000},//set I2S codec to slave mode
	{RT5621_STEREO_AD_DA_CLK_CTRL,0x066d},//AD-DA Filter clock is 256 fs
	//{RT5621_STEREO_AD_DA_CLK_CTRL,0x0B50},//AD-DA Filter clock is 256 fs
	{RT5621_ADD_CTRL_REG		,0x3410},//set Class AB & D ratio to 1 AVdd  0x5f00
#if	1

	{RT5621_OUTPUT_MIXER_CTRL	,0x8740},//default output mixer control,CLASS AB

#else

	{RT5621_OUTPUT_MIXER_CTRL	,0xA740},//default output mixer control,CLASS D
	{RT5621_HID_CTRL_INDEX		,0x46},
	{RT5621_HID_CTRL_DATA		,0xFFFF},//Power on all bit of  Class D

#endif

	{RT5621_JACK_DET_CTRL   ,  0x0180}, //JACK Detect setting

};

#define SET_CODEC_REG_INIT_NUM1 (sizeof(Set_Codec_Reg_Init1)/sizeof(CodecRegister))

static void init_codec(struct i2c_client *client)
{
   /* int i;
    u16 PowerDownState=0;
    u32 Vender1,Vender2;
    ALC5625_write_IIC(client,0,0xffff);

	for(i=0;i<=10000;i++);

	Vender1 = ALC5625_read_IIC(client,0x7C);
	Vender2 = ALC5625_read_IIC(client,0x7E);

	printk("ALC5621 INIT:vender id1 0x%x, id2 0x%x\r\n", Vender1, Vender2);

	//ALC5625_write_IICMask(RT5621_PWR_MANAG_ADD3,PWR_MAIN_BIAS,PWR_MAIN_BIAS);		//power on main bias
	ALC5625_write_IICMask(client,RT5621_PWR_MANAG_ADD3,PWR_MAIN_BIAS|PWR_LINEIN_L_VOL|PWR_LINEIN_R_VOL,PWR_MAIN_BIAS|PWR_LINEIN_L_VOL|PWR_LINEIN_R_VOL);		//power on main bias and Line in
	ALC5625_write_IICMask(client,RT5621_PWR_MANAG_ADD2,PWR_VREF,PWR_VREF);				//power on Vref for All analog circuit

	//initize customize setting
	for(i=0;i<SET_CODEC_REG_INIT_NUM1;i++)
	{
		ALC5625_write_IIC(client, Set_Codec_Reg_Init1[i].CodecIndex,Set_Codec_Reg_Init1[i].wCodecValue);
	}

	ALC5625_write_IIC(client,RT5621_PWR_MANAG_ADD1,~PowerDownState);
	ALC5625_write_IIC(client,RT5621_PWR_MANAG_ADD2,~PowerDownState);
	ALC5625_write_IIC(client,RT5621_PWR_MANAG_ADD3,~PowerDownState);

	//ALC5625_write_IICMask(client,RT5621_HP_OUT_VOL, RT_L_MUTE|RT_R_MUTE,RT_L_MUTE|RT_R_MUTE);	//Mute headphone right/left channel
	//ALC5625_write_IICMask(client,RT5621_SPK_OUT_VOL,RT_L_MUTE|RT_R_MUTE,RT_L_MUTE|RT_R_MUTE);	//Mute Speaker right/left channel

    printk("%s .....%d.......\n",__func__,__LINE__);*/


    ALC5625_write_IIC(client,0x3C,0x2000);
    ALC5625_write_IIC(client,0x3E,0x8000);
    //ALC5625_write_IIC(client,0x1C,0x0740);//class a/b 0000 0111 0100 0000; 0010 
    ALC5625_write_IIC(client,0x1C,0xA740);//class d
    ALC5625_write_IIC(client,0x6A,0x46);
    ALC5625_write_IIC(client,0x6C,0xFFFF);	
    ALC5625_write_IIC(client,0x14,0x3F3F);
    ALC5625_write_IIC(client,0x0C,0x0808);
    ALC5625_write_IIC(client,0x04,0x8888);
    ALC5625_write_IIC(client,0x02,0x8080);
    ALC5625_write_IIC(client,0x34,0x8000);
    ALC5625_write_IIC(client,0x36,0x066D);
    ALC5625_write_IIC(client,0x40,0x5F00);

    ALC5625_write_IIC(client,0x3A,0x8830);
    //ALC5625_write_IIC(client,0x3C,0xA7F7); //6734 class d 1010 0111 1111 0111
    //ALC5625_write_IIC(client,0x3C,0xF7FF);		//0110 0111 1111 0111 
    ALC5625_write_IIC(client,0x3C,0x67F7);
    ALC5625_write_IIC(client,0x3E,0x960A);
    ALC5625_write_IIC(client,0x34,0x8000);
    ALC5625_write_IIC(client,0x0C,0x4808);
    //ALC5625_write_IIC(client,0x1C,0x9F00); //AF00 class d
    //ALC5625_write_IIC(client,0x1C,0xA740);//class d
    ALC5625_write_IIC(client,0x1C,0xBF00);//class d
    ALC5625_write_IIC(client,0x02,0x0000);
    ALC5625_write_IIC(client,0x04,0x0000);
    ALC5625_write_IIC(client,0x10,0xF0E0);
    ALC5625_write_IIC(client,0x0E,0xFFFF); //hdc added 20150209;
    ALC5625_write_IIC(client,0x22,0x0800); // mic1 boost +30db
    ALC5625_write_IIC(client,0x14,0x3F3F);
    ALC5625_write_IIC(client,0x12,0xF58B); //FF9F FB16 F58B

    //ALC5625_write_IIC(client,0x5A,0x8880);

}

/*
 * show all codec regs' value
 * hdc 20150209
 */
static void show_codec_regs(struct i2c_client *client)
{
	int regs_value[140]={0},i=0,j=0;

	for(j=0;j<200;j++);
	for(i=0;i<=0x7E;i=i+2)
	{
		regs_value[i] = ALC5625_read_IIC(client,i);
		for(j=0;j<100;j++);
	}
	for(i=0;i<=0x7E;i=i+2)
	{
		printk("regs 0x%02x : %04x\n",i,regs_value[i]);
	}
}

/*
 * control codec's speaker or headphone's vol;
 * with these to complete the speaker and headphone selection function;
 * hdc 20150209
 */
void alc5623_headset(int on)
{
	int j=0;
	if(on)
	{
		ALC5625_write_IIC(i2c,0x02,0x0000); //speaker on
		for(j=0;j<20;j++);
    		ALC5625_write_IIC(i2c,0x04,0x8080); //headphone	off
		
		//printk("headphone plug out\n");
	}
	else
	{
		ALC5625_write_IIC(i2c,0x02,0x8080); //speaker off
		for(j=0;j<20;j++);
		ALC5625_write_IIC(i2c,0x04,0x0000); //headphone on
		
		//printk("headphone plug in\n");
	}
	//printk("reg 0x02: %04x\n",ALC5625_read_IIC(i2c,0x02));
	//printk("reg 0x04: %04x\n",ALC5625_read_IIC(i2c,0x04));
}
EXPORT_SYMBOL(alc5623_headset);


static int get_coeff(struct snd_soc_codec *codec, int rate)
{
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].fs * rate == alc5623->sysclk)
			return i;
	}
	return -EINVAL;
}

/*
 * Clock after PLL and dividers
 */
static int alc5623_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
    pr_debug("%s  freq is %d \n",__func__,freq);

	switch (freq) {
	case  8192000:
	case 11289600:
	case 12288000:
	case 16384000:
	case 16934400:
	case 18432000:
	case 22579200:
	case 24576000:
		alc5623->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int alc5623_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	/*struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;
    int ret;
    printk("%s .....%d.......\n",__func__,__LINE__);


	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = ALC5623_DAI_SDP_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = ALC5623_DAI_SDP_SLAVE_MODE;
		break;
	default:
		return -EINVAL;
	}
    printk("%s .....%d.......\n",__func__,__LINE__);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= ALC5623_DAI_I2S_DF_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= ALC5623_DAI_I2S_DF_RIGHT;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= ALC5623_DAI_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= ALC5623_DAI_I2S_DF_PCM;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= ALC5623_DAI_I2S_DF_PCM | ALC5623_DAI_I2S_PCM_MODE;
		break;
	default:
		return -EINVAL;
	}
    printk("%s .....%d.......\n",__func__,__LINE__);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= ALC5623_DAI_MAIN_I2S_BCLK_POL_CTRL;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= ALC5623_DAI_MAIN_I2S_BCLK_POL_CTRL;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}

    ret=i2c_smbus_write_word_data(i2c, ALC5623_DAI_CONTROL, iface);

    printk("%s .......ret:%d....over \n",__func__,ret);*/

	return 0;
}

static int alc5623_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	/*struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	int coeff, rate;
	u16 iface;

	iface = i2c_smbus_read_word_data(i2c, ALC5623_DAI_CONTROL);
	iface &= ~ALC5623_DAI_I2S_DL_MASK;
    printk("%s .....%d.......\n",__func__,__LINE__);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		iface |= ALC5623_DAI_I2S_DL_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= ALC5623_DAI_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= ALC5623_DAI_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= ALC5623_DAI_I2S_DL_32;
		break;
	default:
		return -EINVAL;
	}
    //set_init_regs(codec);

	//snd_soc_write(codec, ALC5623_DAI_CONTROL, iface);
    i2c_smbus_write_word_data(i2c,ALC5623_DAI_CONTROL, iface);
	rate = params_rate(params);
	coeff = get_coeff(codec, rate);
	if (coeff < 0)
		return -EINVAL;

	coeff = coeff_div[coeff].regvalue;
	pr_debug("%s: sysclk=%d,rate=%d,coeff=0x%04x\n",
		__func__, alc5623->sysclk, rate, coeff);
	//snd_soc_write(codec, ALC5623_STEREO_AD_DA_CLK_CTRL, coeff);
    i2c_smbus_write_word_data(i2c,ALC5623_STEREO_AD_DA_CLK_CTRL, coeff);

    //init_codec(i2c);*/
    //alc5623_set_bias_level(codec,SND_SOC_BIAS_ON);
    //init_codec(i2c);
	return 0;
}

static int alc5623_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 hp_mute = ALC5623_MISC_M_DAC_L_INPUT | ALC5623_MISC_M_DAC_R_INPUT;
	u16 mute_reg = i2c_smbus_read_word_data(i2c, ALC5623_MISC_CTRL) & ~hp_mute;
    pr_debug("%s .....%d.....mute : %d..\n",__func__,__LINE__,mute);
	if (mute)
		mute_reg |= hp_mute;

	//return snd_soc_write(codec, ALC5623_MISC_CTRL, mute_reg);
    return i2c_smbus_write_word_data(i2c,ALC5623_MISC_CTRL, mute_reg);
    return 0;

}

#define ALC5623_ADD2_POWER_EN (ALC5623_PWR_ADD2_VREF \
	| ALC5623_PWR_ADD2_DAC_REF_CIR)

#define ALC5623_ADD3_POWER_EN (ALC5623_PWR_ADD3_MAIN_BIAS \
	| ALC5623_PWR_ADD3_MIC1_BOOST_AD)

#define ALC5623_ADD1_POWER_EN \
	(ALC5623_PWR_ADD1_SHORT_CURR_DET_EN | ALC5623_PWR_ADD1_SOFTGEN_EN \
	| ALC5623_PWR_ADD1_DEPOP_BUF_HP | ALC5623_PWR_ADD1_HP_OUT_AMP \
	| ALC5623_PWR_ADD1_HP_OUT_ENH_AMP)

#define ALC5623_ADD1_POWER_EN_5622 \
	(ALC5623_PWR_ADD1_SHORT_CURR_DET_EN \
	| ALC5623_PWR_ADD1_HP_OUT_AMP)

static void enable_power_depop(struct snd_soc_codec *codec)
{
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
    pr_debug("%s .....%d.......\n",__func__,__LINE__);
	//snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD1,
	//			ALC5623_PWR_ADD1_SOFTGEN_EN,
	//			ALC5623_PWR_ADD1_SOFTGEN_EN);

    i2c_smbus_write_word_data(i2c,ALC5623_PWR_MANAG_ADD1,i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD1)|ALC5623_PWR_ADD1_SOFTGEN_EN);

	//snd_soc_write(codec, ALC5623_PWR_MANAG_ADD3, ALC5623_ADD3_POWER_EN);
    i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD3,i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD3)|ALC5623_ADD3_POWER_EN);
	//snd_soc_update_bits(codec, ALC5623_MISC_CTRL,
	//			ALC5623_MISC_HP_DEPOP_MODE2_EN,
	//			ALC5623_MISC_HP_DEPOP_MODE2_EN);
    i2c_smbus_write_word_data(i2c,ALC5623_MISC_CTRL,i2c_smbus_read_word_data(i2c,ALC5623_MISC_CTRL)|ALC5623_MISC_HP_DEPOP_MODE2_EN);

	msleep(500);

	//snd_soc_write(codec, ALC5623_PWR_MANAG_ADD2, ALC5623_ADD2_POWER_EN);
    i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD2, i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD2)|ALC5623_ADD2_POWER_EN);
	// avoid writing '1' into 5622 reserved bits
	//if (alc5623->id == 0x22)
	//	snd_soc_write(codec, ALC5623_PWR_MANAG_ADD1,
	//		ALC5623_ADD1_POWER_EN_5622);
	//else
	//	snd_soc_write(codec, ALC5623_PWR_MANAG_ADD1,
	//		ALC5623_ADD1_POWER_EN);
    i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD1,i2c_smbus_read_word_data(i2c,ALC5623_PWR_MANAG_ADD1)|ALC5623_ADD1_POWER_EN);

	//snd_soc_update_bits(codec, ALC5623_MISC_CTRL,
	//			ALC5623_MISC_HP_DEPOP_MODE2_EN,
	//			0);
    i2c_smbus_write_word_data(i2c,ALC5623_MISC_CTRL,i2c_smbus_read_word_data(i2c,ALC5623_MISC_CTRL)&~ALC5623_MISC_HP_DEPOP_MODE2_EN);

}

static int alc5623_set_bias_level(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
    pr_debug("%s .....%d.......\n",__func__,__LINE__);
	switch (level) {
	case SND_SOC_BIAS_ON:
        init_codec(i2c);
       /* ALC5625_write_IIC(i2c,RT5621_PWR_MANAG_ADD1,0xcd60);
        ALC5625_write_IIC(i2c,RT5621_PWR_MANAG_ADD2,0xf7ff);
        ALC5625_write_IIC(i2c,RT5621_PWR_MANAG_ADD3,0xf6ff);
        ALC5625_write_IICMask(i2c,RT5621_SPK_OUT_VOL		,~(RT_L_MUTE|RT_R_MUTE),RT_L_MUTE|RT_R_MUTE);	//Mute Speaker right/left channel
        ALC5625_write_IICMask(i2c,RT5621_HP_OUT_VOL 		,~(RT_L_MUTE|RT_R_MUTE),RT_L_MUTE|RT_R_MUTE);	//Mute headphone right/left channel
        //ALC5625_write_IICMask(client,RT5621_MONO_AUX_OUT_VOL,0,RT_L_MUTE|RT_R_MUTE);	//Mute Aux/Mono right/left channel
        ALC5625_write_IIC(i2c,RT5621_STEREO_DAC_VOL	,0x0808);	//Mute DAC to HP,Speaker,Mono Mixer
        enable_power_depop(codec);*/
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:

		i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD2,
				ALC5623_PWR_ADD2_VREF);

		i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD3,
				ALC5623_PWR_ADD3_MAIN_BIAS);
		break;
	case SND_SOC_BIAS_OFF:
		i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD2, 0);
		i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD3, 0);
		i2c_smbus_write_word_data(i2c, ALC5623_PWR_MANAG_ADD1, 0);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define ALC5623_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops alc5623_dai_ops = {
		.hw_params = alc5623_pcm_hw_params,
		.digital_mute = alc5623_mute,
		.set_fmt = alc5623_set_dai_fmt,
		.set_sysclk = alc5623_set_dai_sysclk,
		.set_pll = alc5623_set_dai_pll,
};

static struct snd_soc_dai_driver alc5623_dai = {
	.name = "alc5623-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5623_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5623_FORMATS,},

	.ops = &alc5623_dai_ops,
};

static int alc5623_suspend(struct snd_soc_codec *codec)
{
	alc5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int alc5623_resume(struct snd_soc_codec *codec)
{
	int i, step = codec->driver->reg_cache_step;
	u16 *cache = codec->reg_cache;


	for (i = 2 ; i < codec->driver->reg_cache_size ; i += step)
		//snd_soc_write(codec, i, cache[i]);
        i2c_smbus_write_word_data(i2c, i, cache[i]);

	alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);


	if (codec->dapm.suspend_bias_level == SND_SOC_BIAS_ON) {
		alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		codec->dapm.bias_level = SND_SOC_BIAS_ON;
		alc5623_set_bias_level(codec, codec->dapm.bias_level);
	}

	return 0;
}

static int alc5623_probe(struct snd_soc_codec *codec)
{
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;
    printk("%s ............\n",__func__);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, alc5623->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	alc5623_reset(codec);



	/* power on device */
	alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

/*	if (alc5623->add_ctrl) {
		//snd_soc_write(codec, ALC5623_ADD_CTRL_REG,
		//		alc5623->add_ctrl);

        i2c_smbus_write_word_data(i2c, ALC5623_ADD_CTRL_REG,
				alc5623->add_ctrl);

	}*/

	/*if (alc5623->jack_det_ctrl) {
		//snd_soc_write(codec, ALC5623_JACK_DET_CTRL,
		//		alc5623->jack_det_ctrl);
        i2c_smbus_write_word_data(i2c, ALC5623_JACK_DET_CTRL,
				alc5623->jack_det_ctrl);
	}*/

	switch (alc5623->id) {
	case 0x21:
//		snd_soc_add_codec_controls(codec, alc5621_vol_snd_controls,
//			ARRAY_SIZE(alc5621_vol_snd_controls));
		break;
	case 0x22:
//		snd_soc_add_codec_controls(codec, alc5622_vol_snd_controls,
//			ARRAY_SIZE(alc5622_vol_snd_controls));
		break;
	case 0x23:
//		snd_soc_add_codec_controls(codec, alc5623_vol_snd_controls,
//			ARRAY_SIZE(alc5623_vol_snd_controls));
		break;
	default:
		return -EINVAL;
	}

//	snd_soc_add_codec_controls(codec, alc5623_snd_controls,
//			ARRAY_SIZE(alc5623_snd_controls));

//	snd_soc_dapm_new_controls(dapm, alc5623_dapm_widgets,
//					ARRAY_SIZE(alc5623_dapm_widgets));

	/* set up audio path interconnects */
//	snd_soc_dapm_add_routes(dapm, intercon, ARRAY_SIZE(intercon));

	switch (alc5623->id) {
	case 0x21:
	case 0x22:
//		snd_soc_dapm_new_controls(dapm, alc5623_dapm_amp_widgets,
//					ARRAY_SIZE(alc5623_dapm_amp_widgets));
//		snd_soc_dapm_add_routes(dapm, intercon_amp_spk,
//					ARRAY_SIZE(intercon_amp_spk));
		break;
	case 0x23:
//		snd_soc_dapm_add_routes(dapm, intercon_spk,
//					ARRAY_SIZE(intercon_spk));
		break;
	default:
		return -EINVAL;
	}

    init_codec(i2c);
    alc5623_fill_cache(codec);

    //show_codec_regs(i2c);	//show codec regs' value;
	return ret;
}

/* power down chip */
static int alc5623_remove(struct snd_soc_codec *codec)
{
	alc5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_device_alc5623 = {
	.probe = alc5623_probe,
	.remove = alc5623_remove,
	.suspend = alc5623_suspend,
	.resume = alc5623_resume,
	.set_bias_level = alc5623_set_bias_level,
	.reg_cache_size = ALC5623_VENDOR_ID2+2,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
};

/*
 * ALC5623 2 wire address is determined by A1 pin
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */
static __devinit int alc5623_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct alc5623_platform_data *pdata;
	struct alc5623_priv *alc5623;
	int ret, vid1, vid2;

    printk("%s ............\n",__func__);

	/*vid1 = i2c_smbus_read_word_data(client, ALC5623_VENDOR_ID1);
	if (vid1 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		return -EIO;
	}
	vid1 = ((vid1 & 0xff) << 8) | (vid1 >> 8);

	vid2 = i2c_smbus_read_word_data(client, ALC5623_VENDOR_ID2);
	if (vid2 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		return -EIO;
	}
    vid2 &=0xff;

	if ((vid1 != 0x10ec) || (vid2 != id->driver_data)) {
		dev_err(&client->dev, "unknown or wrong codec\n");
		dev_err(&client->dev, "Expected %x:%lx, got %x:%x\n",
				0x10ec, id->driver_data,
				vid1, vid2);
		return -ENODEV;
	}

    printk("Found codec id : alc56%02x\n", vid2);*/
	alc5623 = devm_kzalloc(&client->dev, sizeof(struct alc5623_priv),
			       GFP_KERNEL);
	if (alc5623 == NULL)
		return -ENOMEM;

	pdata = client->dev.platform_data;
	if (pdata) {
		alc5623->add_ctrl = pdata->add_ctrl;
		alc5623->jack_det_ctrl = pdata->jack_det_ctrl;
	}
    vid2 = 0x21;
	alc5623->id = vid2;
	switch (alc5623->id) {
	case 0x21:
		alc5623_dai.name = "alc5621-hifi";
		break;
	case 0x22:
		alc5623_dai.name = "alc5622-hifi";
		break;
	case 0x23:
		alc5623_dai.name = "alc5623-hifi";
		break;
	default:
		return -EINVAL;
	}

	i2c_set_clientdata(client, alc5623);
    i2c=client;
	alc5623->control_type = SND_SOC_I2C;

	ret =  snd_soc_register_codec(&client->dev,
		&soc_codec_device_alc5623, &alc5623_dai, 1);
	if (ret != 0)
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);

	return ret;
}

static __devexit int alc5623_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id alc5623_i2c_table[] = {
	{"alc562x-codec", 0x21},
	{"alc5622", 0x22},
	{"alc5623", 0x23},
	{}
};
MODULE_DEVICE_TABLE(i2c, alc5623_i2c_table);

/*  i2c codec control layer */
static struct i2c_driver alc5623_i2c_driver = {
	.driver = {
		.name = "alc562x-codec",
		.owner = THIS_MODULE,
	},
	.probe = alc5623_i2c_probe,
	.remove =  __devexit_p(alc5623_i2c_remove),
	.id_table = alc5623_i2c_table,
};

static int __init alc5623_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&alc5623_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "%s: can't add i2c driver", __func__);
		return ret;
	}

	return ret;
}
module_init(alc5623_modinit);

static void __exit alc5623_modexit(void)
{
	i2c_del_driver(&alc5623_i2c_driver);
}
module_exit(alc5623_modexit);

MODULE_DESCRIPTION("ASoC alc5621/2/3 driver");
MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_LICENSE("GPL");
