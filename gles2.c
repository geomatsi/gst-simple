/*
 * Copyright (C) 2008-2010 Felipe Contreras
 *
 * Author: Felipe Contreras <felipe.contreras@gmail.com>
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

#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>

#include "xutil.h"
#include "gles2.h"

#define ROUND_UP(num, scale) (((num) + ((scale) - 1)) & ~((scale) - 1))

/* */

static GLuint *img_ptr;
static GLuint img_h;
static GLuint img_w;

static GLuint texName;

static GLuint rotation = 0;
static GLfloat spin = 0.0;
static GLfloat rx = 1.0;
static GLfloat ry = 1.0;
static GLfloat rz = 1.0;
static GLuint test = 0;

static const struct pixconv *pixconv;
static sem_t glut_sem;
static pthread_t glt;

/* */

static void *parent_class;

struct gst_gles2_sink {
	GstBaseSink parent;
};

struct gst_gles2_sink_class {
	GstBaseSinkClass parent_class;
};

/* */

static inline void convert_frame_test(void)
{
    int i, j;

    for(i = 0; i < img_h; ++i) {
        for(j = 0; j < img_w; ++j)
        {
			GLuint col = (255L<<24) + ((255L-j*2)<<16) + ((255L-i)<<8) + (255L-i*2);
			if ( ((i*j)/8) % 2 ) col = (GLuint) (255L<<24) + (255L<<16) + (0L<<8) + (255L);
            img_ptr[j*img_h + i] = col;

        }
    }
}

static void updateTexture()
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDeleteTextures(1, &texName);
    glGenTextures(1, &texName);

    glBindTexture(GL_TEXTURE_2D, texName);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_ptr);
}

static void display(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60, 1, 1, 10); /* fov, aspect, near, far */
    gluLookAt(0, 0, 6, /* */ 0, 0, 0, /* */ 0, 1, 0); /* eye, center, up */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_NORMALIZE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 0

    glRotatef(spin, 0.0, 0.0, 1.0);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-2.0, -2.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(-2.0, 2.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(2.0, 2.0, -1.0);
    glTexCoord2d(1, 1); glVertex3f(2.0, -2.0, -1.0);
    glEnd();

#else

    glRotatef(spin, rx, ry, rz);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, 1.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, -1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, -1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, -1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, -1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, -1.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, 1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, -1.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, 1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(1.0, 0.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(-1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(-1.0, 0.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, -1.0);
    glEnd();

#endif

    glPopAttrib();
    glFlush();
    glutSwapBuffers();
}

void updateDisplay(void)
{
    if (rotation % 2) {
        spin += 2.0;

	    if (spin > 360.0)
		    spin -= 360.0;
    }

    glutPostRedisplay();
}

void timerCallback(int value)
{
    sem_wait(&glut_sem);
    glDisable(GL_TEXTURE_2D);
    updateTexture();
    glEnable(GL_TEXTURE_2D);
    sem_post(&glut_sem);
}

void special(int key, int x, int y)
{
    switch (key) {
#if 1
        case 100:
            rx += 1.0;
            break;
        case 101:
            ry += 1.0;
            break;
        case 102:
            rx += -1.0;
            break;
        case 103:
            ry += -1.0;
            break;
        case 104:
            rz += 1.0;
            break;
        case 105:
            rz += -1.0;
            break;
#else
        case 100:
            rx = 0.0;
            ry = -1.0;
            rz = 0.0;
            break;
        case 101:
            rx = -1.0;
            ry = 0.0;
            rz = 0.0;
            break;
        case 102:
            rx = 0.0;
            ry = 1.0;
            rz = 0.0;
            break;
        case 103:
            rx = 1.0;
            ry = 0.0;
            rz = 0.0;
            break;
        case 104:
            rx = -1.0;
            ry = -1.0;
            rz = 0.0;
            break;
        case 105:
            rx = -1.0;
            ry = 1.0;
            rz = 0.0;
            break;
#endif
        default:
            break;
    }
}
void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
        case 'q':       /* quit */
            printf("exit...\n");
            exit(0);
            break;
        case 's':       /* rotation */
            rotation += 1;
            break;
        default:
            break;
    }
}

void reshape(int w, int h)
{
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    glOrtho(-100.0, 100.0, -100.0, 100.0, -100.0, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void * glut_thread(void *p)
{
    glutMainLoop();
    return NULL;
}

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

/*
	{
		GValue list;
		GValue val;

		list.g_type = val.g_type = 0;

		g_value_init(&list, GST_TYPE_LIST);
		g_value_init(&val, GST_TYPE_FOURCC);

		gst_value_set_fourcc(&val, GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'));
		gst_value_list_append_value(&list, &val);

		gst_structure_set_value(struc, "format", &list);

		g_value_unset(&val);
		g_value_unset(&list);
	}
*/

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

	fprintf(stderr, "%s: (h, w) = (%d, %d)\n", __func__, height, width);

	img_h = height;
	img_w = width;

	bufsize = img_h * img_w * 4;

	if (posix_memalign((void **) &img_ptr, 4, bufsize)) {
		fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
		return false;
	}

    pthread_create(&glt, NULL, glut_thread, NULL);

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

	char **argv = NULL;
    int argc = 0;

	int bufsize;

	fprintf(stderr, "--> %s\n", __func__);

    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { 0.0, 0.0, -5.0, 1.0 };
	GLfloat white_light[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat lmodel_ambient[] = { 0.1, 0.1, 0.1, 1.0 };

    /* GLUT init */
    glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(640, 480);
	glutInitWindowPosition(50, 50);
	glutCreateWindow("glut");

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_SMOOTH);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white_light);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);

    glutDisplayFunc(display);
	glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(updateDisplay);

    /* misc init */

    sem_init(&glut_sem, 0, 1);

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

	fprintf(stderr, "--> %s\n", __func__);
	fprintf(stderr, "--> %d\n", GST_BUFFER_SIZE(buffer));

	sem_wait(&glut_sem);
	memcpy(img_ptr, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
    sem_post(&glut_sem);

    glutTimerFunc(0, timerCallback, test++);

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
