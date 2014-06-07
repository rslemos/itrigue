/*******************************************************************************
 * BEGIN COPYRIGHT NOTICE
 * 
 * This file is part of program "I-Trigue 2.1 3300 Digital Control"
 * Copyright 2013-2014  R. Lemos
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * END COPYRIGHT NOTICE
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/control.h>

/* MODULE PARAMETERS */
static uint onoff_gpio = 139;
static uint pot_spi_bus = 4;
static uint pot_spi_cs = 0;
static uint speed_hz = 1500000; /* found experimentally */

module_param(onoff_gpio, uint, S_IRUGO);
MODULE_PARM_DESC(onoff_gpio, "GPIO number of power switch");
module_param(pot_spi_bus, uint, S_IRUGO);
MODULE_PARM_DESC(pot_spi_bus, "SPI bus number of potentiometers");
module_param(pot_spi_cs, uint, S_IRUGO);
MODULE_PARM_DESC(pot_spi_cs, "SPI bus chip select of potentiometers");
module_param(speed_hz, uint, S_IRUGO);
MODULE_PARM_DESC(speed_hz, "SPI bus speed (in Hz)");

/* CORE FUNCTIONS */

#define UNKNOWN -1


static struct spi_device *spi_pot_device;

static int pot[] = { UNKNOWN, UNKNOWN };

static inline int get_onoff(void) {
	return gpio_get_value( onoff_gpio );
}

static inline void set_onoff(int onoff) {
	gpio_set_value( onoff_gpio, onoff );
}

static inline int get_pot(int idx) {
	return pot[idx];
}

#define CMD_SET_POT_X(idx, value) (((0x10 | (0x1 << idx)) << 8) | value)

static inline void set_pot(int idx, int value) {
	static uint16_t write_data;

	write_data = CMD_SET_POT_X(idx, value);
	spi_write( spi_pot_device, &write_data, sizeof write_data );

	pot[idx] = value;
}

/* ALSA FUNCTIONS */

static int playback_switch_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int playback_switch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	ucontrol->value.integer.value[0] = get_onoff();

	return 0;
}

static int playback_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	int changed = ucontrol->value.integer.value[0] != get_onoff();

	set_onoff( ucontrol->value.integer.value[0] );

	return changed;
}

static int playback_pot_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;

	return 0;
}

static int playback_pot_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	ucontrol->value.integer.value[0] = get_pot( kcontrol->private_value );

	return 0;
}

static int playback_pot_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	int changed = ucontrol->value.integer.value[0] != get_pot( kcontrol->private_value );
	
	set_pot( kcontrol->private_value, ucontrol->value.integer.value[0] );

	return changed;
}

/* SETUP GPIO */

static inline __init int gpio_init(void) {
	int ret;

	ret = gpio_request( onoff_gpio, "itrigue::on/off" );
	if( ret )
		return ret;

	ret = gpio_direction_output( onoff_gpio, 0 );
	if( ret )
		gpio_free( onoff_gpio );
	else
		printk( KERN_INFO "I-Trigue 3300 power switch set to GPIO port %u\n", onoff_gpio );

	return ret;
}

static inline void gpio_exit(void) {
	gpio_set_value( onoff_gpio, 0 );

	gpio_free( onoff_gpio );
}

/* SETUP SPI */

static inline __init int spi_init(void) {
	struct spi_board_info spi_pot_device_info = {
		.modalias = "itrigue",
		.max_speed_hz = speed_hz,
		.bus_num = pot_spi_bus,
		.chip_select = pot_spi_cs,
		.mode = 0,
	};

	struct spi_master *master;

	int ret;

	master = spi_busnum_to_master( spi_pot_device_info.bus_num );
	if( !master )
		return -ENODEV;

	spi_pot_device = spi_new_device( master, &spi_pot_device_info );
	if( !spi_pot_device )
		return -ENODEV;

	spi_pot_device->bits_per_word = 16;

	ret = spi_setup( spi_pot_device );
	if( ret )
		spi_unregister_device( spi_pot_device );
	else
		printk( KERN_INFO "I-Trigue 3300 potentiometers registered to SPI bus %u, chipselect %u\n", 
			pot_spi_bus, pot_spi_cs );

	return ret;
}

static inline void spi_exit(void) {
	spi_unregister_device( spi_pot_device );
}

/* SETUP ALSA */

static struct snd_card *card;

static inline __init int alsa_init(void) {
	struct snd_kcontrol_new ctl_onoff = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Switch",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_switch_info,
		.get = playback_switch_get,
		.put = playback_switch_put
	};

	struct snd_kcontrol_new ctl_volume = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_pot_info,
		.get = playback_pot_get,
		.put = playback_pot_put,
		.private_value = 1,
	};

	struct snd_kcontrol_new ctl_tone = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Bass Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_pot_info,
		.get = playback_pot_get,
		.put = playback_pot_put,
		.private_value = 0,
	};

	int ret;

	ret = snd_card_create(-1, "Itrigue", THIS_MODULE, 0, &card);
	if( ret )
		return ret;

	strcpy( card->driver, "I-Trigue" );
	strcpy( card->shortname, "I-Trigue 3300" );
	sprintf( card->longname, "%s at spi %d.%d (@ %d Hz), gpio %d", 
		card->shortname, pot_spi_bus, pot_spi_cs, speed_hz, onoff_gpio );

	/* ALSA controls */
	ret = snd_ctl_add( card, snd_ctl_new1( &ctl_onoff, NULL ) );
	if( ret )
		goto bailout;

	ret = snd_ctl_add( card, snd_ctl_new1( &ctl_volume, NULL ) );
	if( ret )
		goto bailout;

	ret = snd_ctl_add( card, snd_ctl_new1( &ctl_tone, NULL ) );
	if( ret )
		goto bailout;

	ret = snd_card_register( card );
	if( ret )
		goto bailout;

	return ret;

bailout:
	snd_card_free( card );

	return ret;
}

static inline void alsa_exit(void) {
	snd_card_free( card );
}

/* SETUP MODULE */

static int __init itrigue_init(void) {
	int ret;


	ret = gpio_init();
	if( ret )
		return ret;

	ret = spi_init();
	if( ret ) {
		gpio_exit();
		return ret;
	}

	ret = alsa_init();
	if( ret ) {
		spi_exit();
		gpio_exit();
		return ret;
	}

	return ret;
}

static void __exit itrigue_exit(void) {
	alsa_exit();

	spi_exit();

	printk(KERN_INFO "I-Trigue 3300 off.\n");

	gpio_exit();
}

module_init(itrigue_init);
module_exit(itrigue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodrigo Lemos");
MODULE_DESCRIPTION("I-Trigue 3300 Controller");

