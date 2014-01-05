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


static ssize_t onoff_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	int onoff = gpio_get_value( GPIO_ONOFF );

	return sprintf( buf, "%d\n", onoff );
}

static ssize_t onoff_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	int onoff;

	sscanf( buf, "%d", &onoff );

	gpio_set_value( GPIO_ONOFF, onoff != 0 );

	return count;
}

static inline ssize_t pot_show(char *buf, int *pot) {
	if( *pot == UNKNOWN )
		return sprintf( buf, "unknown\n" );
	else
		return sprintf( buf, "%d\n", *pot );
}

static inline ssize_t pot_store(const char *buf, size_t count, int *pot, int cmd) {
	sscanf( buf, "%d", pot );

	if( *pot < 0 )
		*pot = 0;
	if( *pot > 255 )
		*pot = 255;

	write_data = cmd | *pot;
	printk( KERN_INFO "spi_write( ..., 0x%x, %d )\n", write_data, sizeof write_data );
	spi_write( spi_pot_device, &write_data, sizeof write_data );

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

static int __init itrigue_init(void) {
	int ret;
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

	return 0;

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

