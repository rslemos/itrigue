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
#include <jansson.h>
#include <microhttpd.h>

#include <execinfo.h>
#include <sys/queue.h>

#define DEEPEST_BACKTRACE 20

struct error_entry {
	TAILQ_ENTRY(error_entry) list;

	void* backtrace[DEEPEST_BACKTRACE];
	int backtrace_size;

	char formatted_message[0];
};

#define ROUND_TO_MULTIPLE(to_round, multiple) ((to_round + multiple - 1) & ~(multiple - 1))
#define ERROR_ENTRY_SIZE ROUND_TO_MULTIPLE(sizeof(struct error_entry), 4096)
#define ERROR_MESSAGE_SIZE (ERROR_ENTRY_SIZE - sizeof(struct error_entry))

static TAILQ_HEAD(error_entry_head, error_entry) errors;

static void __attribute__((constructor))
init_httpd (void)
{
	TAILQ_INIT(&errors);
}

void
error (const char *fmt,...)
{

	struct error_entry *new = malloc(ERROR_ENTRY_SIZE);

	va_list va;

	va_start(va, fmt);
	vsnprintf(new->formatted_message, ERROR_MESSAGE_SIZE, fmt, va);
	va_end(va);

	new->backtrace_size = backtrace(new->backtrace, sizeof(new->backtrace));

	TAILQ_INSERT_TAIL(&errors, new, list);
}


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

static char*
error_page (void)
{
	int page_size = 4096 * 16;
	struct error_entry *entry;
	struct error_entry copy;

	char* page = malloc(page_size);

	page[0] = '\0';
	strcat(page, "<html><body><dl class=\"errors\">\n");

	TAILQ_FOREACH(entry, &errors, list) {
		int i;
		char **symbols;
		char addresses[DEEPEST_BACKTRACE * (2 + 64 / 4 + 1)];
		char *p;


		strcat(page, "<dt class=\"error message\">");
		strcat(page, entry->formatted_message);
		strcat(page, "</dt>\n");

		p = addresses;
		for (i = 1; i < entry->backtrace_size; i++) {
			p += snprintf(p, sizeof(addresses) - (p - addresses), "%p ", entry->backtrace[i]);
		}

		strcat(page, "<dd class=\"error stack\">\n<p>Stack dump:</p>\n<pre class=\"error stack addresses\">");
		strcat(page, addresses);
		strcat(page, "</pre>\n");
		strcat(page, "<p>addr2line output:</p>\n<pre class=\"error stack addr2line\">\n");

		{
			int fd[2];
			int pid;

			pipe(fd);
			if (!(pid = vfork())) {
				char *argv[3 + entry->backtrace_size - 1 + 1];

				Dl_info info;

				close(0);
				close(1);
				close(2);

				dup2(fd[1], 1);
				dup2(fd[1], 2);
				
				close(fd[1]);

				dladdr(error_page, &info);

				memset(argv, 0, sizeof(argv));

				argv[0] = "/usr/bin/addr2line";
				argv[1] = "-fapie";
				argv[2] = (char*)info.dli_fname;

				p = addresses;
				for (i = 1; i < entry->backtrace_size; i++) {
					argv[2 + i] = p;
					while (*p != ' ' && *p != '\0') p++;
					*p++ = '\0';
				}
				
				execv(argv[0], argv);
			} else {
				char buffer[512];
				int r;

				close(fd[1]);

				while ((r = read(fd[0], buffer, 512)) != 0) {
					strncat(page, buffer, r);
				}

				close(fd[0]);
				waitpid(pid, NULL, WNOHANG);
			}
		}

		strcat(page, "\n</pre>\n");
		strcat(page, "<p>Symbols:</p>\n<ol>\n");

		symbols = backtrace_symbols(entry->backtrace, entry->backtrace_size);

		for (i = 1; i < entry->backtrace_size; i++) {
			strcat(page, "<li>");
			strcat(page, symbols[i]);
			strcat(page, "</li>");
		}

		strcat(page, "</ol>\n");
		
		strcat(page, "</dd>\n");

		TAILQ_REMOVE(&errors, entry, list);

		copy = *entry;
		free(entry);

		entry = &copy;
	}

	strcat(page, "</dl></body></html>\n");

	return page;
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
