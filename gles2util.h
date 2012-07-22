#ifndef GLES2UTIL_H
#define GLES2UTIL_H

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <glib.h>

extern GLuint *img_ptr;
extern GLuint img_h;
extern GLuint img_w;

#if 0
extern sem_t gles2_sem;
#else
extern volatile gint gles2_sem;
#endif

extern bool glt_active;
extern pthread_t glt;


void * gles2_thread(void *p);
int gles2_start(void);

#endif	/* GLES2UTIL_H */
