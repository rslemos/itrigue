
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <execinfo.h>
#include <dlfcn.h>

#define DEEPEST_BACKTRACE 20

struct error_entry {
	TAILQ_ENTRY(error_entry) list;

	void* backtrace[DEEPEST_BACKTRACE];
	int backtrace_size;

	char formatted_message[0];
};

static TAILQ_HEAD(error_entry_head, error_entry) errors;

static void __attribute__((constructor))
init_httpd (void)
{
	TAILQ_INIT(&errors);
}

#define ROUND_TO_MULTIPLE(to_round, multiple) ((to_round + multiple - 1) & ~(multiple - 1))
#define ERROR_ENTRY_SIZE ROUND_TO_MULTIPLE(sizeof(struct error_entry), 4096)
#define ERROR_MESSAGE_SIZE (ERROR_ENTRY_SIZE - sizeof(struct error_entry))


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

static inline void
error_list_remove_entry (struct error_entry **entry)
{
	static struct error_entry copy;

	TAILQ_REMOVE(&errors, *entry, list);

	copy = **entry;
	free(*entry);
	*entry = &copy;
}

void
error_list_clear (void)
{
	struct error_entry *entry;

	TAILQ_FOREACH(entry, &errors, list) {
		error_list_remove_entry(&entry);
	}
}

static void
formatted_message (void* opaque, const char* message)
{
	char* page = (char*)opaque;

	strcat(page, "<dt class=\"error message\">");
	strcat(page, message);
	strcat(page, "</dt>\n");
}

static void
addresses (void* opaque, void** backtrace, int size)
{
	char* page = (char*)opaque;

	int i;
	char addresses[DEEPEST_BACKTRACE * (2 + 64 / 4 + 1)];
	char *p;

	p = addresses;
	for (i = 0; i < size; i++) {
		p += snprintf(p, sizeof(addresses) - (p - addresses), "%p ", backtrace[i]);
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
			char *argv[3 + size + 1];

			Dl_info info;

			dladdr(error, &info);

			memset(argv, 0, sizeof(argv));

			argv[0] = "/usr/bin/addr2line";
			argv[1] = "-fapie";
			argv[2] = (char*)info.dli_fname;
			
			printf("argv2 = %s\n", argv[2]);
			close(0);
			close(1);
			close(2);

			dup2(fd[1], 1);
			dup2(fd[1], 2);
			
			close(fd[1]);

			p = addresses;
			for (i = 0; i < size; i++) {
				argv[3 + i] = p;
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
}

static void
symbols (void* opaque, char** symbols, int size)
{
	char* page = (char*)opaque;

	int i;

	strcat(page, "<p>Symbols:</p>\n<ol>\n");

	for (i = 0; i < size; i++) {
		strcat(page, "<li>");
		strcat(page, symbols[i]);
		strcat(page, "</li>");
	}

	strcat(page, "</ol>\n");
	
	strcat(page, "</dd>\n");
}

char*
error_page (void)
{
	int page_size = 4096 * 16;
	struct error_entry *entry;

	char* page = malloc(page_size);

	page[0] = '\0';
	strcat(page, "<html><body><dl class=\"errors\">\n");

	TAILQ_FOREACH(entry, &errors, list) {
		char **sym;

		formatted_message(page, entry->formatted_message);
		addresses(page, entry->backtrace + 1, entry->backtrace_size - 1);

		sym = backtrace_symbols(entry->backtrace + 1, entry->backtrace_size - 1);
		symbols(page, sym, entry->backtrace_size - 1);
		free(sym);

		error_list_remove_entry(&entry);
	}

	strcat(page, "</dl></body></html>\n");

	return page;
}


