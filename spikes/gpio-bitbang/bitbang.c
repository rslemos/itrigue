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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BUF_SIZE 4096
#define false 0
#define true !false


static void trim(char** s, size_t* length) {
	while( **s == ' ' && *length > 0 ) {
		(*s)++; (*length)--;
	}

	while( ( *((*s) + *length - 1) == ' ' || *((*s) + *length - 1) == '\n' ) && *length > 0 ) {
		(*length)--;
	}
}

static int echo0(char* args, size_t length) {
	char *string;
	size_t strlength;

	int close_at_end;
	int fd;

	char *gt = (char*) memchr( args, '>', length );
	
	string = args;
	if( gt == NULL ) {
		close_at_end = false;
		fd = 1;
		strlength = length;
	} else {
		strlength = gt - args;
		length -= gt - args;

		gt++; length--;
		trim( &gt, &length );

		if( length > 0 && *gt == '#' ) {
			gt++;
			length--;

			sscanf( gt, "%d", &fd );
			fd += 100;
			close_at_end = false;
		} else {
			char filename[255];
			memcpy( filename, gt, length );
			filename[length] = '\0';

			fd = open( filename, O_WRONLY );
			if( fd < 0 )
				return fd;

			close_at_end = true;
		}

	}

	// trim
	trim( &string, &strlength );

	write( fd, string, strlength );

	if( close_at_end )
		close( fd );

	return 0;
}

static int open0(char* args, size_t length) {
	char *arg_fname;
	size_t arg_fname_len;

	char *gt = (char*) memchr( args, '>', length );
	int fd0, fd;

	if( gt == NULL )
		return -1;

	arg_fname = args;
	arg_fname_len = gt - args;
	length -= gt - args;

	trim( &arg_fname, &arg_fname_len );

	gt++; length--;
	trim( &gt, &length );

	//if( length <= 0 || *gt != '#' )
	//	return -1;


	//gt++;
	//length--;

	sscanf( gt, "%d", &fd );
	fd += 100;

	char filename[255];
	memcpy( filename, arg_fname, arg_fname_len );
	filename[arg_fname_len] = '\0';

	fd0 = open( filename, O_WRONLY );
	if( fd0 < 0 )
		return fd0;

	dup2( fd0, fd );
	close( fd0 );

	return 0;
}

static int nanosleep0(char* s, size_t length) {
	struct timespec req;


	sscanf( s, "%ld\n", &req.tv_nsec );
	req.tv_sec = req.tv_nsec / 1000000000;
	req.tv_nsec %= 1000000000;

	return nanosleep( &req, NULL );
}


static int do_line(char* s, size_t length) {
	if( length == 0 )
		return 0;

	if( length >= 5 && memcmp( s, "echo ", 5 ) == 0 ) {
		return echo0( s + 5, length - 5 );
	}

	if( length >= 5 && memcmp( s, "open ", 5 ) == 0 ) {
		return open0( s + 5, length - 5 );
	}

	if( length >= 10 && memcmp( s, "nanosleep ", 10 ) == 0 ) {
		return nanosleep0( s + 10, length - 10 );
	}

	return -1;
}

int main(int argc, char* argv[]) {
	char buffer[BUF_SIZE];
	char *s;
	size_t length;
	int line;

	s = buffer;
	length = 0;
	line = 0;

	do {
		
		char* nl = (char*) memchr( s, '\n', length );
		if( nl == NULL ) {
			// no newline found; get more
			ssize_t len1;

			if( length == sizeof( buffer ) ) {
				fprintf( stdout, "Line %d too long: more than %zu characters\n", line + 1, sizeof( buffer ) );

				return -1;
			}

			s = memmove( buffer, s, length );
			len1 = read( 0, s + length, sizeof( buffer ) - length );
			length += len1;

			if( len1 == 0 )
				break;

			continue;
		}

		line++;

		if( do_line( s, nl - s ) ) {
			fprintf( stdout, "Line %d error\n", line );
			return -1;
		}

		length -= nl - s + 1;
		s = nl + 1;
	} while( 1 );

	if( do_line( s, length ) ) {
		fprintf( stdout, "Line %d error\n", line );
		return -1;
	}

	return 0;
}
