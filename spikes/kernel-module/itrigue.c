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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodrigo Lemos");
MODULE_DESCRIPTION("I-Trigue 3300 Controller");

#define GPIO_ONOFF 139
#define HIGH 1
#define LOW 0

static int __init itrigue_init(void) {
	int ret;

	ret = gpio_request( GPIO_ONOFF, "itrigue" );
	if( ret ) {
		return ret;
	}

	ret = gpio_direction_output( GPIO_ONOFF, HIGH );
	if( ret ) {
		gpio_free( GPIO_ONOFF );
		return ret;
	}

	printk(KERN_INFO "I-Trigue 3300 on\n");

	return 0;
}

static void __exit itrigue_exit(void) {
	gpio_set_value( GPIO_ONOFF, LOW );

	printk(KERN_INFO "I-Trigue 3300 off.\n");

	gpio_free( GPIO_ONOFF );
}

module_init(itrigue_init);
module_exit(itrigue_exit);
