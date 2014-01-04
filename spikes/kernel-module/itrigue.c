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

#define GPIO_ONOFF 139
#define GPIO_POT_CS 138
#define GPIO_POT_SCK 137
#define GPIO_POT_SIMO 136

#define HIGH 1
#define LOW 0

static int volume;
static int pitch;

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

static ssize_t volume_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf( buf, "%d\n", volume );
}

static ssize_t volume_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	int _volume;

	sscanf( buf, "%d", &_volume );

	if( _volume < 0 )
		_volume = 0;
	if( _volume >  255 )
		_volume = 255;

	volume = _volume;

	return count;
}

static ssize_t pitch_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf( buf, "%d\n", pitch );
}

static ssize_t pitch_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	int _pitch;

	sscanf( buf, "%d", &_pitch );

	if( _pitch < 0 )
		_pitch = 0;
	if( _pitch >  255 )
		_pitch = 255;

	pitch = _pitch;

	return count;
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

	ret = gpio_request( GPIO_ONOFF, "itrigue::on/off" );
	cleanup_if_nonzero( ret, gpio_onoff_request );

	ret = gpio_request( GPIO_POT_CS, "itrigue::pot_cs" );
	cleanup_if_nonzero( ret, gpio_pot_cs_request );

	ret = gpio_request( GPIO_POT_SCK, "itrigue::pot_sck" );
	cleanup_if_nonzero( ret, gpio_pot_sck_request );

	ret = gpio_request( GPIO_POT_SIMO, "itrigue::pot_simo" );
	cleanup_if_nonzero( ret, gpio_pot_simo_request );

	ret = gpio_direction_output( GPIO_ONOFF, LOW );
	cleanup_if_nonzero( ret, gpio_onoff_direction_output );

	ret = gpio_direction_output( GPIO_POT_CS, HIGH );
	cleanup_if_nonzero( ret, gpio_pot_cs_direction_output );

	ret = gpio_direction_output( GPIO_POT_SCK, LOW );
	cleanup_if_nonzero( ret, gpio_pot_sck_direction_output );

	ret = gpio_direction_output( GPIO_POT_SIMO, LOW );
	cleanup_if_nonzero( ret, gpio_pot_simo_direction_output );

	itrigue_kobj = kobject_create_and_add( "itrigue", kernel_kobj );
	cleanup_if_nonzero_with_ret( !itrigue_kobj, kobject_create_and_add, -ENOMEM );

	ret = sysfs_create_group( itrigue_kobj, &attr_group );
	if( ret )	
		kobject_put( itrigue_kobj );

	return 0;

fail_kobject_create_and_add:
fail_gpio_pot_simo_direction_output:
fail_gpio_pot_sck_direction_output:
fail_gpio_pot_cs_direction_output:
fail_gpio_onoff_direction_output:
	gpio_free( GPIO_POT_SIMO );

fail_gpio_pot_simo_request:
	gpio_free( GPIO_POT_SCK );

fail_gpio_pot_sck_request:
	gpio_free( GPIO_POT_CS );

fail_gpio_pot_cs_request:
	gpio_free( GPIO_ONOFF );

fail_gpio_onoff_request:
	return ret;
}

static void __exit itrigue_exit(void) {
	kobject_put( itrigue_kobj );

	gpio_set_value( GPIO_ONOFF, LOW );

	printk(KERN_INFO "I-Trigue 3300 off.\n");

	gpio_free( GPIO_POT_SIMO );
	gpio_free( GPIO_POT_SCK );
	gpio_free( GPIO_POT_CS );
	gpio_free( GPIO_ONOFF );
}

module_init(itrigue_init);
module_exit(itrigue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodrigo Lemos");
MODULE_DESCRIPTION("I-Trigue 3300 Controller");

