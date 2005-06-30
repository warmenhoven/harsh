#include "main.h"
#include "xml.h"

void
rss_parse(struct feed *feed, void *xml_tree)
{
	const char *version;

	if (strcmp(xml_name(xml_tree), "rss") != 0) {
		feed->status = FEED_ERR_RSS;
		return;
	}

	version = xml_get_attrib(xml_tree, "version");
	if (strcmp(version, "2.0") != 0) {
		feed->status = FEED_ERR_RSS;
		return;
	}
}
