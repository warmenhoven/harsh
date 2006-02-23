#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <expat.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "xml.h"

typedef struct _xmlnode {
	struct _xmlnode *parent;
	list *children;
	list *attribs;
	char *name;
	char *data;
} xmlnode;

void *
xml_new(const char *el)
{
	xmlnode *node = calloc(1, sizeof (xmlnode));
	if (!node)
		return (NULL);
	node->name = strdup(el);
	return (node);
}

void *
xml_child(void *p, const char *el)
{
	xmlnode *parent = p, *node;

	if (!parent)
		return (NULL);

	node = calloc(1, sizeof (xmlnode));
	if (!node)
		return (NULL);
	node->name = strdup(el);
	node->parent = parent;

	parent->children = list_append(parent->children, node);

	return (node);
}

void
xml_attrib(void *n, const char *attr, const char *val)
{
	xmlnode *node = n, *attrib;

	if (!node)
		return;

	attrib = calloc(1, sizeof (xmlnode));
	if (!attrib)
		return;
	attrib->name = strdup(attr);
	attrib->data = strdup(val);

	node->attribs = list_append(node->attribs, attrib);
}

void
xml_data(void *n, const char *data, int len)
{
	xmlnode *node = n;
	int totlen;

	if (!node)
		return;
	if (!len)
		return;
	if (!data)
		return;

	if (len == -1)
		len = strlen(data);

	totlen = len;

	if (node->data)
		totlen += strlen(node->data);

	node->data = realloc(node->data, totlen + 1);
	if (totlen == len) {
		strncpy(node->data, data, len);
	} else {
		strncat(node->data, data, len);
	}
	node->data[totlen] = 0;
}

void *
xml_parent(void *child)
{
	xmlnode *node = child;
	if (!node)
		return (NULL);

	return (node->parent);
}

char *
xml_name(void *n)
{
	xmlnode *node = n;
	if (!node)
		return (NULL);

	return (node->name);
}

void *
xml_get_child(void *n, const char *name)
{
	xmlnode *node = n;
	list *l;

	if (!n || !name)
		return (NULL);

	l = node->children;
	while (l) {
		xmlnode *child = l->data;
		if (!strcasecmp(child->name, name))
			return (child);
		l = l->next;
	}
	return (NULL);
}

list *
xml_get_children(void *p)
{
	xmlnode *parent = p;
	if (!parent)
		return (NULL);
	return (parent->children);
}

char *
xml_get_attrib(void *n, const char *name)
{
	xmlnode *node = n;
	list *l;

	if (!n || !name)
		return (NULL);

	l = node->attribs;
	while (l) {
		xmlnode *attrib = l->data;
		if (!strcasecmp(attrib->name, name))
			return (attrib->data);
		l = l->next;
	}
	return (NULL);
}

char *
xml_get_data(void *n)
{
	if (!n) return NULL;
	return (((xmlnode *)n)->data);
}

void
xml_free(void *n)
{
	xmlnode *node = n;
	if (!node)
		return;

	while (node->attribs) {
		xmlnode *attrib = node->attribs->data;
		node->attribs = list_remove(node->attribs, attrib);
		xml_free(attrib);
	}

	while (node->children) {
		xmlnode *child = node->children->data;
		node->children = list_remove(node->children, child);
		xml_free(child);
	}

	if (node->name)
		free(node->name);
	if (node->data)
		free(node->data);

	free(node);
}

static void
xml_start(void *data, const char *el, const char **attr)
{
	void **node = (void **)data;
	int i;

	if (*node)
		*node = xml_child(*node, el);
	else
		*node = xml_new(el);

	for (i = 0; attr[i]; i += 2)
		xml_attrib(*node, attr[i], attr[i + 1]);
}

static void
xml_end(void *data, const char *el)
{
	void **node = (void **)data;

	if (!*node)
		return;

	if (!xml_parent(*node))
		return;
	else if (strcmp(xml_name(*node), el) == 0)
		*node = xml_parent(*node);
}

static void
xml_chardata(void *data, const char *s, int len)
{
	void **node = (void **)data;

	xml_data(*node, s, len);
}

void *
xml_parse(const char *data, int len)
{
	XML_Parser parser;
	void *ret = NULL;

	parser = XML_ParserCreate(NULL);
	if (!parser)
		return (NULL);

	XML_SetUserData(parser, &ret);
	XML_SetElementHandler(parser, xml_start, xml_end);
	XML_SetCharacterDataHandler(parser, xml_chardata);

	XML_Parse(parser, data, len, 0);

	XML_ParserFree(parser);

	while (xml_parent(ret))
		ret = xml_parent(ret);

	return (ret);
}

static char *
xml_enc(const char *str)
{
	const char *x = str;
	char *ret = NULL, *y;
	int len = strlen(str);

	if (ret)
		free(ret);

	y = ret = malloc(len * 6);

	while (*x) {
		switch (*x) {
		case '"':
			*y++ = '&';
			*y++ = 'q';
			*y++ = 'u';
			*y++ = 'o';
			*y++ = 't';
			*y++ = ';';
			break;
		case '<':
			*y++ = '&';
			*y++ = 'l';
			*y++ = 't';
			*y++ = ';';
			break;
		case '>':
			*y++ = '&';
			*y++ = 'g';
			*y++ = 't';
			*y++ = ';';
			break;
		case '&':
			*y++ = '&';
			*y++ = 'a';
			*y++ = 'm';
			*y++ = 'p';
			*y++ = ';';
			break;
		default:
			*y++ = *x;
		}
		x++;
	}
	*y = 0;

	return (ret);
}

void
xml_print(FILE *f, void *n)
{
	xmlnode *node = n;
	list *l;

	fprintf(f, "<%s", xml_name(node));

	l = node->attribs;
	while (l) {
		xmlnode *attrib = l->data;
		l = l->next;
		fprintf(f, " %s=\"%s\"", attrib->name, xml_enc(attrib->data));
	}

	fprintf(f, ">");

	if (!node->data)
		fprintf(f, "\n");

	l = node->children;
	while (l) {
		xml_print(f, l->data);
		l = l->next;
	}

	/* XXX does not ensure data is cdata-safe! */
	if (node->data)
		fprintf(f, "%s", xml_enc(node->data));

	fprintf(f, "</%s>\n", node->name);
}
