#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "main.h"
#include "xml.h"

#define CFG_VER "1"

static int
parse_config(void *xml_tree)
{
	char *version;
	list *f;

	if (strcmp(xml_name(xml_tree), PROG) != 0) {
		fprintf(stderr, "incorrect name '%s', expect '%s'!\n", xml_name(xml_tree), PROG);
		return (1);
	}

	version = xml_get_attrib(xml_tree, "version");
	if (!version || strcmp(version, CFG_VER) != 0) {
		fprintf(stderr, "unknown version!\n");
		return (1);
	}

	f = xml_get_children(xml_tree);

	while (f) {
		void *xml_feed = f->data;
		struct feed *feed;
		void *child;
		int interval;
		list *g;

		f = f->next;

		if (strcmp(xml_name(xml_feed), "feed") != 0)
			continue;

		child = xml_get_child(xml_feed, "interval");
		if (!child || !xml_get_data(child))
			continue;
		interval = strtoul(xml_get_data(child), NULL, 10);
		if (!interval)
			interval = 24;

		child = xml_get_child(xml_feed, "url");
		if (!child || !xml_get_data(child))
			continue;

		/* XXX should probably strip whitespace */
		feed = feed_add(xml_get_data(child), interval);

		g = xml_get_children(xml_feed);
		while (g) {
			void *guid = g->data;
			g = g->next;

			if (strcmp(xml_name(guid), "guid") != 0)
				continue;
			if (!xml_get_data(guid))
				continue;

			feed->read_guids = list_append(feed->read_guids,
										   strdup(xml_get_data(guid)));
		}
	}

	return (0);
}

int
read_config()
{
	struct stat sb;
	char path[1024];

	sprintf(path, "%s/.%s", getenv("HOME"), PROG);
	if (stat(path, &sb))
		mkdir(path, 0700);
	else if (!S_ISDIR(sb.st_mode)) {
		unlink(path);
		mkdir(path, 0700);
	}

	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (stat(path, &sb))
		return (0);
	else if (!S_ISREG(sb.st_mode)) {
		unlink(path);
		return (0);
	} else {
		FILE *f = fopen(path, "r");
		void *xml_tree;
		char *data;
		int rc;

		if (!f) {
			unlink(path);
			return (0);
		}

		data = malloc(sb.st_size);
		rc = fread(data, 1, sb.st_size, f);
		fclose(f);

		if (rc != sb.st_size) {
			free(data);
			fprintf(stderr, "bad read of config file\n");
			return (1);
		}

		xml_tree = xml_parse(data, sb.st_size);
		free(data);

		if (!xml_tree) {
			fprintf(stderr, "error parsing xml\n");
			return (1);
		}

		rc = parse_config(xml_tree);
		xml_free(xml_tree);

		if (rc) {
			fprintf(stderr, "error parsing data\n");
			return (1);
		}
	}

	return (0);
}

int
save_config()
{
	list *l = feeds;
	FILE *f = NULL;
	char path[256];
	void *node;

	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (!(f = fopen(path, "w"))) {
		return (1);
	}

	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");

	node = xml_new(PROG);
	xml_attrib(node, "version", CFG_VER);

	while (l) {
		void *xml_feed;
		void *child;
		char str[10];

		list *g;

		struct feed *feed = l->data;
		l = l->next;

		xml_feed = xml_child(node, "feed");

		child = xml_child(xml_feed, "url");
		xml_data(child, feed->url, -1);

		child = xml_child(xml_feed, "interval");
		snprintf(str, sizeof (str), "%d", feed->interval);
		xml_data(child, str, -1);

		g = feed->read_guids;
		while (g) {
			child = xml_child(xml_feed, "guid");
			xml_data(child, g->data, -1);
			g = g->next;
		}
	}

	xml_print(f, node);

	fclose(f);

	return (0);
}
