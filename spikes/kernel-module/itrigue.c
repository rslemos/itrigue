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
#define HIGH 1
#define LOW 0

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

static struct kobj_attribute onoff_attribute = __ATTR( onoff, 0666, onoff_show, onoff_store);

static struct attribute *attrs[] = {
	&onoff_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *itrigue_kobj;

static int __init itrigue_init(void) {
	int ret;

	ret = gpio_request( GPIO_ONOFF, "itrigue::on/off" );
	if( ret ) {
		goto fail_gpio_onoff_request;
	}

	ret = gpio_direction_output( GPIO_ONOFF, LOW );
	if( ret ) {
		goto fail_gpio_onoff_direction_output;
	}

	itrigue_kobj = kobject_create_and_add( "itrigue", kernel_kobj );
	if( !itrigue_kobj ) {
		ret = -ENOMEM;
		goto fail_kobject_create_and_add;
	}

	ret = sysfs_create_group( itrigue_kobj, &attr_group );
	if( ret ) {
		kobject_put( itrigue_kobj );
	}
		

	return 0;


fail_kobject_create_and_add:
fail_gpio_onoff_direction_output:
	gpio_free( GPIO_ONOFF );

fail_gpio_onoff_request:
	return ret;
}

static void __exit itrigue_exit(void) {
	kobject_put( itrigue_kobj );

	gpio_set_value( GPIO_ONOFF, LOW );

	printk(KERN_INFO "I-Trigue 3300 off.\n");

	gpio_free( GPIO_ONOFF );
}

module_init(itrigue_init);
module_exit(itrigue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodrigo Lemos");
MODULE_DESCRIPTION("I-Trigue 3300 Controller");

