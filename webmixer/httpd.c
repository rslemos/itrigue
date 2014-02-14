#include <string.h>
#include <jansson.h>
#include <microhttpd.h>

extern json_t* get_cards(void);

static int
file_handler (void *cls, struct MHD_Connection *connection,
	      const char *url,
	      const char *method, const char *version,
	      const char *upload_data,
	      size_t *upload_data_size, void **con_cls)
{
	const char *page = "<html><body>This is a file</body></html>\n";
	struct MHD_Response *response;
	int ret;

	response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_MUST_FREE);

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

	page = json_dumps(cards, JSON_INDENT(2) | JSON_PRESERVE_ORDER | JSON_ESCAPE_SLASH);
	json_decref(cards);

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
	{ "/file", file_handler },
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

#define PORT 8888

int
run_server (void)
{
	struct MHD_Daemon *daemon;
	daemon = MHD_start_daemon (MHD_USE_DEBUG, PORT, NULL, NULL,
		             &handler, NULL, MHD_OPTION_END);

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
