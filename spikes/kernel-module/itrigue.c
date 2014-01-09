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
#include <linux/gpio.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/control.h>

#define GPIO_ONOFF 139

#define HIGH 1
#define LOW 0

static struct spi_board_info spi_pot_device_info = {
	.modalias = "itrigue",
	.max_speed_hz = 1500000, /* found experimentally */
	.bus_num = 4,
	.chip_select = 0,
	.mode = 0,
};

static struct spi_device *spi_pot_device;

#define UNKNOWN -1

static int volume = UNKNOWN;
static int pitch = UNKNOWN;
	
static uint16_t write_data;

static inline int get_onoff(void) {
	return gpio_get_value( GPIO_ONOFF );
}

static inline void set_onoff(int onoff) {
	gpio_set_value( GPIO_ONOFF, onoff );
}

static inline int get_pot(int *pot) {
	return *pot;
}

static inline void set_pot(int *pot, int value, int cmd) {
	*pot = value;

	if( *pot < 0 )
		*pot = 0;
	if( *pot > 255 )
		*pot = 255;

	write_data = cmd | *pot;
	printk( KERN_INFO "spi_write( ..., 0x%x, %zu )\n", write_data, sizeof write_data );
	spi_write( spi_pot_device, &write_data, sizeof write_data );
}

static ssize_t onoff_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	int onoff = get_onoff();

	return sprintf( buf, "%d\n", onoff );
}

static ssize_t onoff_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	int onoff;

	sscanf( buf, "%d", &onoff );

	set_onoff( onoff != 0 );

	return count;
}

static inline ssize_t pot_show(char *buf, int *pot) {
	int value = get_pot( pot );
	if( value == UNKNOWN )
		return sprintf( buf, "unknown\n" );
	else
		return sprintf( buf, "%d\n", value );
}

static inline ssize_t pot_store(const char *buf, size_t count, int *pot, int cmd) {
	int value;
	sscanf( buf, "%d", &value );

	set_pot( pot, value, cmd );

	return count;
}

static ssize_t volume_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return pot_show( buf, &volume );
}

static ssize_t pitch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return pot_show( buf, &pitch );
}

#define SET_POT_0 0x11 << 8
#define SET_POT_1 0x12 << 8

static ssize_t volume_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	return pot_store( buf, count, &volume, SET_POT_1 );
}

static ssize_t pitch_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	return pot_store( buf, count, &pitch, SET_POT_0 );
}

static struct kobj_attribute onoff_attribute = __ATTR( onoff, 0666, onoff_show, onoff_store);
static struct kobj_attribute volume_attribute = __ATTR( volume, 0666, volume_show, volume_store);
static struct kobj_attribute pitch_attribute = __ATTR( pitch, 0666, pitch_show, pitch_store);

static struct attribute *attrs[] = {
	&onoff_attribute.attr,
	&volume_attribute.attr,
	&pitch_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *itrigue_kobj;

#define cleanup_if_nonzero(value, cleanup_label)					\
	do {															\
		if( (value) )												\
			goto fail_##cleanup_label;								\
	} while(0)


#define cleanup_if_nonzero_with_ret(value, cleanup_label, retval)	\
	do {															\
		if( (value) ) {												\
			ret = (retval);											\
			goto fail_##cleanup_label;								\
		}															\
	} while(0)

static struct snd_card *card;

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

static int playback_volume_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;

	return 0;
}

static int playback_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	ucontrol->value.integer.value[0] = get_pot( &volume );

	return 0;
}

static int playback_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	int changed = ucontrol->value.integer.value[0] != get_pot( &volume );
	
	set_pot( &volume, ucontrol->value.integer.value[0], SET_POT_1 );

	return changed;
}

static int playback_pitch_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo) {
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;

	return 0;
}

static int playback_pitch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	ucontrol->value.integer.value[0] = get_pot( &pitch );

	return 0;
}

static int playback_pitch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	int changed = ucontrol->value.integer.value[0] != get_pot( &pitch );

	set_pot( &pitch, ucontrol->value.integer.value[0], SET_POT_0 );

	return changed;
}

static struct snd_kcontrol_new controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Switch",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_switch_info,
		.get = playback_switch_get,
		.put = playback_switch_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_volume_info,
		.get = playback_volume_get,
		.put = playback_volume_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Tone Control - Bass",
		.device = 0, .subdevice = 1,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = playback_pitch_info,
		.get = playback_pitch_get,
		.put = playback_pitch_put
	},
};

static int __init itrigue_init(void) {
	int ret;
	int i;

	struct spi_master *master;

	ret = gpio_request( GPIO_ONOFF, "itrigue::on/off" );
	cleanup_if_nonzero( ret, gpio_onoff_request );

	ret = gpio_direction_output( GPIO_ONOFF, LOW );
	cleanup_if_nonzero( ret, gpio_onoff_direction_output );

	master = spi_busnum_to_master( 4 );
	cleanup_if_nonzero_with_ret( !master, spi_busnum_to_master, -ENODEV );

	spi_pot_device = spi_new_device( master, &spi_pot_device_info );
	cleanup_if_nonzero_with_ret( !spi_pot_device, spi_new_device, -ENODEV );

	spi_pot_device->bits_per_word = 16;
	ret = spi_setup( spi_pot_device );
	cleanup_if_nonzero( ret, spi_setup );

	itrigue_kobj = kobject_create_and_add( "itrigue", kernel_kobj );
	cleanup_if_nonzero_with_ret( !itrigue_kobj, kobject_create_and_add, -ENOMEM );

	ret = sysfs_create_group( itrigue_kobj, &attr_group );
	if( ret )	
		kobject_put( itrigue_kobj );

	ret = snd_card_create(-1, "Itrigue", THIS_MODULE, 0, &card);
	cleanup_if_nonzero( ret, snd_card_create );

	strcpy( card->driver, "I-Trigue" );
	strcpy( card->shortname, "I-Trigue 3300" );
	sprintf( card->longname, "%s at spi %d.%d, gpio %d", 
		card->shortname, spi_pot_device_info.bus_num,
		spi_pot_device_info.chip_select, GPIO_ONOFF );

	for( i = 0; i < ARRAY_SIZE( controls ); i++ ) {
		ret = snd_ctl_add( card, snd_ctl_new1( &controls[i], NULL ) );
		cleanup_if_nonzero( ret, snd_ctl_new1_or_add );
	}

	ret = snd_card_register( card );
	cleanup_if_nonzero( ret, snd_card_register );

	return 0;

fail_snd_ctl_new1_or_add:
fail_snd_card_register:
	snd_card_free( card );

fail_snd_card_create:
fail_kobject_create_and_add:
fail_spi_setup:
	spi_unregister_device( spi_pot_device );

fail_spi_new_device:
fail_spi_busnum_to_master:

fail_gpio_onoff_direction_output:

	gpio_free( GPIO_ONOFF );

fail_gpio_onoff_request:
	return ret;
}

static void __exit itrigue_exit(void) {
	snd_card_free( card );

	kobject_put( itrigue_kobj );

	spi_unregister_device( spi_pot_device );

	gpio_set_value( GPIO_ONOFF, LOW );

	printk(KERN_INFO "I-Trigue 3300 off.\n");

	gpio_free( GPIO_ONOFF );
}

module_init(itrigue_init);
module_exit(itrigue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodrigo Lemos");
MODULE_DESCRIPTION("I-Trigue 3300 Controller");

