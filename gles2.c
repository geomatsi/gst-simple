/*
 * Copyright (C) 2008-2010 Felipe Contreras
 * Copyright (C) 2012 matsi
 *
 * Originated from gst-omapfb by Felipe Contreras <felipe.contreras@gmail.com>
 *
 * This file may be used under the terms of the GNU Lesser General Public
 * License version 2.1, a copy of which is found in LICENSE included in the
 * packaging of this file.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include "gles2util.h"
#include "gles2.h"

/* */

GLuint *img_ptr;
GLuint img_h;
GLuint img_w;

sem_t gles2_sem;
bool glt_active;
pthread_t glt;

/* */

static void *parent_class;

struct gst_gles2_sink {
	GstBaseSink parent;
};

struct gst_gles2_sink_class {
	GstBaseSinkClass parent_class;
};

/* */

static GstCaps * generate_sink_template(void)
{
	GstStructure *struc;
	GstCaps *caps;

	fprintf(stderr, "--> %s\n", __func__);

	caps = gst_caps_new_empty();

	struc = gst_structure_new("video/x-raw-rgb",
			"width", GST_TYPE_INT_RANGE, 16, 4096,
			"height", GST_TYPE_INT_RANGE, 16, 4096,
			"bpp", G_TYPE_INT, 32,
			"depth", G_TYPE_INT, 24,
			"endianness", G_TYPE_INT, 4321,
			"green_mask", G_TYPE_INT, 16711680,
			"blue_mask", G_TYPE_INT, 65280,
			"red_mask", G_TYPE_INT, -16777216,
			"framerate", GST_TYPE_FRACTION_RANGE, 0, 1, 30, 1,
			NULL);

	gst_caps_append_structure(caps, struc);

	return caps;
}

static gboolean setup(struct gst_gles2_sink *self, GstCaps *caps)
{
	GstStructure *structure;
	int bufsize, width, height;

	fprintf(stderr, "--> %s\n", __func__);

	structure = gst_caps_get_structure(caps, 0);

	gst_structure_get_int(structure, "width", &width);
	gst_structure_get_int(structure, "height", &height);

	img_h = height;
	img_w = width;

	bufsize = img_h * img_w * 4;

	if (posix_memalign((void **) &img_ptr, 4, bufsize)) {
		fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
		return false;
	}

    sem_init(&gles2_sem, 0, 1);
	glt_active = true;

    pthread_create(&glt, NULL, gles2_thread, NULL);

	return true;
}

static gboolean setcaps(GstBaseSink *base, GstCaps *caps)
{
	struct gst_gles2_sink *self = (struct gst_gles2_sink *)base;

	return setup(self, caps);
}

static gboolean start(GstBaseSink *base)
{
	struct gst_gles2_sink *self = (struct gst_gles2_sink *)base;

	fprintf(stderr, "--> %s\n", __func__);

	if (0 > gles2_start()) {
		return false;
	}

	return true;
}

static gboolean stop(GstBaseSink *base)
{
	struct gst_gles2_sink *self = (struct gst_gles2_sink *)base;

	fprintf(stderr, "--> %s\n", __func__);

	return true;
}

static GstFlowReturn render(GstBaseSink *base, GstBuffer *buffer)
{
	struct gst_gles2_sink *self = (struct gst_gles2_sink *)base;

	fprintf(stderr, "--> %d\n", GST_BUFFER_SIZE(buffer));

	sem_wait(&gles2_sem);
	memcpy(img_ptr, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
    sem_post(&gles2_sem);

	if (!glt_active)
		return GST_FLOW_UNEXPECTED;

	return GST_FLOW_OK;
}

static void class_init(void *g_class, void *class_data)
{
	GstBaseSinkClass *base_sink_class;

	fprintf(stderr, "--> %s\n", __func__);

	base_sink_class = g_class;

	parent_class = g_type_class_ref(GST_GLES2_SINK_TYPE);

	base_sink_class->set_caps = setcaps;
	base_sink_class->start = start;
	base_sink_class->stop = stop;
	base_sink_class->render = render;
	base_sink_class->preroll = render;
}

static void base_init(void *g_class)
{
	GstElementClass *element_class = g_class;
	GstPadTemplate *template;

	fprintf(stderr, "--> %s\n", __func__);

	gst_element_class_set_details_simple(element_class,
			"Experimental GLES2 plugin",
			"Sink/Video",
			"Renders video with gles2",
			"matsi");

	template = gst_pad_template_new("sink", GST_PAD_SINK,
			GST_PAD_ALWAYS,
			generate_sink_template());

	gst_element_class_add_pad_template(element_class, template);
	gst_object_unref(template);
}

GType gst_gles2_sink_get_type(void)
{
	static GType type;

	fprintf(stderr, "--> %s\n", __func__);

	if (G_UNLIKELY(type == 0)) {
		GTypeInfo type_info = {
			.class_size = sizeof(struct gst_gles2_sink_class),
			.class_init = class_init,
			.base_init = base_init,
			.instance_size = sizeof(struct gst_gles2_sink),
		};

		type = g_type_register_static(GST_TYPE_BASE_SINK, "GstGles2Sink", &type_info, 0);
	}

	return type;
}

static gboolean plugin_init(GstPlugin *plugin)
{
	fprintf(stderr, "--> %s\n", __func__);

	if (!gst_element_register(plugin, "gles2sink", GST_RANK_SECONDARY, GST_GLES2_SINK_TYPE))
		return false;

	return true;
}

GstPluginDesc gst_plugin_desc = {
	.major_version = GST_VERSION_MAJOR,
	.minor_version = GST_VERSION_MINOR,
	.name = "gles2",
	.description = (gchar *) "Experimental GLES2 plugin",
	.plugin_init = plugin_init,
	.version = VERSION,
	.license = "LGPL",
	.source = "source",
	.package = "package",
	.origin = "origin",
};
