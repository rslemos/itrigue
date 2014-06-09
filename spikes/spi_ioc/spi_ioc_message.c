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
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <fcntl.h>


#include <linux/spi/spidev.h>

#define ASSERT(assertion, msg)		\
	do {							\
		if( !(assertion) ) {		\
			perror( msg );			\
			abort();				\
		}							\
	} while( 0 )


static struct option long_options[] = {
	{ name: "bits-per-word", has_arg: 1, flag: NULL, val: 'w' },
	{ name: "speed",         has_arg: 1, flag: NULL, val: 's' },
	{ name: "data-hex",      has_arg: 1, flag: NULL, val: 'x' },
	{ name: "data-bin",      has_arg: 1, flag: NULL, val: 'b' },
	{ name: "data-dec",      has_arg: 1, flag: NULL, val: 'd' },
	{ name: "delay",         has_arg: 1, flag: NULL, val: 'y' },
	{ name: "mode",          has_arg: 1, flag: NULL, val: 'm' },
	{ name: NULL,            has_arg: 0, flag: NULL, val: 0   }
};

static void parse_hex(char* s, void** buf, __u32* len) {
	int alloced = 0;
	int nibblesleft = 0;
	*len = 0;

	*buf = NULL;

	while( *s != '\0' ) {
		int nibble;

		if( *s >= '0' && *s <= '9' )
			nibble = *s - '0';
		else if( *s >= 'a' && *s <= 'f' )
			nibble = *s - 'a' + 10;
		else if( *s >= 'A' && *s <= 'F' )
			nibble = *s - 'A' + 10;
		else if( *s == ',' || *s == ' ' || *s == '.' ) {
			nibblesleft = 0;
			s++;
			continue;
		} else {
			fprintf( stdout, "Invalid input: %c\n", *s );
			abort();
		}
		
		s++;


		if( nibblesleft == 0 ) {
			if( *len == alloced ) {
				void* tmp = *buf;
				
				alloced *= 2;
				alloced++;
	
				*buf = malloc( alloced );
				memset( *buf, 0, alloced );

				if( tmp ) {
					memcpy( *buf, tmp, *len );
					free( tmp );
				}
			}

			(*len)++;
			nibblesleft = 2;
		}

		nibblesleft--;

		*(unsigned char *)(*buf + *len - 1) <<= 4;
		*(unsigned char *)(*buf + *len - 1) |= nibble;
	}

}

static void parse_bin(char* s, void** buf, __u32* len) {
	int alloced = 0;
	int bitsleft = 0;
	*len = 0;

	*buf = NULL;

	while( *s != '\0' ) {
		int bit;

		if( *s == '0' )
			bit = 0;
		else if( *s == '1' )
			bit = 1;
		else if( *s == ',' || *s == ' ' || *s == '.' ) {
			bitsleft = 0;
			s++;
			continue;
		} else {
			fprintf( stdout, "Invalid input: %c\n", *s );
			abort();
		}
		
		s++;


		if( bitsleft == 0 ) {
			if( *len == alloced ) {
				void* tmp = *buf;
				
				alloced *= 2;
				alloced++;
	
				*buf = malloc( alloced );
				memset( *buf, 0, alloced );

				if( tmp ) {
					memcpy( *buf, tmp, *len );
					free( tmp );
				}
			}

			(*len)++;
			bitsleft = 8;
		}

		bitsleft--;

		*(unsigned char *)(*buf + *len - 1) <<= 1;
		*(unsigned char *)(*buf + *len - 1) |= bit;
	}

}

static void parse_dec(char* s, void** buf, __u32* len) {
	int alloced = 0;
	int left = 0;
	*len = 0;

	*buf = NULL;

	while( *s != '\0' ) {
		int digit;

		if( *s >= '0' && *s <= '9' )
			digit = *s - '0';
		else if( *s == ',' || *s == ' ' || *s == '.' ) {
			left = 0;
			s++;
			continue;
		} else {
			fprintf( stdout, "Invalid input: %c\n", *s );
			abort();
		}
		
		s++;


		if( left == 0 ) {
			if( *len == alloced ) {
				void* tmp = *buf;
				
				alloced *= 2;
				alloced++;
	
				*buf = malloc( alloced );
				memset( *buf, 0, alloced );

				if( tmp ) {
					memcpy( *buf, tmp, *len );
					free( tmp );
				}
			}

			(*len)++;
			left = 1;
		}

		*(unsigned char *)(*buf + *len - 1) *= 10;
		*(unsigned char *)(*buf + *len - 1) += digit;
	}
}

int main(int argc, char* argv[]) {
	int c;
	char* name;
	int fd;
	int ret;
	unsigned char mode = 0xff;

	struct spi_ioc_transfer tr;

	memset( &tr, 0, sizeof( tr ) );

	while( ( c = getopt_long( argc, argv, "w:s:x:b:d:y:m:", long_options, NULL ) ) != EOF ) {
		switch( c ) {
		case 'w':
			tr.bits_per_word = atoi( optarg );
			continue;
		case 's':
			tr.speed_hz = atoi( optarg );
			continue;
		case 'x':
			if( tr.tx_buf ) free( (void*) tr.tx_buf );

			parse_hex( optarg, (void*) &tr.tx_buf, &tr.len );

			continue;
		case 'b':
			if( tr.tx_buf ) free( (void*)tr.tx_buf );

			parse_bin( optarg, (void*) &tr.tx_buf, &tr.len );

			continue;
		case 'd':
			if( tr.tx_buf ) free( (void*)tr.tx_buf );

			parse_dec( optarg, (void*) &tr.tx_buf, &tr.len );

			continue;
		case 'y':
			tr.delay_usecs = atoi( optarg );
			continue;
		case 'm':
			mode = atoi( optarg );
			continue;
		}
	}

	if( ( optind + 1 ) != argc ) {
		fprintf( stdout, "You need to specify spidev device\n" );
		abort();
	}

	name = argv[optind];
	fd = open( name, O_RDWR );
	ASSERT( fd >= 0, "open" );

	if( mode < 4 ) {
		ret = ioctl( fd, SPI_IOC_WR_MODE, &mode );
		ASSERT( ret >= 0, "SPI_IOC_WR_MODE" );
	}

	ret = ioctl( fd, SPI_IOC_MESSAGE(1), &tr );
	ASSERT( ret >= 0, "SPI_IOC_MESSAGE" );

	return 0;
}

