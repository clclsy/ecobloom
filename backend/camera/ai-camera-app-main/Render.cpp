/*
 * Copyright 2025 QNX Software Systems Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <EGL/egl.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include <array>
#include <Logging.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#include <vector>

#include "Global.h"
#include "Processor.h"
#include "Render.h"
#include "Shaders.h"
#include "Video.h"

// --------------------------------------------------------------------
// Types
// --------------------------------------------------------------------
struct gl_attr_def {
    const char *name;
    GLuint idx;
};
typedef struct gl_attr_def gl_attr_def_t;

struct gl_pipeline {
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint program;
};
typedef struct gl_pipeline gl_pipeline_t;

typedef enum {
    RENDER_STATE_UNINITIALIZED = 0,
    RENDER_STATE_INITIALIZED,
    RENDER_STATE_ACTIVE,
    RENDER_STATE_STOPPING
} render_state_t;

struct point_f {
    float x;
    float y;
};
typedef struct point_f point_f_t;

struct point_u16 {
    uint16_t x;
    uint16_t y;
};
typedef struct point_u16 point_u16_t;

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define BORDER_THICKNESS 3

static const EGLint EglCtxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

// Matrix to convert YCbCr/YUV to RGBA.
// It assumes full-range YCbCr values with component
// values in the range [0, 1.0].
// See: https://web.archive.org/web/20180423091842/http://www.equasys.de/colorconversion.html
//
// Output RGBA values are in range [0, 1.0]
//
// NOTE: Column major order
static const float YuvToRgbMatrix[16] = {
        1,              1,              1,              0,
        0,         -0.343,          1.765,              0,
      1.4,         -0.711,              0,              0,
   -0.703,          0.529,         -0.886,              1
};

#define QUAD_FLAG_WINDING_CCW (0 << 0)
#define QUAD_FLAG_WINDING_CW  (1 << 0)
#define QUAD_FLAG_WINDING_MASK (1 << 0)

#define QUAD_FLAG_ORIGIN_SHIFT 1
#define QUAD_FLAG_ORIGIN_TOP    (0 << QUAD_FLAG_ORIGIN_SHIFT)
#define QUAD_FLAG_ORIGIN_BOTTOM (1 << QUAD_FLAG_ORIGIN_SHIFT)
#define QUAD_FLAG_ORIGIN_LEFT   (0 << (QUAD_FLAG_ORIGIN_SHIFT+1))
#define QUAD_FLAG_ORIGIN_RIGHT  (1 << (QUAD_FLAG_ORIGIN_SHIFT+1))
#define QUAD_FLAG_ORIGIN_TL (QUAD_FLAG_ORIGIN_TOP | QUAD_FLAG_ORIGIN_LEFT)
#define QUAD_FLAG_ORIGIN_BL (QUAD_FLAG_ORIGIN_BOTTOM | QUAD_FLAG_ORIGIN_LEFT)
#define QUAD_FLAG_ORIGIN_TR (QUAD_FLAG_ORIGIN_TOP | QUAD_FLAG_ORIGIN_RIGHT)
#define QUAD_FLAG_ORIGIN_BR (QUAD_FLAG_ORIGIN_BOTTOM | QUAD_FLAG_ORIGIN_RIGHT)
#define QUAD_FLAG_ORIGIN_MASK (3 << QUAD_FLAG_ORIGIN_SHIFT)
#define IS_QUAD_FLAG_ORIGIN_TOP(x) (((x) & (1 << QUAD_FLAG_ORIGIN_SHIFT)) == QUAD_FLAG_ORIGIN_TOP)
#define IS_QUAD_FLAG_ORIGIN_BOTTOM(x) (((x) & (1 << QUAD_FLAG_ORIGIN_SHIFT)) == QUAD_FLAG_ORIGIN_BOTTOM)
#define IS_QUAD_FLAG_ORIGIN_LEFT(x) (((x) & (1 << (QUAD_FLAG_ORIGIN_SHIFT+1))) == QUAD_FLAG_ORIGIN_LEFT)
#define IS_QUAD_FLAG_ORIGIN_RIGHT(x) (((x) & (1 << (QUAD_FLAG_ORIGIN_SHIFT+1))) == QUAD_FLAG_ORIGIN_RIGHT)

// The view-projection matrix to convert my video-world space co-ords
// (0-1) into NDC (-1, 1). My view is fixed along the Z axis so it is
// just the identity matrix. So really, this is just the
// projection matrix.
//
// NOTE: Column major order
static const float VideoVPMatrix[16] = {
    2,                      0,                      0,      0,
    0,                      2,                      0,      0,
    0,                      0,                      1,      0,
    -1,                    -1,                      0,      1
};

// --------------------------------------------------------------------
// Global Variables
// --------------------------------------------------------------------
static screen_window_t Window;
static pthread_t RenderThread;
static pthread_mutex_t RenderMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t RenderCond = PTHREAD_COND_INITIALIZER;
static render_state_t RenderState = RENDER_STATE_UNINITIALIZED;
static int LastModelState;

static uint32_t RenderFlags;
static int WindowDims[2]; // Dimensions of the window

// EGL State
static EGLDisplay EglDisp = EGL_NO_DISPLAY;
static EGLContext EglCtx = EGL_NO_CONTEXT;
static EGLSurface EglSurf = EGL_NO_SURFACE;
static EGLConfig EglConfig;

// State for UI rendering
static gl_pipeline_t UiPipeline = {0};
// The view-projection matrix to convert my screen/world space co-ords
// into NDC. My view is fixed along the Z axis so it is
// just the identity matrix. So really, this is just the
// projection matrix.
//
// Some of the values need to be filled in later once the dimensions
// of the screen are known.
//
// NOTE: Column major order
static float UiVPMatrix[16] = {
    /*filled in later*/0,   0,                      0,      0,
    0,                      /*filled in later*/0,   0,      0,
    0,                      0,                      1,      0,
    -1,                     1,                      0,      1
};

// State for Video rendering
static gl_pipeline_t VideoPipeline = {0};
static GLuint YuvVideoTexId = 0;
static GLuint RgbVideoTexId = 1;
static GLint FrameWidthUniform;

// A pending video frame to process
static video_frame_t *NewVideoFrame = NULL;

cv::Mat NewRgbFrame;
cv::Mat MenuImage;

// A pending touch event to process
static int NewFaceDetection = 0;
std::vector<std::array<int,4 >> FaceDetection = {};

// The flags passed to the function to create the texture's quad.
static int VideoTextureQuadOrigin = 0;

// For drawing boxes
static float FactorX = 1;
static float FactorY = 1;
static int OffsetX = 0;
static int OffsetY = 0;

static char RenderLogBuf[100] = {0};

// --------------------------------------------------------------------
// Static Functions
// --------------------------------------------------------------------
// Load the shader, taken from
// https://www.khronos.org/assets/uploads/books/openglr_es_20_programming_guide_sample.pdf
static GLuint load_shader(GLenum type, const char *src) {
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if(shader == 0) {
        sprintf(RenderLogBuf, "Unable to create shader!\n");
        ERROR() << RenderLogBuf << FLUSH;
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);

    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if(!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
            char* infoLog = static_cast<char*>(malloc(sizeof(char) * infoLen));
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            sprintf(RenderLogBuf, "Compilation error:\n%s\n", infoLog);
            ERROR() << RenderLogBuf << FLUSH;
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void destroy_pipeline(gl_pipeline_t *p) {
    if (p != NULL) {
        glDeleteProgram(p->program);
        glDeleteShader(p->fragmentShader);
        glDeleteShader(p->vertexShader);
    }
}

static int create_pipeline(gl_pipeline_t *pipeline, const char *vShader,
                          const char *fShader, gl_attr_def_t *attrs)
{
    struct gl_pipeline p = {0};
    GLint glint = 0;

    // Create the shader program I'm going to use
    p.vertexShader = load_shader(GL_VERTEX_SHADER, vShader);
    if (!p.vertexShader) {
        sprintf(RenderLogBuf, "Unable to load vertex shader\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto failure;
    }
    p.fragmentShader = load_shader(GL_FRAGMENT_SHADER, fShader);
    if (!p.fragmentShader) {
        sprintf(RenderLogBuf, "Unable to load fragment shader\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto failure;
    }
    p.program = glCreateProgram();
    if (!p.program) {
        sprintf(RenderLogBuf, "Unable to create GL program\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto failure;
    }
    glAttachShader(p.program, p.vertexShader);
    glAttachShader(p.program, p.fragmentShader);

    // Bind any attributes to the vertex shader
    while (attrs != NULL && attrs->name != NULL) {
        glBindAttribLocation(p.program, attrs->idx, attrs->name);
        if ((glint = glGetError())) {
            sprintf(RenderLogBuf, "Error binding attribute %s to index/location %d. Error=0x%x\n", attrs->name, attrs->idx, glint);
            ERROR() << RenderLogBuf << FLUSH;
            goto failure;
        }
        attrs++;
    }

    // Link the program
    glLinkProgram(p.program);
    if ((glint = glGetError())) {
        sprintf(RenderLogBuf, "Error linking program. Error=0x%x\n", glint);
        ERROR() << RenderLogBuf << FLUSH;
        goto failure;
    }
    glGetProgramiv(p.program, GL_LINK_STATUS, &glint);
    if (!glint) {
        GLint infoLen = 0;
        glGetProgramiv(p.program, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
            char* infoLog = static_cast<char*>(malloc(sizeof(char) * infoLen));
            glGetProgramInfoLog(p.program, infoLen, NULL, infoLog);
            sprintf(RenderLogBuf, "Linkage error in program:\n%s\n", infoLog);
            ERROR() << RenderLogBuf << FLUSH;
            free(infoLog);
        }
        goto failure;
    }

    // Switch to using this program so I can set its uniforms
    glUseProgram(p.program);
    if ((glint = glGetError())) {
        sprintf(RenderLogBuf, "Error using program. Error = 0x%x\n", glint);
        ERROR() << RenderLogBuf << FLUSH;
        goto failure;
    }

    *pipeline = p;
    return 0;

failure:
    destroy_pipeline(&p);
    return 1;
}

// Assumes the mutex is held and is in correct states
static void do_render_destroy(void) {
    // Before destroying gl/egl state we must tell egl/gl to stop using
    // the state. All the state is bound to the surface so just tell
    // EGL to stop using that surface.
    if (EglSurf != EGL_NO_SURFACE) {
        eglMakeCurrent(EglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }


    if (RenderState != RENDER_STATE_UNINITIALIZED) {
        // Destroy GL state
        glDeleteTextures(1, &YuvVideoTexId);
        YuvVideoTexId = 0;
        glDeleteTextures(1, &RgbVideoTexId);
        RgbVideoTexId = 1;
        destroy_pipeline(&VideoPipeline);
        destroy_pipeline(&UiPipeline);
    }

    // Destroy EGL state
    if (EglSurf != EGL_NO_SURFACE) {
        eglDestroySurface(EglDisp, EglSurf);
        EglSurf = EGL_NO_SURFACE;
    }
    if (EglCtx != EGL_NO_CONTEXT) {
        eglDestroyContext(EglDisp, EglCtx);
        EglCtx = EGL_NO_CONTEXT;
    }
    if (EglDisp != EGL_NO_DISPLAY) {
        eglTerminate(EglDisp);
        EglDisp = EGL_NO_DISPLAY;
    }

    RenderFlags = 0;
    RenderState = RENDER_STATE_UNINITIALIZED;
}

// Assumes the mutex is held and in correct state
static void do_render_stop(void) {
    RenderState = RENDER_STATE_STOPPING;
    pthread_cond_broadcast(&RenderCond);

    // I have to give up the mutex so that the thread can
    // wake up and actually stop. Otherwise the join will deadlock
    pthread_mutex_unlock(&RenderMutex);
    pthread_join(RenderThread, NULL);
    // Relock the mutex as the caller expects that
    pthread_mutex_lock(&RenderMutex);
}

// Populates the 4 points required for a quad of the given width/height.
//
// NOTE: This function assumes it is dealing with pixel co-ords and will
//       subtract 1 from width and height, clamping them to 0.
static void make_quad_u16(point_u16_t *points, uint16_t width, uint16_t height, int flags) {
    uint16_t temp;
    uint16_t w;
    uint16_t h;

    w = width != 0 ? width -1 : 0;
    h = height != 0 ? height -1 : 0;

    // Set up points assuming a winding of CCW
    if (IS_QUAD_FLAG_ORIGIN_LEFT(flags)) {
        points[0].x = 0;
        points[1].x = 0;
        points[2].x = w;
        points[3].x = w;
    } else {
        points[3].x = 0;
        points[2].x = 0;
        points[1].x = w;
        points[0].x = w;
    }

    if (IS_QUAD_FLAG_ORIGIN_TOP(flags)) {
        points[0].y = 0;
        points[1].y = h;
        points[2].y = 0;
        points[3].y = h;
    } else {
        points[1].y = 0;
        points[0].y = h;
        points[3].y = 0;
        points[2].y = h;
    }

    // If winding is instead CW, I need to swap the y values
    if ((flags & QUAD_FLAG_WINDING_MASK) == QUAD_FLAG_WINDING_CW) {
        temp = points[0].y;
        points[0].y = points[1].y;
        points[2].y = points[1].y;
        points[1].y = temp;
        points[3].y = temp;
    }
}

// Populates the 4 points required for a quad of the given width/height.
// NOTE: This function assumes co-ordinates are in a continuous space
//       and hence don't need any adjustment (eg: no subtracting one)
static void make_quad_f(point_f_t *points, float width, float height, int flags) {
    float temp;

    // Set up points assuming a winding of CCW
    if (IS_QUAD_FLAG_ORIGIN_LEFT(flags)) {
        points[0].x = 0;
        points[1].x = 0;
        points[2].x = width;
        points[3].x = width;
    } else {
        points[3].x = 0;
        points[2].x = 0;
        points[1].x = width;
        points[0].x = width;
    }

    if (IS_QUAD_FLAG_ORIGIN_TOP(flags)) {
        points[0].y = 0;
        points[1].y = height;
        points[2].y = 0;
        points[3].y = height;
    } else {
        points[1].y = 0;
        points[0].y = height;
        points[3].y = 0;
        points[2].y = height;
    }

    // If winding is instead CW, I need to swap the y values
    if ((flags & QUAD_FLAG_WINDING_MASK) == QUAD_FLAG_WINDING_CW) {
        temp = points[0].y;
        points[0].y = points[1].y;
        points[2].y = points[1].y;
        points[1].y = temp;
        points[3].y = temp;
    }
}

// --------------------------------------------------------------------
// The render thread's routine
// --------------------------------------------------------------------
static void* render_thread(void *arg) {
    int active;
    EGLContext eglCtx = EGL_NO_CONTEXT;
    int dims[2];
    GLint glint;
    struct timespec tpStart;
    struct timespec tpEnd;
    uint64_t totalTimeUs = 0;
    unsigned renderCount = 0;
    uint64_t duration;
    unsigned minRenderTime = UINT_MAX;
    unsigned maxRenderTime = 0;
    //int firstIdx;
    //int lastIdx;
    //float f;

    point_f_t videoPoints[8];
    point_f_t imagePoints[8];
    video_frame_t *newVf;
    int vfDims[2] = {0, 0};
    int vfDimsChanged;
    uint8_t *vfPtr;
    int vfTextureLoaded = 0;
    cv::Mat newRgb;

    int newFaces;
    std::vector<std::array<int,4 >> faceboxes = {};
    //int boxes[0][4] = {0, 0, 0, 0};
    std::vector<std::array<int,4 >> boxes = {};
    std::vector<std::array<point_u16_t, 12>> uiPoints = {};

    (void)arg;

    sprintf(RenderLogBuf, "Render thread started\n");
    INFO() << RenderLogBuf << FLUSH;

    // EGL contexts are thread specific. The state (and associated info) of a context
    // can be shared across threads, but each thread still needs it own context.
    eglCtx = eglCreateContext(EglDisp, EglConfig, EglCtx, EglCtxAttr);
    if (eglCtx == EGL_NO_CONTEXT) {
        sprintf(RenderLogBuf, "eglCreateContext error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto exit;
    }
    // Make this context current
    if (eglMakeCurrent(EglDisp, EglSurf, EglSurf, eglCtx) != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglMakeCurrent error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto exit;
    }

    // Set up the video points. The first four are the
    // vertices for the quad. The second set of four
    // are the texture's sample points.
    // NOTE:
    //   The input image has the origin at the top-left
    //   Input image co-ords range from (0,0) to (width-1, height-1)
    //
    //   The 'texture' has the origin at the bottom-left
    //   Texture co-ords range from (0.0, 0.0) to (1.0, 1.0)
    //
    // Vertices
    make_quad_f(imagePoints, 0.1014, 1, QUAD_FLAG_WINDING_CCW | QUAD_FLAG_ORIGIN_BR);
    make_quad_f(videoPoints, 1, 1, QUAD_FLAG_WINDING_CCW | QUAD_FLAG_ORIGIN_BL);
    make_quad_f(videoPoints+4, 1, 1, QUAD_FLAG_WINDING_CCW | VideoTextureQuadOrigin);

    while (1) {
        newVf = NULL;
        newFaces = 0;
        vfDimsChanged = 0;
        vfTextureLoaded = 0;
        newRgb.release();
        if (processor_get_model_state() != LastModelState) {
            boxes.clear();
            glUseProgram(VideoPipeline.program);
            LastModelState = processor_get_model_state();
            glint = glGetUniformLocation(VideoPipeline.program, "uFormat");
            if (glint == -1) {
                sprintf(RenderLogBuf, "Unable to get 'uFormat' uniform . Error = %d\n", glGetError());
                ERROR() << RenderLogBuf << FLUSH;
                goto exit;
            }
            if (LastModelState != MODEL_FACE) {
                glUniform1i(glint, 3);
                DEBUG() << "setting to RGB" << FLUSH;
            } else if (RenderFlags & RENDER_FLAG_BGRA) {
                glUniform1i(glint, 0);
            } else if (RenderFlags & RENDER_FLAG_UYVY){
                glUniform1i(glint, 1);
            } else if (RenderFlags & RENDER_FLAG_YUYV){
                glUniform1i(glint, 2);
            } else {
                sprintf(RenderLogBuf, "Unknown video format.\n");
                ERROR() << RenderLogBuf << FLUSH;
                goto exit;
            }
            glint = glGetUniformLocation(VideoPipeline.program, "uTexSampler");
            if (glint == -1) {
                sprintf(RenderLogBuf, "Unable to get 'uTexSampler' uniform . Error = %d\n", glGetError());
                ERROR() << RenderLogBuf << FLUSH;
                goto exit;
            }
            if (LastModelState == MODEL_FACE) {
                glUniform1i(glint, 0);
            } else {
                glUniform1i(glint, 1);
            }
        }
        if ((glint = glGetError())) {
            sprintf(RenderLogBuf, "Error after setting format. Error = 0x%x\n", glint);
            ERROR() << RenderLogBuf << FLUSH;
            break;
        }

        pthread_mutex_lock(&RenderMutex);
        active = RenderState == RENDER_STATE_ACTIVE;
        if (((NewVideoFrame || NewFaceDetection) && LastModelState == MODEL_FACE) || (!NewRgbFrame.empty() && LastModelState != MODEL_FACE)) {
            if (NewVideoFrame && LastModelState == MODEL_FACE /*&& !(RenderFlags & RENDER_FLAG_BGRA)*/) {
                newVf = NewVideoFrame;
            } else if (NewVideoFrame) {
                // dropping frame
                video_frame_release(NewVideoFrame);
            }

            if (!NewRgbFrame.empty() && LastModelState != MODEL_FACE) {
                newRgb = NewRgbFrame;
            }

            if (NewFaceDetection && LastModelState == MODEL_FACE) {
                faceboxes = FaceDetection;
                newFaces = 1;
            }
            NewFaceDetection = 0;
            NewRgbFrame.release();
            NewVideoFrame = NULL;
            FaceDetection.clear();
        } else if (active && renderCount > 0) {
            // Block and wait for something that would change the render.
            // We don't block the first time through the loop so that an initial
            // view is rendered. If we don't do that, the app window (and any child
            // windows owned by it - such as the video window) will remain invisible
            // until something causes a render.
            pthread_cond_wait(&RenderCond, &RenderMutex);
            continue;
        }
        pthread_mutex_unlock(&RenderMutex);

        // Process a new video frame, this has to happen before the active check
        // so that we don't leak next_vf.
        if (newVf) {
            // Check if the vf dimensions have changed. If so I need to do a few things later.
            video_frame_dimensions(newVf, dims);
            if (dims[0] != vfDims[0] || dims[1] != vfDims[1]) {
                vfDims[0] = dims[0];
                vfDims[1] = dims[1];
                vfDimsChanged = 1;
            }
            vfPtr = static_cast<uint8_t*>(video_frame_ptr(newVf));
            if (!vfPtr) {
                sprintf(RenderLogBuf, "Unable to get pointer to video frame: %s\n", strerror(errno));
                ERROR() << RenderLogBuf << FLUSH;
                video_frame_release(newVf);
                break;
            }

            // Upload the new viewfinder as a texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, YuvVideoTexId);
            if (RenderFlags & RENDER_FLAG_BGRA) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vfDims[0], vfDims[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, vfPtr);
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vfDims[0]/2, vfDims[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, vfPtr);
            }
            vfTextureLoaded = 1;
            glFlush();
            glFinish();
            video_frame_release(newVf);
        } else if (!newRgb.empty()) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, RgbVideoTexId);
            //use fast 4-byte alignment (default anyway) if possible
            glPixelStorei(GL_UNPACK_ALIGNMENT, (newRgb.step & 3) ? 1 : 4);

            //set length of one complete row in data (doesn't need to equal image.cols)
            //glPixelStorei(GL_UNPACK_ROW_LENGTH, newRgb.step/newRgb.elemSize());
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newRgb.cols, newRgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, newRgb.data);
            glint = glGetUniformLocation(VideoPipeline.program, "uFormat");
            if (glint == -1) {
                sprintf(RenderLogBuf, "Unable to get 'uFormat' uniform . Error = %d\n", glGetError());
                ERROR() << RenderLogBuf << FLUSH;
                goto exit;
            }
            glUniform1i(glint, 3);
            vfTextureLoaded = 1;
            glFlush();
            glFinish();
            DEBUG() << "rgb texture loaded" << FLUSH;
        }

        // Process a new boxes
        if (newFaces) {
            boxes.resize(faceboxes.size());
            uiPoints.resize(faceboxes.size());
            for (unsigned int i = 0; i < faceboxes.size(); i++) {
                // Figure out new box location and dimensions
                // Randomize the size of the box. It has to be at least big enough for the border, with
                // a maximum size of 1/5th of the screen
                boxes[i][2] = faceboxes[i][2] - faceboxes[i][0];
                boxes[i][3] = faceboxes[i][1] - faceboxes[i][3];
                // Center the box at this location
                boxes[i][0] = faceboxes[i][0];
                boxes[i][1] = faceboxes[i][3];
                char buf[100] = {0};
                sprintf(buf, "Render face bounding box @ (%d,%d) of wxh=(%dx%d)\n", boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3]);
                DEBUG() << buf << FLUSH;

                // Create the various quads. Note that I use CW (instead of CCW) winding because the
                // winding gets inverted during projection for UI elements due to screen's origin being
                // top left, and GL's being bottom left.
                // Vertical border
                make_quad_u16(uiPoints[i].data(), BORDER_THICKNESS, boxes[i][3], QUAD_FLAG_WINDING_CW | QUAD_FLAG_ORIGIN_TL);
                // Horizontal border
                make_quad_u16(uiPoints[i].data()+4, boxes[i][2], BORDER_THICKNESS, QUAD_FLAG_WINDING_CW | QUAD_FLAG_ORIGIN_TL);
            }
        }

        if (!active) break;

        clock_gettime(CLOCK_MONOTONIC, &tpStart);
        // ------------------------------------------------------------
        // Render video
        // ------------------------------------------------------------
        if (vfTextureLoaded) {
            glUseProgram(VideoPipeline.program);
            glDisable(GL_BLEND);

            // If the video frame's dimensions have changed, I need to update some uniforms
            if (vfDimsChanged && LastModelState == MODEL_FACE) {
                glUniform1i(FrameWidthUniform, vfDims[0]-1);
            }

            // Upload the vertices
            glVertexAttribPointer(VSARG_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, videoPoints);
            glEnableVertexAttribArray(VSARG_VERTEX);
            glVertexAttribPointer(VSARG_TEX_LOC, 2, GL_FLOAT, GL_FALSE, 0, videoPoints + 4);
            glEnableVertexAttribArray(VSARG_TEX_LOC);

            // Make sure the texture with the video frame is active
            //if (LastModelState == MODEL_FACE) {
            if (LastModelState == MODEL_FACE) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, YuvVideoTexId);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            } else {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, RgbVideoTexId);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            // Render!
            // Done video
            glDisableVertexAttribArray(VSARG_TEX_LOC);
            glDisableVertexAttribArray(VSARG_VERTEX);
        }
        // ------------------------------------------------------------
        // DONE: Video
        // ------------------------------------------------------------
        if ((glint = glGetError())) {
            sprintf(RenderLogBuf, "Error after video. Error = 0x%x\n", glint);
            ERROR() << RenderLogBuf << FLUSH;
            break;
        }

        // ------------------------------------------------------------
        // Render chrome/ui
        // ------------------------------------------------------------
        glUseProgram(UiPipeline.program);
        // We want to blend with the video frame I rendered above.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Set up the initial aModel matrix to give
        // no changes.
        glVertexAttrib3f(VSARG_MODEL_COL_0, 1, 0, 0);
        glVertexAttrib3f(VSARG_MODEL_COL_1, 0, 1, 0);
        glVertexAttrib3f(VSARG_MODEL_COL_2, 0, 0, 1);

        // Draw boxes

        if (vfTextureLoaded) {
            for (unsigned long i = 0; i < boxes.size(); i++) {
                if (LastModelState == MODEL_FACE) {
                    glActiveTexture(GL_TEXTURE0);
                } else {
                    glActiveTexture(GL_TEXTURE1);
                }

                // Load the vertex data
                glVertexAttribPointer(VSARG_VERTEX, 2, GL_UNSIGNED_SHORT, GL_FALSE, 0, uiPoints[i].data());
                glEnableVertexAttribArray(VSARG_VERTEX);

                // Render borders
                glVertexAttrib4f(VSARG_COLOUR, 0.0, 1.0, 0.0, 1.0);
                // Left border
                glVertexAttrib3f(VSARG_MODEL_COL_2, boxes[i][0], boxes[i][1], 1);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                // Right border
                glVertexAttrib3f(VSARG_MODEL_COL_2, boxes[i][0] + boxes[i][2] - BORDER_THICKNESS, boxes[i][1], 1);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                // Bottom border
                glVertexAttrib3f(VSARG_MODEL_COL_2, boxes[i][0], boxes[i][1], 1);
                glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
                // Top border
                glVertexAttrib3f(VSARG_MODEL_COL_2, boxes[i][0], boxes[i][1] + boxes[i][3] - BORDER_THICKNESS, 1);
                glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
            }
        }
        if (vfTextureLoaded) {
            glUseProgram(VideoPipeline.program);
            glDisable(GL_BLEND);

            // Upload the vertices
            glVertexAttribPointer(VSARG_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, videoPoints);
            glEnableVertexAttribArray(VSARG_VERTEX);
            glVertexAttribPointer(VSARG_TEX_LOC, 2, GL_FLOAT, GL_FALSE, 0, videoPoints + 4);
            glEnableVertexAttribArray(VSARG_TEX_LOC);
            if (!MenuImage.empty()) {
                DEBUG() << "Rendering menu" << FLUSH;
                glBindTexture(GL_TEXTURE_2D, RgbVideoTexId);
                glVertexAttribPointer(VSARG_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, imagePoints);
                glPixelStorei(GL_UNPACK_ALIGNMENT, (MenuImage.step & 3) ? 1 : 4);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MenuImage.cols, MenuImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, MenuImage.data);
                glint = glGetUniformLocation(VideoPipeline.program, "uFormat");
                if (glint == -1) {
                    sprintf(RenderLogBuf, "Unable to get 'uFormat' uniform . Error = %d\n", glGetError());
                    ERROR() << RenderLogBuf << FLUSH;
                    goto exit;
                }
                glUniform1i(glint, 3);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (LastModelState != MODEL_FACE) {
                    glUniform1i(glint, 3);
                } else if (RenderFlags & RENDER_FLAG_BGRA) {
                    glUniform1i(glint, 0);
                } else if (RenderFlags & RENDER_FLAG_UYVY){
                    glUniform1i(glint, 1);
                } else if (RenderFlags & RENDER_FLAG_YUYV){
                    glUniform1i(glint, 2);
                } else {
                    sprintf(RenderLogBuf, "Unknown video format.\n");
                    ERROR() << RenderLogBuf << FLUSH;
                    goto exit;
                }
            }
            // Render!
            // Done video
            glDisableVertexAttribArray(VSARG_TEX_LOC);
            glDisableVertexAttribArray(VSARG_VERTEX);
        }

        // ------------------------------------------------------------
        // DONE: Chrome/UI
        // ------------------------------------------------------------
        if ((glint = glGetError())) {
            sprintf(RenderLogBuf, "Error after ui. Error = 0x%x\n", glint);
            ERROR() << RenderLogBuf << FLUSH;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &tpEnd);

        if (eglSwapBuffers(EglDisp, EglSurf) != EGL_TRUE) {
            sprintf(RenderLogBuf, "eglSwapBuffers error = 0x%x\n", eglGetError());
            ERROR() << RenderLogBuf << FLUSH;
            break;
        }

        duration = (uint64_t)((tpEnd.tv_sec * 1000 * 1000 * 1000 + tpEnd.tv_nsec) - (tpStart.tv_sec * 1000 *1000 *1000 + tpStart.tv_nsec));
        duration = (duration + 500) / (1000);
        if (duration > maxRenderTime) maxRenderTime = duration;
        if (duration < minRenderTime) minRenderTime = duration;
        totalTimeUs += duration;
        renderCount++;
        DEBUG() << "Render took " << duration << "us. Avg=" << (double)totalTimeUs/renderCount << "us Min=" << minRenderTime << "us Max=" << maxRenderTime << "us" << FLUSH;
    }

exit:
    sprintf(RenderLogBuf, "Render thread no longer active. Exiting\n");
    INFO() << RenderLogBuf << FLUSH;

    // Before destroying gl/egl state we must tell egl/gl to stop using
    // the state. All the state is bound to the surface so just tell
    // EGL to stop using that surface.
    if (EglSurf != EGL_NO_SURFACE) {
        eglMakeCurrent(EglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (eglCtx != EGL_NO_CONTEXT) {
        eglDestroyContext(EglDisp, eglCtx);
    }

    pthread_mutex_lock(&RenderMutex);
    RenderState = RENDER_STATE_INITIALIZED;

    return NULL;
}

// --------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------
int render_initialize(screen_window_t scr_win, uint32_t flags) {
    Window = scr_win;
    EGLBoolean eglBool;
    EGLint numConfigs;
    EGLint eglint;
    EGLConfig *configs = NULL;
    int i;
    GLint glint;
    cv::Mat image;
    gl_attr_def_t attrs[] = {
        {"aVertex", VSARG_VERTEX},
        {"aColour", VSARG_COLOUR},
        {"aModel", VSARG_MODEL},
        {NULL, 0}
    };

    pthread_mutex_lock(&RenderMutex);
    if (RenderState != RENDER_STATE_UNINITIALIZED) {
        sprintf(RenderLogBuf, "render is already initialized.\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }

    LastModelState = processor_get_model_state();

    // Get the dimensions of the window.
    if (screen_get_window_property_iv(scr_win, SCREEN_PROPERTY_SIZE, WindowDims)) {
        sprintf(RenderLogBuf, "Unable to get dimensions of window. Error=%s\n", strerror(errno));
        ERROR() << RenderLogBuf << FLUSH;
    }

    // Get a display
    EglDisp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (EglDisp == EGL_NO_DISPLAY) {
        sprintf(RenderLogBuf, "eglGetDisplay error = %d\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    eglBool = eglInitialize(EglDisp, NULL, NULL);
    if (eglBool != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglInitialize error = %d\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }

    // Figure out the configuration. I want the first one that supports RGBA8888.
    eglBool = eglGetConfigs(EglDisp, NULL, 0, &numConfigs);
    if (eglBool != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglGetConfigs#1 error = %d\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    configs = static_cast<void**>(calloc(numConfigs, sizeof(*configs)));
    eglBool = eglGetConfigs(EglDisp, configs, numConfigs, &numConfigs);
    if (eglBool != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglGetConfigs#2 error = %d\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    for (i = 0; i < numConfigs; ++i) {
        // Surface type must be a window
        eglGetConfigAttrib(EglDisp, configs[i], EGL_SURFACE_TYPE, &eglint);
        if ((eglint & EGL_WINDOW_BIT) != EGL_WINDOW_BIT) continue;
        // Render type supports OpenGLESv2
        eglGetConfigAttrib(EglDisp, configs[i], EGL_RENDERABLE_TYPE, &eglint);
        if ((eglint & EGL_OPENGL_ES2_BIT) == 0) continue;
        // Red bit depth
        eglGetConfigAttrib(EglDisp, configs[i], EGL_RED_SIZE, &eglint);
        if (eglint != 8) continue;
        // Green bit depth
        eglGetConfigAttrib(EglDisp, configs[i], EGL_GREEN_SIZE, &eglint);
        if (eglint != 8) continue;
        // Blue bit depth
        eglGetConfigAttrib(EglDisp, configs[i], EGL_BLUE_SIZE, &eglint);
        if (eglint != 8) continue;
        // Alpha bit depth
        eglGetConfigAttrib(EglDisp, configs[i], EGL_ALPHA_SIZE, &eglint);
        if (eglint != 8) continue;

        // Success!
        break;
    }
    if (i == numConfigs) {
        sprintf(RenderLogBuf, "Unable to find egl config for rendering RGBA8888 via OpenGL ES2\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }

    EglConfig = configs[i];
    free(configs);
    configs = NULL;

    // Create the egl context
    EglCtx = eglCreateContext(EglDisp, EglConfig, EGL_NO_CONTEXT, EglCtxAttr);
    if (EglCtx == EGL_NO_CONTEXT) {
        sprintf(RenderLogBuf, "eglCreateContext error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Create the EGL window surface
    EglSurf = eglCreateWindowSurface(EglDisp, EglConfig, reinterpret_cast<EGLNativeWindowType>(scr_win), NULL);
    if (EglSurf == EGL_NO_SURFACE) {
        sprintf(RenderLogBuf, "eglCreateWindowSurface error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Make this window surface current
    eglBool = eglMakeCurrent(EglDisp, EglSurf, EglSurf, EglCtx);
    if (eglBool != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglMakeCurrent error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
#if 0
    // And make sure our swap interval is 1 so that we re-render the chrome
    // every frame.
    rc = eglSwapInterval(EglDisp, 1);
    if (rc != EGL_TRUE) {
        sprintf(RenderLogBuf, "eglSwapInterval error = 0x%x\n", eglGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto exit;
    }
#endif

    // Set up the GL state
    // Load the programs I need
    // UI Pipeline
    if (create_pipeline(&UiPipeline, UiVertexShaderSource, UiFragmentShaderSource, attrs)) {
        sprintf(RenderLogBuf, "Error loading UI pipeline\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Set the UiVPMatrix uniform
    glint = glGetUniformLocation(UiPipeline.program, "uVPMatrix");
    if (glint == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uVPMatrix' uniform. Error = 0x%x\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Set up the two values in the UiVPMatrix that depend on the dimensions of the window
    UiVPMatrix[0] = 2.0/(WindowDims[0]-1);
    UiVPMatrix[5] = -2.0/(WindowDims[1]-1);
    glUniformMatrix4fv(glint, 1, GL_FALSE, UiVPMatrix);

    // Load menu image
    image = cv::imread("styleImages/menu.jpg");
    cvtColor(image, MenuImage, cv::COLOR_BGR2RGB);

    // Video Pipeline
    attrs[1].name = "aTexLoc";
    attrs[1].idx = VSARG_TEX_LOC;
    attrs[2].name = NULL;
    attrs[2].idx = 0;
    if (create_pipeline(&VideoPipeline, VideoVertexShaderSource, VideoFragmentShaderSource, attrs)) {
        sprintf(RenderLogBuf, "Error loading video pipeline\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Video pipeline requires a few more uniforms. Get their indices and set
    // what I can now.
    // Set the VideoVPMatrix uniform
    glint = glGetUniformLocation(VideoPipeline.program, "uVPMatrix");
    if (glint == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uVPMatrix' uniform. Error = 0x%x\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // Set up the two values in the UiVPMatrix that depend on the dimensions of the window
    glUniformMatrix4fv(glint, 1, GL_FALSE, VideoVPMatrix);
    glint = glGetUniformLocation(VideoPipeline.program, "uColourMat");
    if (glint == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uColourMat' uniform . Error = %d\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    glUniformMatrix4fv(glint, 1, GL_FALSE, YuvToRgbMatrix);
    FrameWidthUniform = glGetUniformLocation(VideoPipeline.program, "uFrameWidth");
    if (FrameWidthUniform == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uFrameWidth' uniform . Error = %d\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    // The uniform must be loaded when dimensions are known
    glint = glGetUniformLocation(VideoPipeline.program, "uTexSampler");
    if (glint == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uTexSampler' uniform . Error = %d\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    if (LastModelState == MODEL_FACE) {
        glUniform1i(glint, 0);
    } else {
        glUniform1i(glint, 1);
    }

    glint = glGetUniformLocation(VideoPipeline.program, "uFormat");
    if (glint == -1) {
        sprintf(RenderLogBuf, "Unable to get 'uFormat' uniform . Error = %d\n", glGetError());
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }
    if (LastModelState != MODEL_FACE) {
        glUniform1f(glint, 3);
    } else if (flags & RENDER_FLAG_BGRA) {
        glUniform1i(glint, 0);
    } else if (flags & RENDER_FLAG_UYVY){
        glUniform1i(glint, 1);
    } else if (flags & RENDER_FLAG_YUYV){
        glUniform1i(glint, 2);
    } else {
        sprintf(RenderLogBuf, "Unknown video format.\n");
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }

    // Create and configure the texture object that will hold the video frame
    glGenTextures(1, &YuvVideoTexId);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, YuvVideoTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &RgbVideoTexId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, RgbVideoTexId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S , GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // For texture sample points, gl assumes an origin at bottom-left
    // but the video frame has a top-left origin. This causes the
    // video to appear flipped.
    VideoTextureQuadOrigin = QUAD_FLAG_ORIGIN_TOP | QUAD_FLAG_ORIGIN_RIGHT;

    // Do a final sanity check that GL is set up properly
    if ((glint = glGetError())) {
        sprintf(RenderLogBuf, "Error setting up initial GL state. Error=0x%x\n", glint);
        ERROR() << RenderLogBuf << FLUSH;
        goto fail;
    }

    // Unbind the current context. This is required so that the rendering thread
    // can bind its context to the same surface.
    eglMakeCurrent(EglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    RenderFlags = flags;
    RenderState = RENDER_STATE_INITIALIZED;
    pthread_mutex_unlock(&RenderMutex);
    return 0;

fail:
    free(configs);
    do_render_destroy();
    pthread_mutex_unlock(&RenderMutex);
    return -1;
}

void render_destroy(void) {
    pthread_mutex_lock(&RenderMutex);
    if (RenderState == RENDER_STATE_ACTIVE) {
        do_render_stop();
    }
    do_render_destroy();
    pthread_mutex_unlock(&RenderMutex);
}

int render_start(void) {
    int result = 0;

    pthread_mutex_lock(&RenderMutex);
    if (RenderState == RENDER_STATE_ACTIVE) {
        sprintf(RenderLogBuf, "render is already running\n");
        WARNING() << RenderLogBuf << FLUSH;
        goto exit;
    } else if (RenderState != RENDER_STATE_INITIALIZED) {
        sprintf(RenderLogBuf, "render is not in initialized state.\n");
        ERROR() << RenderLogBuf << FLUSH;
        result = -1;
        goto exit;
    }

    // Spin up the render thread.
    pthread_create(&RenderThread, NULL, render_thread, NULL);
    pthread_setname_np(RenderThread, "render");
    RenderState = RENDER_STATE_ACTIVE;

exit:
    pthread_mutex_unlock(&RenderMutex);
    return result;
}

void render_stop() {
    pthread_mutex_lock(&RenderMutex);
    if (RenderState == RENDER_STATE_ACTIVE) {
        do_render_stop();
    } else if (RenderState == RENDER_STATE_INITIALIZED) {
        sprintf(RenderLogBuf, "render is already stopped\n");
        WARNING() << RenderLogBuf << FLUSH;
    } else {
        sprintf(RenderLogBuf, "render is not running\n");
        ERROR() << RenderLogBuf << FLUSH;
    }

    pthread_mutex_unlock(&RenderMutex);
}

void render_new_video_frame(video_frame_t *vf) {
    int format;

    // Make sure the format is one I understand.
    if ((format = video_frame_format(vf)) >= 0) {
        if (format == SCREEN_FORMAT_UYVY || format == SCREEN_FORMAT_YUY2 || format == SCREEN_FORMAT_BGRX8888) {
            pthread_mutex_lock(&RenderMutex);
            if (RenderState == RENDER_STATE_ACTIVE) {
                DEBUG() << "got new frame to render" << FLUSH;
                NewVideoFrame = video_frame_replace(NewVideoFrame, vf);
                pthread_cond_signal(&RenderCond);
            } else {
                WARNING() << "Renderer not active. Ignoring new video frame." << FLUSH;
            }
            pthread_mutex_unlock(&RenderMutex);
        } else {
            ERROR() << "Video frame format " << format << " is NOT UYVY or YUY2!, Ignoring" << FLUSH;
        }
    } else {
        ERROR() << "Error getting video frame format:" << strerror(errno) << FLUSH;
    }
}

void render_new_rgb_frame(cv::Mat data) {
    pthread_mutex_lock(&RenderMutex);
    NewRgbFrame = data;
    pthread_cond_signal(&RenderCond);
    pthread_mutex_unlock(&RenderMutex);
}

void render_new_face_detection(nlohmann::json& data, int width, int height) {
    if (LastModelState != MODEL_FACE) return;
    pthread_mutex_lock(&RenderMutex);
    FaceDetection.resize(data["detections"].size());
    for (unsigned long i = 0; i < data["detections"].size(); i++) {
        FaceDetection[i][2] = WindowDims[0] - static_cast<int>(data["detections"][i]["box"]["left"]) * WindowDims[0] / width;
        FaceDetection[i][1] = static_cast<int>(data["detections"][i]["box"]["bottom"]) * WindowDims[1] / height;
        FaceDetection[i][0] = WindowDims[0] - static_cast<int>(data["detections"][i]["box"]["right"]) * WindowDims[0] / width;
        FaceDetection[i][3] = static_cast<int>(data["detections"][i]["box"]["top"]) * WindowDims[1] / height;
        FaceDetection[i][0] = FaceDetection[i][0] * FactorX + OffsetX;
        FaceDetection[i][1] = FaceDetection[i][1] * FactorY + OffsetY;
        FaceDetection[i][2] = FaceDetection[i][2] * FactorX + OffsetX;
        FaceDetection[i][3] = FaceDetection[i][3] * FactorY + OffsetY;
    }
    NewFaceDetection = 1;
    pthread_cond_signal(&RenderCond);
    pthread_mutex_unlock(&RenderMutex);
}

void render_set_x_scale(float factor) {
    FactorX = factor;
}

void render_set_y_scale(float factor) {
    FactorY = factor;
}

void render_set_x_offset(int offset) {
    OffsetX = offset;
}

void render_set_y_offset(int offset) {
    OffsetY = offset;
}
