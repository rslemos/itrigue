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

#include <execinfo.h>

#include <jansson.h>

#define DEEPEST_BACKTRACE 20


json_t*
error (const char *fmt,...)
{

	char buffer[4096];
	char *p;
	int max;

	void *bt[DEEPEST_BACKTRACE];
	int backtrace_size;
	void **backtrace_p = bt;

	char **backtrace_sym;

	json_t *error = json_object();

	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof buffer, fmt, va);
	va_end(va);

	json_object_set_new(error, "message", json_string(buffer));

	backtrace_size = backtrace(bt, sizeof bt);
	backtrace_sym = backtrace_symbols(backtrace_p, backtrace_size);

	buffer[0] = 0;
	p = buffer;
	max = sizeof buffer;

	backtrace_p++;
	backtrace_sym++;
	backtrace_size--;
	
	while (backtrace_size > 0) {
		p += snprintf(p, max, "%p: %s\n", *backtrace_p, *backtrace_sym);

		backtrace_p++;
		backtrace_sym++;
		backtrace_size--;
	}

	json_object_set_new(error, "backtrace", json_string(buffer));
	
	return error;
}

