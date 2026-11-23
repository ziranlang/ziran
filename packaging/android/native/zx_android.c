// ZX Runner on Android: a NativeActivity that hands a .zx path to the
// Ziran-generated runner and answers std/guest_window from the activity's
// surface. Everything the bundle draws is an RGBA frame the glue uploads as
// one texture and letterboxes onto the screen; touch, key and text input are
// translated into the same 40-byte guest events the desktop runner sends.
//
// The runner's own code (cmd/zx_runner*.zi) is compiled by zi2c and calls
// the zx_android_* functions at the bottom through std/guest_window.
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TAG "ZXRunner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

extern int main(int argc, char** argv);

// The checked-in bootstrap engine was generated for glibc, whose errno
// accessor is __errno_location; bionic calls the same storage __errno.
int* __errno_location(void) { return __errno(); }

#define EVENT_BYTES 40
#define EVENT_QUEUE 64

typedef struct {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    int surface_width;
    int surface_height;
    int ready;
    int presented;
    int closed;
    GLuint program;
    GLuint texture;
    GLuint quad;
    GLint position;
    GLint texcoord;
    GLint sampler;
    int texture_width;
    int texture_height;
    // Where the last frame landed on the surface, for touch mapping.
    int view_x;
    int view_y;
    int view_width;
    int view_height;
    uint8_t events[EVENT_QUEUE][EVENT_BYTES];
    int head;
    int tail;
} ZxAndroid;

static ZxAndroid zx;
static char launch_path[4096];

static void put_word(uint8_t* event, int at, int32_t value) {
    event[at] = (uint8_t)(value & 255);
    event[at + 1] = (uint8_t)((value >> 8) & 255);
    event[at + 2] = (uint8_t)((value >> 16) & 255);
    event[at + 3] = (uint8_t)((value >> 24) & 255);
}

static void queue_event(int kind, int32_t x, int32_t y, int32_t button, int32_t key,
                        int32_t modifiers, int32_t codepoint, int32_t wheel_x,
                        int32_t wheel_y, int32_t width, int32_t height) {
    uint8_t* event = zx.events[zx.tail];
    memset(event, 0, EVENT_BYTES);
    put_word(event, 0, kind);
    put_word(event, 4, x);
    put_word(event, 8, y);
    put_word(event, 12, button);
    put_word(event, 16, key);
    put_word(event, 20, modifiers);
    put_word(event, 24, codepoint);
    put_word(event, 28, wheel_x);
    put_word(event, 32, wheel_y);
    put_word(event, 36, (width << 16) | (height & 65535));
    int next = (zx.tail + 1) % EVENT_QUEUE;
    if (next == zx.head) zx.head = (zx.head + 1) % EVENT_QUEUE;
    zx.tail = next;
}

// A Java call needs the thread attached; android_main's thread becomes the
// main thread, so attaching is normally a no-op.
static JNIEnv* attach_env(int* attached) {
    struct android_app* app = zx.app;
    *attached = 0;
    if (app == NULL || app->activity == NULL || app->activity->vm == NULL) return NULL;
    JNIEnv* env = NULL;
    if ((*app->activity->vm)->GetEnv(app->activity->vm, (void**)&env, JNI_VERSION_1_6) == JNI_OK) return env;
    if ((*app->activity->vm)->AttachCurrentThread(app->activity->vm, &env, NULL) != JNI_OK) return NULL;
    *attached = 1;
    return env;
}

static void detach_env(int attached) {
    if (attached) (*zx.app->activity->vm)->DetachCurrentThread(zx.app->activity->vm);
}

static void read_launch_path(void) {
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, cls, "launchPath", "()Ljava/lang/String;");
    if (method != NULL) {
        jstring text = (jstring)(*env)->CallStaticObjectMethod(env, cls, method);
        if (text != NULL) {
            const char* utf = (*env)->GetStringUTFChars(env, text, NULL);
            if (utf != NULL) {
                strncpy(launch_path, utf, sizeof(launch_path) - 1);
                (*env)->ReleaseStringUTFChars(env, text, utf);
            }
            (*env)->DeleteLocalRef(env, text);
        }
    }
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
}

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int setup_graphics(void) {
    static const char* vertex_source =
        "attribute vec2 position; attribute vec2 texcoord; varying vec2 uv;\n"
        "void main() { uv = texcoord; gl_Position = vec4(position, 0.0, 1.0); }\n";
    static const char* fragment_source =
        "precision mediump float; varying vec2 uv; uniform sampler2D picture;\n"
        "void main() { gl_FragColor = texture2D(picture, uv); }\n";
    if (zx.program != 0) return 1;
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex == 0 || fragment == 0) return 0;
    zx.program = glCreateProgram();
    glAttachShader(zx.program, vertex);
    glAttachShader(zx.program, fragment);
    glLinkProgram(zx.program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = 0;
    glGetProgramiv(zx.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(zx.program, sizeof(log), NULL, log);
        LOGE("program: %s", log);
        glDeleteProgram(zx.program);
        zx.program = 0;
        return 0;
    }
    zx.position = glGetAttribLocation(zx.program, "position");
    zx.texcoord = glGetAttribLocation(zx.program, "texcoord");
    zx.sampler = glGetUniformLocation(zx.program, "picture");
    // Two triangles covering the surface; the texture is flipped in Y because
    // the guest's first row is the top of the picture.
    static const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };
    glGenBuffers(1, &zx.quad);
    glBindBuffer(GL_ARRAY_BUFFER, zx.quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glGenTextures(1, &zx.texture);
    glBindTexture(GL_TEXTURE_2D, zx.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return 1;
}

static int init_surface(void) {
    if (zx.app->window == NULL) return 0;
    if (zx.display == EGL_NO_DISPLAY) {
        zx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (zx.display == EGL_NO_DISPLAY || !eglInitialize(zx.display, NULL, NULL)) {
            LOGE("no EGL display");
            zx.display = EGL_NO_DISPLAY;
            return 0;
        }
        const EGLint attributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE,
        };
        EGLint count = 0;
        if (!eglChooseConfig(zx.display, attributes, &zx.config, 1, &count) || count < 1) {
            LOGE("no EGL config");
            return 0;
        }
        const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        zx.context = eglCreateContext(zx.display, zx.config, EGL_NO_CONTEXT, context_attributes);
        if (zx.context == EGL_NO_CONTEXT) {
            LOGE("no EGL context");
            return 0;
        }
    }
    zx.surface = eglCreateWindowSurface(zx.display, zx.config, zx.app->window, NULL);
    if (zx.surface == EGL_NO_SURFACE) {
        LOGE("no EGL surface");
        return 0;
    }
    if (!eglMakeCurrent(zx.display, zx.surface, zx.surface, zx.context)) {
        LOGE("eglMakeCurrent failed");
        return 0;
    }
    EGLint width = 0;
    EGLint height = 0;
    eglQuerySurface(zx.display, zx.surface, EGL_WIDTH, &width);
    eglQuerySurface(zx.display, zx.surface, EGL_HEIGHT, &height);
    zx.surface_width = width;
    zx.surface_height = height;
    if (!setup_graphics()) return 0;
    zx.ready = 1;
    queue_event(2, 0, 0, 0, 0, 0, 0, 0, 0, width, height);
    LOGI("surface %dx%d", width, height);
    return 1;
}

static void drop_surface(void) {
    if (zx.display != EGL_NO_DISPLAY && zx.surface != EGL_NO_SURFACE) {
        eglMakeCurrent(zx.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(zx.display, zx.surface);
    }
    zx.surface = EGL_NO_SURFACE;
    zx.ready = 0;
}

static void handle_command(struct android_app* app, int32_t command) {
    (void)app;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            init_surface();
            break;
        case APP_CMD_TERM_WINDOW:
            drop_surface();
            queue_event(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            break;
        case APP_CMD_GAINED_FOCUS:
            queue_event(10, 0, 0, 0, 0, 0, 0, 0, 0, zx.surface_width, zx.surface_height);
            break;
        case APP_CMD_LOST_FOCUS:
            queue_event(11, 0, 0, 0, 0, 0, 0, 0, 0, zx.surface_width, zx.surface_height);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (zx.ready && zx.display != EGL_NO_DISPLAY) {
                EGLint width = 0;
                EGLint height = 0;
                eglQuerySurface(zx.display, zx.surface, EGL_WIDTH, &width);
                eglQuerySurface(zx.display, zx.surface, EGL_HEIGHT, &height);
                if (width != zx.surface_width || height != zx.surface_height) {
                    zx.surface_width = width;
                    zx.surface_height = height;
                    queue_event(2, 0, 0, 0, 0, 0, 0, 0, 0, width, height);
                }
            }
            break;
        case APP_CMD_DESTROY:
            zx.closed = 1;
            queue_event(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            break;
        default:
            break;
    }
}

static int32_t map_key(int32_t code) {
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) return 'A' + (code - AKEYCODE_A);
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) return '0' + (code - AKEYCODE_0);
    if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) return 290 + (code - AKEYCODE_F1);
    switch (code) {
        case AKEYCODE_ESCAPE: return 256;
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER: return 257;
        case AKEYCODE_TAB: return 258;
        case AKEYCODE_DEL: return 259;
        case AKEYCODE_INSERT: return 260;
        case AKEYCODE_FORWARD_DEL: return 261;
        case AKEYCODE_DPAD_RIGHT: return 262;
        case AKEYCODE_DPAD_LEFT: return 263;
        case AKEYCODE_DPAD_DOWN: return 264;
        case AKEYCODE_DPAD_UP: return 265;
        case AKEYCODE_PAGE_UP: return 266;
        case AKEYCODE_PAGE_DOWN: return 267;
        case AKEYCODE_MOVE_HOME: return 268;
        case AKEYCODE_MOVE_END: return 269;
        case AKEYCODE_SPACE: return 32;
        case AKEYCODE_COMMA: return 44;
        case AKEYCODE_PERIOD: return 46;
        case AKEYCODE_SLASH: return 47;
        case AKEYCODE_SEMICOLON: return 59;
        case AKEYCODE_APOSTROPHE: return 39;
        case AKEYCODE_LEFT_BRACKET: return 91;
        case AKEYCODE_RIGHT_BRACKET: return 93;
        case AKEYCODE_BACKSLASH: return 92;
        case AKEYCODE_GRAVE: return 96;
        case AKEYCODE_MINUS: return 45;
        case AKEYCODE_EQUALS: return 61;
        default: return 0;
    }
}

static int32_t modifiers_of(int32_t meta) {
    int32_t modifiers = 0;
    if ((meta & AMETA_SHIFT_ON) != 0) modifiers |= 1;
    if ((meta & AMETA_CTRL_ON) != 0) modifiers |= 2;
    if ((meta & AMETA_ALT_ON) != 0) modifiers |= 4;
    if ((meta & AMETA_META_ON) != 0) modifiers |= 8;
    return modifiers;
}

// Surface pixels to the guest's picture pixels, so a letterboxed frame gets
// touches where the user sees them. Non-positive view sizes mean the whole
// surface is the picture.
static int32_t map_x(float x) {
    if (zx.view_width <= 0 || zx.texture_width <= 0) return (int32_t)x;
    float scaled = (x - (float)zx.view_x) * ((float)zx.texture_width / (float)zx.view_width);
    if (scaled < 0.0f) scaled = 0.0f;
    if (scaled > (float)zx.texture_width) scaled = (float)zx.texture_width;
    return (int32_t)scaled;
}

static int32_t map_y(float y) {
    if (zx.view_height <= 0 || zx.texture_height <= 0) return (int32_t)y;
    float scaled = (y - (float)zx.view_y) * ((float)zx.texture_height / (float)zx.view_height);
    if (scaled < 0.0f) scaled = 0.0f;
    if (scaled > (float)zx.texture_height) scaled = (float)zx.texture_height;
    return (int32_t)scaled;
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    (void)app;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float raw_x = AMotionEvent_getX(event, 0);
        float raw_y = AMotionEvent_getY(event, 0);
        int32_t x = map_x(raw_x) * 100;
        int32_t y = map_y(raw_y) * 100;
        int32_t modifiers = modifiers_of(AMotionEvent_getMetaState(event));
        switch (action) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                queue_event(4, x, y, 1, 0, modifiers, 0, 0, 0, 0, 0);
                return 1;
            case AMOTION_EVENT_ACTION_MOVE:
                queue_event(3, x, y, 0, 0, modifiers, 0, 0, 0, 0, 0);
                return 1;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
            case AMOTION_EVENT_ACTION_CANCEL:
                queue_event(5, x, y, 1, 0, modifiers, 0, 0, 0, 0, 0);
                return 1;
            default:
                return 1;
        }
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);
        if (code == AKEYCODE_BACK) {
            if (action == AKEY_EVENT_ACTION_UP) {
                queue_event(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                zx.closed = 1;
            }
            return 1;
        }
        if (code == AKEYCODE_VOLUME_UP || code == AKEYCODE_VOLUME_DOWN ||
            code == AKEYCODE_HOME || code == AKEYCODE_APP_SWITCH) {
            return 0;
        }
        int32_t key = map_key(code);
        int32_t modifiers = modifiers_of(AKeyEvent_getMetaState(event));
        if (action == AKEY_EVENT_ACTION_DOWN) {
            if (AKeyEvent_getRepeatCount(event) == 0) {
                queue_event(7, 0, 0, 0, key, modifiers, 0, 0, 0, 0, 0);
                // The NDK exposes key codes, not composed characters: letters
                // follow Shift and the printable keys travel as ASCII.
                int32_t character = 0;
                if (key >= 'A' && key <= 'Z') character = (modifiers & 1) != 0 ? key : key + 32;
                else if (key >= 32 && key <= 126) character = key;
                if (character != 0) {
                    queue_event(9, 0, 0, 0, 0, modifiers, character, 0, 0, 0, 0);
                }
            }
            return key != 0 ? 1 : 0;
        }
        if (action == AKEY_EVENT_ACTION_UP) {
            queue_event(8, 0, 0, 0, key, modifiers, 0, 0, 0, 0, 0);
            return key != 0 ? 1 : 0;
        }
        return 0;
    }
    return 0;
}

typedef struct {
    int read_end;
    int remember;
} LogPipe;

static LogPipe output_pipe;
static LogPipe error_pipe;
static char error_text[4096];
static size_t error_length;

// Keeps the tail of the runner's error output for the failure dialog.
static void remember_error(const char* text, size_t count) {
    if (count >= sizeof(error_text) - 1) {
        text += count - (sizeof(error_text) - 2);
        count = sizeof(error_text) - 2;
    }
    if (error_length + count >= sizeof(error_text)) {
        size_t drop = error_length + count - (sizeof(error_text) - 2);
        if (drop > error_length) drop = error_length;
        memmove(error_text, error_text + drop, error_length - drop);
        error_length -= drop;
    }
    memcpy(error_text + error_length, text, count);
    error_length += count;
    error_text[error_length] = 0;
}

static void* log_pipe(void* argument) {
    LogPipe* pipe = (LogPipe*)argument;
    char buffer[512];
    ssize_t count;
    while ((count = read(pipe->read_end, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[count] = 0;
        if (pipe->remember) remember_error(buffer, (size_t)count);
        __android_log_write(ANDROID_LOG_INFO, TAG, buffer);
    }
    return NULL;
}

static void start_log_pipe(void) {
    int ends[2];
    if (pipe(ends) == 0) {
        output_pipe.read_end = ends[0];
        output_pipe.remember = 0;
        dup2(ends[1], 1);
        pthread_t thread;
        if (pthread_create(&thread, NULL, log_pipe, &output_pipe) == 0) pthread_detach(thread);
    }
    if (pipe(ends) == 0) {
        error_pipe.read_end = ends[0];
        error_pipe.remember = 1;
        dup2(ends[1], 2);
        pthread_t thread;
        if (pthread_create(&thread, NULL, log_pipe, &error_pipe) == 0) pthread_detach(thread);
    }
}

static void pump(int milliseconds) {
    int events = 0;
    struct android_poll_source* source = NULL;
    int timeout = milliseconds;
    for (;;) {
        int ident = ALooper_pollOnce(timeout, NULL, &events, (void**)&source);
        if (ident == ALOOPER_POLL_TIMEOUT || ident == ALOOPER_POLL_ERROR) break;
        if (source != NULL) source->process(zx.app, source);
        timeout = 0;
        if (zx.closed) break;
    }
}

static void call_runner_finished(void) {
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, "runnerFinished", "()V");
    if (method != NULL) (*env)->CallVoidMethod(env, activity, method);
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
}

// A console bundle and a failure before the first frame both leave the
// activity without a drawn window; its starting window then waits, and input
// dispatch times out. Paint one plain frame when the bundle never did, so the
// activity is visible and can finish, and a dialog has a real window.
static void paint_plain_frame(void) {
    if (zx.presented) return;
    for (int waited = 0; waited < 5000 && !zx.ready && !zx.closed; waited += 20) pump(20);
    if (!zx.ready || zx.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(zx.display, zx.surface, zx.surface, zx.context);
    glViewport(0, 0, zx.surface_width, zx.surface_height);
    glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(zx.display, zx.surface);
    zx.presented = 1;
}

static void call_show_error(const char* message) {
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, "showError", "(Ljava/lang/String;)V");
    if (method == NULL) {
        LOGE("showError method missing");
    } else {
        jstring text = (*env)->NewStringUTF(env, message);
        if (text == NULL) {
            LOGE("showError message could not be created");
        } else {
            (*env)->CallVoidMethod(env, activity, method, text);
            (*env)->DeleteLocalRef(env, text);
        }
    }
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
}

void android_main(struct android_app* app) {
    memset(&zx, 0, sizeof(zx));
    zx.app = app;
    zx.display = EGL_NO_DISPLAY;
    zx.surface = EGL_NO_SURFACE;
    zx.context = EGL_NO_CONTEXT;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;
    read_launch_path();
    start_log_pipe();
    if (launch_path[0] == 0) {
        LOGE("no .zx file to run");
        ANativeActivity_finish(app->activity);
        return;
    }
    LOGI("running %s", launch_path);
    char* arguments[2];
    arguments[0] = (char*)"zx";
    arguments[1] = launch_path;
    int status = main(2, arguments);
    // Let the error pipe's reader catch up with the last write before the
    // failure is judged.
    fflush(NULL);
    usleep(100000);
    LOGI("exit %d, %zu error bytes", status, error_length);
    if (status != 0 && error_length > 0) {
        // The dialog closes the activity after the user reads it.
        paint_plain_frame();
        usleep(150000);
        call_show_error(error_text);
    } else {
        paint_plain_frame();
        usleep(100000);
        call_runner_finished();
        ANativeActivity_finish(app->activity);
        // The runner process starts one program and then ends, so the next
        // launch gets a fresh NativeActivity and engine.
        _exit(0);
    }
}

int32_t zx_android_ready(void) {
    // The surface arrives after the activity starts; wait for it while the
    // bundle is still opening its window.
    for (int waited = 0; waited < 10000 && !zx.ready && !zx.closed; waited += 20) pump(20);
    return zx.ready;
}

int32_t zx_android_width(void) { return zx.surface_width; }

int32_t zx_android_height(void) { return zx.surface_height; }

int32_t zx_android_present(const uint8_t* pixels, int32_t width, int32_t height) {
    if (!zx.ready || zx.program == 0 || pixels == NULL || width <= 0 || height <= 0) return 0;
    eglMakeCurrent(zx.display, zx.surface, zx.surface, zx.context);
    glBindTexture(GL_TEXTURE_2D, zx.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (width != zx.texture_width || height != zx.texture_height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        zx.texture_width = width;
        zx.texture_height = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    int surface_width = zx.surface_width > 0 ? zx.surface_width : width;
    int surface_height = zx.surface_height > 0 ? zx.surface_height : height;
    float scale_x = (float)surface_width / (float)width;
    float scale_y = (float)surface_height / (float)height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    int shown_width = (int)((float)width * scale);
    int shown_height = (int)((float)height * scale);
    if (shown_width < 1) shown_width = 1;
    if (shown_height < 1) shown_height = 1;
    zx.view_width = shown_width;
    zx.view_height = shown_height;
    zx.view_x = (surface_width - shown_width) / 2;
    zx.view_y = (surface_height - shown_height) / 2;
    glViewport(zx.view_x, zx.view_y, shown_width, shown_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(zx.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, zx.texture);
    glUniform1i(zx.sampler, 0);
    glBindBuffer(GL_ARRAY_BUFFER, zx.quad);
    glEnableVertexAttribArray((GLuint)zx.position);
    glVertexAttribPointer((GLuint)zx.position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray((GLuint)zx.texcoord);
    glVertexAttribPointer((GLuint)zx.texcoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)zx.position);
    glDisableVertexAttribArray((GLuint)zx.texcoord);
    eglSwapBuffers(zx.display, zx.surface);
    zx.presented = 1;
    return 1;
}

int32_t zx_android_next_event(uint8_t* out) {
    pump(0);
    if (zx.head == zx.tail) return 0;
    memcpy(out, zx.events[zx.head], EVENT_BYTES);
    zx.head = (zx.head + 1) % EVENT_QUEUE;
    return 1;
}

int32_t zx_android_wait(int32_t milliseconds) {
    if (zx.head != zx.tail) return 1;
    if (milliseconds < 0) milliseconds = 0;
    pump(milliseconds);
    return zx.head != zx.tail;
}

void zx_android_set_title(const char* title) {
    (void)title;
}

static void call_boolean(const char* name, const char* signature, int32_t value) {
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, name, signature);
    if (method != NULL) {
        (*env)->CallVoidMethod(env, activity, method, value != 0 ? JNI_TRUE : JNI_FALSE);
    }
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
}

void zx_android_set_text_input(int32_t active) {
    call_boolean("textInput", "(Z)V", active);
}

int32_t zx_android_clipboard_read(char* out, int32_t capacity) {
    if (out == NULL || capacity < 1) return -1;
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return -1;
    int32_t length = -1;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, "clipboardRead", "()Ljava/lang/String;");
    if (method != NULL) {
        jstring text = (jstring)(*env)->CallObjectMethod(env, activity, method);
        if (text != NULL) {
            const char* utf = (*env)->GetStringUTFChars(env, text, NULL);
            if (utf != NULL) {
                length = (int32_t)strlen(utf);
                if (length > capacity - 1) length = capacity - 1;
                memcpy(out, utf, (size_t)length);
                (*env)->ReleaseStringUTFChars(env, text, utf);
            }
            (*env)->DeleteLocalRef(env, text);
        } else {
            length = 0;
        }
    }
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
    return length;
}

int32_t zx_android_clipboard_write(const char* text) {
    if (text == NULL) return 0;
    int attached = 0;
    JNIEnv* env = attach_env(&attached);
    if (env == NULL) return 0;
    int32_t written = 0;
    jobject activity = zx.app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, "clipboardWrite", "(Ljava/lang/String;)V");
    if (method != NULL) {
        jstring value = (*env)->NewStringUTF(env, text);
        if (value != NULL) {
            (*env)->CallVoidMethod(env, activity, method, value);
            (*env)->DeleteLocalRef(env, value);
            written = 1;
        }
    }
    (*env)->DeleteLocalRef(env, cls);
    detach_env(attached);
    return written;
}

void zx_android_close(void) {
    zx.closed = 1;
    if (zx.app != NULL && zx.app->activity != NULL) ANativeActivity_finish(zx.app->activity);
}
