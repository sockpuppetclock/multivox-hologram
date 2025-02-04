#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include "sim.h"

static const char* window_title = "virtex";
static long event_mask = KeyPressMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ExposureMask|StructureNotifyMask;

typedef struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLConfig config;
    EGLContext context;
} egl_state_t;

typedef struct {
    Display* display;
    Window window;
    XWindowAttributes attributes;
    Atom wm_delete_msg;
} x11_state_t;



static bool init_x11(x11_state_t* x11) {
    x11->display = XOpenDisplay(NULL);
    if (x11->display == NULL) {
        printf("XOpenDisplay failed.\n");
        return false;
    }

    x11->window = XCreateWindow(
        x11->display,
        DefaultRootWindow(x11->display),
        0, 0, 800, 600, 0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWEventMask,
        &(XSetWindowAttributes){.event_mask = event_mask});

    XStoreName(x11->display, x11->window, window_title);
    XSetWMHints(x11->display, x11->window, &(XWMHints){.input=True, .flags=InputHint});

    XMapWindow(x11->display, x11->window);

    x11->wm_delete_msg = XInternAtom(x11->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11->display, x11->window, &x11->wm_delete_msg, 1);

    XGetWindowAttributes(x11->display, x11->window, &x11->attributes);


    return true;
}

static void resize_egl(const x11_state_t* x11, egl_state_t* egl) {
    if (egl->surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl->display, egl->surface);
    }
    egl->surface = eglCreateWindowSurface(egl->display, egl->config, x11->window, NULL);
    if (egl->surface == EGL_NO_SURFACE) {
        printf("eglCreateWindowSurface failed.\n");
        exit(1);
    }

    eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);

    sim_resize(x11->attributes.width, x11->attributes.height);
}

static bool init_egl(const x11_state_t* x11, egl_state_t* egl) {

    egl->display = eglGetDisplay((EGLNativeDisplayType)x11->display);
    if (egl->display == EGL_NO_DISPLAY) {
        printf("eglGetDisplay failed.\n");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
        printf("eglInitialize failed.\n");
        return false;
    }

    EGLint num_config;
    EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                            EGL_RED_SIZE, 8,
                            EGL_GREEN_SIZE, 8,
                            EGL_BLUE_SIZE, 8,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                            EGL_CONFIG_CAVEAT, EGL_NONE,
                            EGL_NONE};
    if (!eglChooseConfig(egl->display, attrib_list, &egl->config, 1, &num_config) || num_config != 1) {
        printf("eglChooseConfig failed. (%d)\n", eglGetError());
        return false;
    }

    egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    if (egl->context == EGL_NO_CONTEXT) {
        printf("eglCreateContext failed. (%d)\n", eglGetError());
        return false;
    }

    resize_egl(x11, egl);

    EGLint value;
    if (!eglQueryContext(egl->display, egl->context, EGL_RENDER_BUFFER, &value)) {
        printf("eglQueryContext failed: %d\n", eglGetError());
    }

    return true;
}

static void close_egl(egl_state_t* egl) {
    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl->display, egl->context);
    eglDestroySurface(egl->display, egl->surface);
    eglTerminate(egl->display);
}

static void close_x11(x11_state_t* x11) {
    XDestroyWindow(x11->display, x11->window);
    XCloseDisplay(x11->display);
}

static bool process_events(x11_state_t* x11, egl_state_t* egl) {
    static int wheel = 0;
    static int mousex = 0;
    static int mousey = 0;

    static bool dragging = false;
    static int dragx = 0;
    static int dragy = 0;

    while (XPending(x11->display)) {
        XEvent xev;
        XNextEvent(x11->display, &xev);
        switch (xev.type) {
            case ConfigureNotify: {
                XGetWindowAttributes(x11->display, x11->window, &x11->attributes);
                resize_egl(x11, egl);
            } break;

            case ClientMessage: {
                if (xev.xclient.data.l[0] == x11->wm_delete_msg) {
                    return false;
                }
            } break;

            case MotionNotify: {
                mousex = xev.xmotion.x;
                mousey = xev.xmotion.y;
            } break;

            case ButtonPress: {
                switch (xev.xbutton.button) {
                    case 1:
                        dragging = true;
                        mousex = xev.xbutton.x;
                        mousey = xev.xbutton.y;
                        dragx = mousex;
                        dragy = mousey;
                        break;
                    
                    case 4:
                        ++wheel;
                        break;

                    case 5:
                        --wheel;
                        break;
                }
            } break;

            case ButtonRelease: {
                if (xev.xbutton.button == 1) {
                    dragging = false;
                }
            } break;
        }
    }

    if (dragging) {
        if (mousex != dragx || mousey != dragy) {
            float scale = (float)(x11->attributes.width + x11->attributes.height) * 0.5f;
            sim_drag((float)(mousex - dragx) / scale, (float)(mousey - dragy) / scale);
            dragx = mousex;
            dragy = mousey;
        }
    }

    if (wheel) {
        sim_zoom((float)wheel / 12.0f);
        wheel = 0;
    }

    return true;
}

int main(int argc, char** argv) {
    x11_state_t x11 = {0};
    if (!init_x11(&x11)) {
        printf("init_x11 failed.\n");
        return 1;
    }

    egl_state_t egl = {0};
    if (!init_egl(&x11, &egl)) {
        printf("init_egl failed.\n");
        return 1;
    }

    if (!sim_init(argc, argv)) {
        return 1;
    }

    do {
        //glClearColor(0.0f, (float)(rand()&0xff) / 255.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        sim_draw();

        eglSwapBuffers(egl.display, egl.surface);

    } while (process_events(&x11, &egl));

    close_egl(&egl);
    close_x11(&x11);

    return 0;
}

