#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "xml.h"

static void
free_item(struct feed *feed, struct item *item)
{
	if (feed && feed->items)
		feed->items = list_remove(feed->items, item);
	free(item->guid);
	free(item->title);
	free(item->link);
	free(item->desc);
	free(item);
}

static void
free_items(struct feed *feed)
{
	while (feed->items)
		free_item(feed, feed->items->data);
}

void
rss_parse(struct feed *feed, void *xml_tree)
{
	list *children;
	void *channel;
	void *child;
	int vers = 2;	/* also 0.9x */

	/* by default we assume there's a problem with the feed. */
	feed->status = FEED_ERR_RSS;

	if (strcmp(xml_name(xml_tree), "rdf:RDF") == 0) {
		vers = 1;
	} else if (strcmp(xml_name(xml_tree), "rss") != 0) {
		return;
	}

	channel = xml_get_child(xml_tree, "channel");
	if (!channel)
		return;

	child = xml_get_child(channel, "title");
	if (!child || !xml_get_data(child))
		return;
	if (feed->title)
		free(feed->title);
	feed->title = strdup(xml_get_data(child));

	child = xml_get_child(channel, "link");
	if (!child || !xml_get_data(child))
		return;
	if (feed->link)
		free(feed->link);
	feed->link = strdup(xml_get_data(child));

	child = xml_get_child(channel, "description");
	if (!child || !xml_get_data(child))
		return;
	if (feed->desc)
		free(feed->desc);
	feed->desc = strdup(xml_get_data(child));

	free_items(feed);

	if (vers == 2)
		children = xml_get_children(channel);
	else
		children = xml_get_children(xml_tree);

	while (children) {
		struct item *item;
		void *xml_item = children->data;
		children = children->next;

		if (strcmp(xml_name(xml_item), "item"))
			continue;

		item = calloc(1, sizeof (struct item));

		child = xml_get_child(xml_item, "title");
		if (child && xml_get_data(child)) {
			item->title = strdup(xml_get_data(child));
		}

		child = xml_get_child(xml_item, "link");
		if (child && xml_get_data(child)) {
			item->link = strdup(xml_get_data(child));
		}

		child = xml_get_child(xml_item, "description");
		if (child && xml_get_data(child)) {
			item->desc = strdup(xml_get_data(child));
		}

		child = xml_get_child(xml_item, "guid");
		if (child && xml_get_data(child)) {
			item->guid = strdup(xml_get_data(child));
			char *perm = xml_get_attrib(child, "isPermaLink");
			if (!perm || strcmp(perm, "true") == 0)
				item->isPermaLink = 1;
		}

		if (!item->title && !item->desc) {
			free_item(NULL, item);
			continue;
		}

		feed->items = list_append(feed->items, item);
	}

	feed->status = FEED_ERR_NONE;
}
