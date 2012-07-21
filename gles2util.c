/*
    Copyright (C) 2009 Mans Rullgard
    Copyright (C) 2012 matsi

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#include <math.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <semaphore.h>

#include "gles2util.h"
#include "x11util.h"

/* */

#define WINDOW_WIDTH	600
#define WINDOW_HEIGHT	600

#define VERTEX_ARRAY	0
#define TEXCOORD_ARRAY	1

/* */

static EGLDisplay  eglDisplay	= 0;
static EGLConfig   eglConfig	= 0;
static EGLSurface  eglSurface	= 0;
static EGLContext  eglContext	= 0;

static Display *x11Display	= 0;
static Window x11Window	= 0;

static GLuint shaderObject;
static GLuint tex;
static GLuint vbo;

static GLfloat	m_fAngle = 0.0;
static GLfloat spin = 1.0;

/* */

static inline uint32_t UP_PWR2(uint32_t x)
{
        return 1 << (32 - __builtin_clz (x - 1));
}

static inline uint32_t DOWN_PWR2(uint32_t x)
{
        return 1 << ((32 - __builtin_clz (x - 1)) - 1);
}

static int TestEGLError(char* msg)
{
    EGLint err = eglGetError();
    if (err != EGL_SUCCESS)
    {
        fprintf(stderr, "EGL: %s failed (%d).\n", msg, err);
        return 0;
    }

    return 1;
}

#if 0
static void TestGLError(char* msg)
{
    GLenum err = glGetError();

    if (err != GL_NO_ERROR) {
        fprintf(stderr, "OPENGL: %s failed with (%d).\n", msg, err);
    }

    return;
}
#endif

static void update_texture()
{
    glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_ptr);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void display(void)
{
	float pfIdentity[] = {
		cos(m_fAngle),	0,	sin(m_fAngle),	0,
		0,				1,	0,				0,
		-sin(m_fAngle),	0,	cos(m_fAngle),	0,
		0,				0,	0,				1
	};

	int lc;

    glClear(GL_COLOR_BUFFER_BIT);

	lc = glGetUniformLocation(shaderObject, "myPMVMatrix");
	glUniformMatrix4fv(lc, 1, GL_FALSE, pfIdentity);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Pass the vertex data

	glEnableVertexAttribArray(VERTEX_ARRAY);
	glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), 0);

	// pass the texture coordinate data

	glEnableVertexAttribArray(TEXCOORD_ARRAY);
	glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (void*) (3 * sizeof(GLfloat)));

	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

    eglSwapBuffers(eglDisplay, eglSurface);
	m_fAngle += .02f;
}

static void main_loop(Display *xdisp)
{
	XEvent	event;
	int xmsgs;

	char buffer[10];
	int i, code;

	while(1) {

		xmsgs = XPending(xdisp);

		for(i = 0; i < xmsgs; i++ ) {

			XNextEvent(xdisp, &event);

			switch (event.type) {
				case Expose:
					printf("EXPOSE\n");
					break;

				case ConfigureNotify:
					/* reshape(event.xconfigure.width, event.xconfigure.height); */
					break;

				case KeyPress:
					code = XLookupKeysym(&event.xkey, 0);

					if (code == XK_Left) {
						printf("LEFT\n");
						spin = -1.0;
					}
					else if (code == XK_Right) {
						printf("RIGHT\n");
						spin = +1.0;
					}
					else if (code == XK_Up) {
						printf("UP\n");
						spin = 0.0;
					}
					else if (code == XK_Down) {
						printf("UP\n");
						spin = 0.0;
					}
					else {
						XLookupString(&event.xkey, buffer, sizeof(buffer), NULL, NULL);
						if (buffer[0] == 27) {
							exit(0);
						}
						else if (buffer[0] == 'q') {
							exit(0);
						}
						else {
							printf("KeyPress[%d, %d, %d, %d]\n",
								buffer[0], buffer[1], buffer[2], buffer[3]);
						}
					}
					break;

				default:
					break;
			}
		}

        sem_wait(&gles2_sem);
        update_texture();
        sem_post(&gles2_sem);

        display();
	}
}


int gles2_start(void)
{
	EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

	EGLint iMajorVersion, iMinorVersion;
	EGLint pi32ConfigAttribs[5];
	int iConfigs;

	int ret;

	/* Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output */

	ret = xutil_create_display(&x11Display, &x11Window, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (!ret) {
		goto cleanup;
	}

	/* Step 1 - Get the default display */

	eglDisplay = eglGetDisplay((NativeDisplayType)x11Display);

	/* Step 2 - Initialize EGL */

	if (!eglInitialize(eglDisplay, &iMajorVersion, &iMinorVersion)) {
		fprintf(stderr, "Error: eglInitialize() failed.\n");
		goto cleanup;
	}

	/* Step 3 - Specify the required configuration attributes */

	pi32ConfigAttribs[0] = EGL_SURFACE_TYPE;
	pi32ConfigAttribs[1] = EGL_WINDOW_BIT;
	pi32ConfigAttribs[2] = EGL_RENDERABLE_TYPE;
	pi32ConfigAttribs[3] = EGL_OPENGL_ES2_BIT;
	pi32ConfigAttribs[4] = EGL_NONE;

	/* Step 4 - Find a config that matches all requirements */

	if (!eglChooseConfig(eglDisplay, pi32ConfigAttribs, &eglConfig, 1, &iConfigs) || (iConfigs != 1)) {
		fprintf(stderr, "Error: eglChooseConfig() failed.\n");
		goto cleanup;
	}

	/* Step 5 - Create a surface to draw to */

	eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (NativeWindowType)x11Window, NULL);

	if (!TestEGLError("eglCreateWindowSurface")) {
		goto cleanup;
	}

	/* Step 6 - Create a context */

	eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, ai32ContextAttribs);

	if (!TestEGLError("eglCreateContext")) {
		goto cleanup;
	}

    return 0;

cleanup:

	eglTerminate(eglDisplay);
    return -1;
}

void * gles2_thread(void *p)
{
    GLfloat afVertices[] = {
        -0.4f, -0.4f, +0.4f,
		+0.0f, +0.0f,
        +0.4f, -0.4f, +0.4f,
		+1.0f, +0.0f,
        +0.0f, +0.4f, -0.4f,
		+0.5f, +1.0f,
    };

	GLint shaderCompiled;
    GLint shaderLinked;

	GLuint shaderFragment;
	GLuint shaderVertex;

	/* shader code */

	const char* shaderFragmentCode = "\
		uniform sampler2D sampler2d;\
		varying mediump vec2	myTexCoord;\
		void main (void)\
		{\
		    gl_FragColor = texture2D(sampler2d,myTexCoord);\
		}";

	const char* shaderVertexCode = "\
		attribute highp vec4	myVertex;\
		attribute mediump vec4	myUV;\
		uniform mediump mat4	myPMVMatrix;\
		varying mediump vec2	myTexCoord;\
		void main(void)\
		{\
			gl_Position = myPMVMatrix * myVertex;\
			myTexCoord = myUV.st;\
		}";

    /* we make current context in this thread */

    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    if (!TestEGLError("eglMakeCurrent")) {
        goto display_out;
    }

	/* create shaders */

	shaderFragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shaderFragment, 1, (const char**)&shaderFragmentCode, NULL);
	glCompileShader(shaderFragment);

	glGetShaderiv(shaderFragment, GL_COMPILE_STATUS, &shaderCompiled);

	if (!shaderCompiled) {
		int infoLogLength, charsWritten;
		char* infoLog;

		glGetShaderiv(shaderFragment, GL_INFO_LOG_LENGTH, &infoLogLength);

		infoLog = calloc(1, infoLogLength);
        glGetShaderInfoLog(shaderFragment, infoLogLength, &charsWritten, infoLog);

		printf("Failed to compile fragment shader: %s\n", infoLog);
		free(infoLog);
		exit(0);
	}

	shaderVertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shaderVertex, 1, (const char**)&shaderVertexCode, NULL);
	glCompileShader(shaderVertex);
    glGetShaderiv(shaderVertex, GL_COMPILE_STATUS, &shaderCompiled);

	if (!shaderCompiled) {
		int infoLogLength, charsWritten;
		char* infoLog;

		glGetShaderiv(shaderVertex, GL_INFO_LOG_LENGTH, &infoLogLength);

		infoLog = calloc(1, infoLogLength);
        glGetShaderInfoLog(shaderFragment, infoLogLength, &charsWritten, infoLog);

		printf("Failed to compile vertex shader: %s\n", infoLog);
		free(infoLog);
		exit(0);
	}

    shaderObject = glCreateProgram();

    glAttachShader(shaderObject, shaderFragment);
    glAttachShader(shaderObject, shaderVertex);


    glBindAttribLocation(shaderObject, VERTEX_ARRAY, "myVertex");
    glBindAttribLocation(shaderObject, TEXCOORD_ARRAY, "myUV");

    glLinkProgram(shaderObject);
    glGetProgramiv(shaderObject, GL_LINK_STATUS, &shaderLinked);

	if (!shaderLinked) {
		int infoLogLength, charsWritten;
		char* infoLog;

		glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);

		infoLog = calloc(1, infoLogLength);
        glGetShaderInfoLog(shaderFragment, infoLogLength, &charsWritten, infoLog);

		printf("Failed to link shaders: %s\n", infoLog);
		free(infoLog);
		exit(0);
	}

    glUseProgram(shaderObject);
	glUniform1i(glGetUniformLocation(shaderObject, "sampler2d"), 0);

    /* create VBO for 3D scene */

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 3*5*sizeof(GLfloat), afVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* init test texture */
	update_texture();

	/* event/render loop */

	main_loop(x11Display);

    glDeleteBuffers(1, &vbo);
	glDeleteTextures(1, &tex);

display_out:

    /* delete context */

	eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(eglDisplay);

    return NULL;
}
