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

#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <microhttpd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern void error(const char *fmt,...);

extern char* error_page(void);
extern json_t* get_cards(void);

static int
file_handler (void *cls, struct MHD_Connection *connection,
	      const char *url,
	      const char *method, const char *version,
	      const char *upload_data,
	      size_t *upload_data_size, void **con_cls)
{
	int fd;
	struct stat file_stat;

	struct MHD_Response *response;
	int ret;

	if ((fd = open(url + 1, O_RDONLY)) < 0) {
		error("Error opening file %s: %s\n", url + 1, strerror(errno));
		return MHD_NO;
	}
	
	fstat(fd, &file_stat);

	response = MHD_create_response_from_fd (file_stat.st_size, fd);

	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);

	return ret;
}

static int
json_handler (void *cls, struct MHD_Connection *connection,
	      const char *url,
	      const char *method, const char *version,
	      const char *upload_data,
	      size_t *upload_data_size, void **con_cls)
{
	char *page;
	struct MHD_Response *response;
	int ret;
	json_t *cards = get_cards();

	if (cards == NULL) {
		page = error_page();
	} else {
		page = json_dumps(cards, JSON_INDENT(2) | JSON_PRESERVE_ORDER | JSON_ESCAPE_SLASH);
		json_decref(cards);
	}

	response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_MUST_FREE);

	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);

	return ret;
}

static const struct mapping {
	const char *prefix;
	MHD_AccessHandlerCallback handler;
}
mappings[] = {
	{ "/json", json_handler },
	{ "/", file_handler },
};


static int
handler (void *cls, struct MHD_Connection *connection,
	 const char *url,
	 const char *method, const char *version,
	 const char *upload_data,
	 size_t *upload_data_size, void **con_cls)
{
	int i;

	for (i = 0; i < sizeof(mappings)/sizeof(mappings[0]); i++) {
		if (!strncmp(mappings[i].prefix, url, strlen(mappings[i].prefix)))
			return mappings[i].handler(
				cls, connection, url, method, version, upload_data, upload_data_size, con_cls);
	}

	return MHD_NO;
}

static void*
log_uri (void *cls, const char *uri, struct MHD_Connection *con)
{
	struct sockaddr **addr = (struct sockaddr **)MHD_get_connection_info (con, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	fprintf(stderr, "URI: %s", uri);
	if ((*addr)->sa_family == AF_INET) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *) *addr;
		fprintf(stderr, " [%s]\n", inet_ntoa(addr_in->sin_addr));
	} else {
		fprintf(stderr, " [non AF_INET]\n");
	}

	return NULL;
}

#define PORT 8888

int
run_server (void)
{
	struct MHD_Daemon *daemon;
	daemon = MHD_start_daemon (MHD_USE_DEBUG, PORT, NULL, NULL,
		             &handler, NULL, MHD_OPTION_URI_LOG_CALLBACK, log_uri, NULL, MHD_OPTION_END);

	if (NULL == daemon)
		return 1;

	while(1) {
		fd_set readfds, writefds, exceptfds;
		int maxfd = 0;
		struct timeval timeout;
		unsigned MHD_LONG_LONG mhd_timeout;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		MHD_get_fdset (daemon, &readfds, &writefds, &exceptfds, &maxfd);
		MHD_get_timeout (daemon, &mhd_timeout);
		timeout.tv_sec = mhd_timeout / 1000;
		timeout.tv_usec = (mhd_timeout - (timeout.tv_sec * 1000)) * 1000;

		select(maxfd + 1, &readfds, &writefds, &exceptfds, &timeout);
		MHD_run_from_select(daemon, &readfds, &writefds, &exceptfds);
	}

	// provide means to reach here (perhaps SIGHUP)
	MHD_stop_daemon (daemon);

	return 0;
}
