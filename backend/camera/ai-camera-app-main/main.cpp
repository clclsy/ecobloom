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

#include <camera/camera_api.h>
#include <ctype.h>
#include <errno.h>
#include <Logging.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#include <pthread.h>
#include <screen/screen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "Global.h"
#include "Processor.h"
#include "Render.h"
#include "Screenlistener.h"
#include "Video.h"

static const char *IdStr = "face-detection-qnxe";
static int CameraHandle = CAMERA_HANDLE_INVALID;
static unsigned int CameraIndex = 0;

static screen_context_t ScreenCtx = NULL;

// The application window. This is the top of the window hierarchy.
// This window will contain at least the UI elements we want to draw
// and the video.
static screen_window_t AppWindow = NULL;

static pthread_mutex_t AppMutex;
static pthread_cond_t AppCond;

static unsigned int FrameWidth;
static unsigned int FrameHeight;
static camera_frametype_t FrameFormat;

static char MainLogBuf[100] = {0};

static void on_quit_requested(int signo)
{
    if (signo == SIGHUP
        || signo == SIGABRT
        || signo == SIGTERM
        || signo == SIGQUIT
        || signo == SIGINT) {
        DEBUG() << "Quit signal received" << FLUSH;

        int rc = pthread_cond_signal(&AppCond) ;
        if (rc != EOK) {
            sprintf(MainLogBuf, "pthread_cond_signal failed: %d", rc);
            ERROR() << MainLogBuf << FLUSH;
        }
    }
}

int initialize_camera(void)
{
    // Connect to the camera and set the viewfinder mode
    int rc;
    if (CameraHandle == CAMERA_HANDLE_INVALID) {
        int cameraRetries = 0;
        camera_unit_t unit;
        uint32_t numSupported;
        camera_unit_t* supportedCameras;

        do {
            // Get how many cameras are supported
            rc = camera_get_supported_cameras(0, &numSupported, NULL);
            if (rc != CAMERA_EOK) {
                sprintf(MainLogBuf, "Failed to get number of supported cameras: rc = %d", rc);
                ERROR() << MainLogBuf << FLUSH;
            }
            if (numSupported > 0) break;
            cameraRetries++;
            sleep(1);
        }
        while (numSupported <= 0 && cameraRetries < MAX_RETRIES);

        if (numSupported <= 0) {
            sprintf(MainLogBuf, "Fail: No cameras available");
            ERROR() << MainLogBuf << FLUSH;
            return EINVAL;
        }

        if (CameraIndex < 0 || CameraIndex >= numSupported) {
            sprintf(MainLogBuf, "Fail: Requested camera index is invalid: %d. Valid range is (0-%d).", CameraIndex, numSupported-1);
            ERROR() << MainLogBuf << FLUSH;
            return EINVAL;
        }

        sprintf(MainLogBuf, "Found %d cameras.", numSupported);

        // Allocate an array big enough to hold unit of all cameras
        supportedCameras = (camera_unit_t*) malloc(sizeof(camera_unit_t) * numSupported);
        if (supportedCameras == NULL) {
            sprintf(MainLogBuf, "Failed to allocate memory for supported cameras.");
            ERROR() << MainLogBuf << FLUSH;
            return ENOMEM;
        }
        rc = camera_get_supported_cameras(numSupported, &numSupported, supportedCameras);
        if (rc != CAMERA_EOK) {
            sprintf(MainLogBuf, "Failed to get list of supported cameras: rc = %d", rc);
            ERROR() << MainLogBuf << FLUSH;
            free(supportedCameras);
            return rc;
        }

        unit = supportedCameras[CameraIndex];
        free(supportedCameras);

        rc = camera_open(unit, CAMERA_MODE_RW, &CameraHandle);
        if (rc != CAMERA_EOK) {
            sprintf(MainLogBuf, "camera_open failed: rc = %d", rc);
            ERROR() << MainLogBuf << FLUSH;
            return rc;
        }
    }


    rc = camera_get_vf_property(CameraHandle, CAMERA_IMGPROP_FORMAT, &FrameFormat);
    if (rc != CAMERA_EOK) {
        sprintf(MainLogBuf, "getting camera frame type failed, rc = %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        return rc;
    }

    if (FrameFormat != CAMERA_FRAMETYPE_YCBYCR && FrameFormat != CAMERA_FRAMETYPE_BGR8888 && FrameFormat != CAMERA_FRAMETYPE_CBYCRY) {
        // Default frame type of camera not supported, checking if camera supports supported frame types
        uint32_t numOfFrameTypes;
        rc = camera_get_supported_vf_frame_types(CameraHandle, 0, &numOfFrameTypes, NULL);
        if (rc != CAMERA_EOK) {
            sprintf(MainLogBuf, "first get frametypes failed, rc = %d", rc);
            ERROR() << MainLogBuf << FLUSH;
            return rc;
        }
        camera_frametype_t frameTypes[numOfFrameTypes] = {};
        uint32_t dummy;
        rc = camera_get_supported_vf_frame_types(CameraHandle, numOfFrameTypes, &dummy, frameTypes);
        if (rc != CAMERA_EOK) {
            sprintf(MainLogBuf, "second get frametypes failed, rc = %d", rc);
            ERROR() << MainLogBuf << FLUSH;
            return rc;
        }
        camera_frametype_t newFrameFormat;
        if (std::find(frameTypes, frameTypes + numOfFrameTypes, CAMERA_FRAMETYPE_YCBYCR) != frameTypes + numOfFrameTypes) {
            newFrameFormat = CAMERA_FRAMETYPE_YCBYCR;
            rc = camera_set_vf_property(CameraHandle, CAMERA_IMGPROP_FORMAT, &newFrameFormat);
            if (rc != CAMERA_EOK) {
                sprintf(MainLogBuf, "setting camera frametype failed, rc = %d", rc);
                ERROR() << MainLogBuf << FLUSH;
                return rc;
            }
        } else if (std::find(frameTypes, frameTypes + numOfFrameTypes, CAMERA_FRAMETYPE_CBYCRY) != frameTypes + numOfFrameTypes) {
            newFrameFormat = CAMERA_FRAMETYPE_CBYCRY;
            rc = camera_set_vf_property(CameraHandle, CAMERA_IMGPROP_FORMAT, &newFrameFormat);
            if (rc != CAMERA_EOK) {
                sprintf(MainLogBuf, "setting camera frametype failed, rc = %d", rc);
                ERROR() << MainLogBuf << FLUSH;
                return rc;
            }
        } else if (std::find(frameTypes, frameTypes + numOfFrameTypes, CAMERA_FRAMETYPE_CBYCRY) != frameTypes + numOfFrameTypes) {
            newFrameFormat = CAMERA_FRAMETYPE_CBYCRY;
            rc = camera_set_vf_property(CameraHandle, CAMERA_IMGPROP_FORMAT, &newFrameFormat);
            if (rc != CAMERA_EOK) {
                sprintf(MainLogBuf, "setting camera frametype failed, rc = %d", rc);
                ERROR() << MainLogBuf << FLUSH;
                return rc;
            }
        }
    }

    rc = camera_get_vf_property(CameraHandle, CAMERA_IMGPROP_WIDTH, &FrameWidth, CAMERA_IMGPROP_HEIGHT, &FrameHeight);
    if (rc != CAMERA_EOK) {
        sprintf(MainLogBuf, "getting camera frame dimensions failed: rc = %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        return rc;
    }
    return EOK;
}

int start_camera(void)
{
    int rc;

    rc = camera_set_buffer_retrieval_mode(CameraHandle, CAMERA_BRM_LATEST_FLUSH);
    if (rc != CAMERA_EOK) {
        sprintf(MainLogBuf, "camera_set_buffer_retrieval_mode failed: rc = %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        return rc;
    }

    // We are doing our own compositing so we don't want camera to create a window.
    rc = camera_set_vf_property(CameraHandle, CAMERA_IMGPROP_CREATEWINDOW, 0);
    if (rc != CAMERA_EOK) {
        sprintf(MainLogBuf, "Unable to disable camera window. rc = %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        return rc;
    }

    video_acquire_from_camera_event(CameraHandle);
    if (rc != CAMERA_EOK) {
        sprintf(MainLogBuf, "camera acquisition failed, rc = %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        return rc;
    }

    return EOK;
}

int stop_camera(void)
{
    video_stop_acquisition();
    return EOK;
}

int main(int argc, char **argv)
{
    int i, rc, order, rval = EXIT_FAILURE;
    struct timespec tp;
    int usage = 0;
    int format = 0;
    std::vector<size_t> shapeMap;
    rc = pthread_setname_np(pthread_self(), "main");
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_setname_np failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Make random things (somewhat) random
    clock_gettime(CLOCK_MONOTONIC, &tp);
    srandom(tp.tv_sec);

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-cameraindex=", strlen("-cameraindex="))){
            if (strlen(argv[i] + strlen("-cameraindex=")) < 3) { //Assume max 99 cameras
                CameraIndex = atoi(argv[i] + strlen("-cameraindex="));
            } else {
                sprintf(MainLogBuf, "cameraindex is too large. Must be less than 100.");
                ERROR() << MainLogBuf << FLUSH;
                std::cout << MainLogBuf << std::endl;
                goto fail;
            }
        } else {
            printf("W Ignoring invalid option: %s", argv[i]);
        }
    }

    // Get camera info, we will need frame size for setting up the windows
    INFO() << "Initializing camera." << FLUSH;
    do {
        rc = initialize_camera();
        if (rc == EINVAL || rc == ENOMEM) {
            goto fail;
        } else if (rc) {
            sprintf(MainLogBuf, "initialize_camera failed. Retrying in 5 seconds.");
            ERROR() << MainLogBuf << FLUSH;
            sleep(5);
        }
    } while(rc != 0);

    INFO() << "Setting up the window." << FLUSH;
    rc = screen_create_context(&ScreenCtx, 0);
    if (rc) {
        sprintf(MainLogBuf, "screen_create_context failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Create top level application window
    rc = screen_create_window_type(&AppWindow, ScreenCtx, SCREEN_APPLICATION_WINDOW);
    if (rc) {
        sprintf(MainLogBuf, "screen_create_window_from_class failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Set the zorder to be above 0 so it is not covered by the splash screen before there is a window manager.
    order = 1;
    rc = screen_set_window_property_iv(AppWindow, SCREEN_PROPERTY_ZORDER, &order);
    if (rc) {
        sprintf(MainLogBuf, "setting window zorder property failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Set the id string so other apps can recognize the window for grouping
    rc = screen_set_window_property_cv(AppWindow, SCREEN_PROPERTY_ID_STRING, strlen(IdStr), IdStr);
    if (rc) {
        sprintf(MainLogBuf, "setting window ID property failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Set a pixel format
    format = SCREEN_FORMAT_RGBX8888;
    rc = screen_set_window_property_iv(AppWindow, SCREEN_PROPERTY_FORMAT, &format);
    if (rc) {
        sprintf(MainLogBuf, "setting window format property failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Set the window for OpenGL usage
    usage = SCREEN_USAGE_OPENGL_ES2;
    rc = screen_set_window_property_iv(AppWindow, SCREEN_PROPERTY_USAGE, &usage);
    if (rc) {
        sprintf(MainLogBuf, "setting window usage property failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Create its buffers
    rc = screen_create_window_buffers(AppWindow, 2);
    if (rc) {
        sprintf(MainLogBuf, "screen_create_window_buffers failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    int dims[2];
    if (screen_get_window_property_iv(AppWindow, SCREEN_PROPERTY_BUFFER_SIZE, dims)) {
        sprintf(MainLogBuf, "Failed to get main window's size, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }


    screen_flush_context(ScreenCtx, SCREEN_WAIT_IDLE);
    // Start monitoring screen for events.
    // Events will tell us when the video stream becomes available
    // Events will tell us when a child window joins the group
    // Events will tell us when the video stream goes away
    // Events will tell us when a touch is detected
    rc = screen_listener_start(ScreenCtx, AppWindow);
    if (rc) {
        sprintf(MainLogBuf, "screen_listener_start failed, %s", strerror(errno));
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Initialize renderer
    INFO() << "Initializing renderer." << FLUSH;
    uint32_t pixelFlag;
    if (FrameFormat == CAMERA_FRAMETYPE_YCBYCR) {
        pixelFlag = RENDER_FLAG_YUYV;
        printf("frame format = yuyv\n\n\n");
    } else if (FrameFormat == CAMERA_FRAMETYPE_BGR8888) {
        pixelFlag = RENDER_FLAG_BGRA;
        printf("frame format = bgra\n\n\n");
    } else if (FrameFormat == CAMERA_FRAMETYPE_CBYCRY) {
        pixelFlag = RENDER_FLAG_UYVY;
        printf("frame format = uyvy\n\n\n");
    } else {
        sprintf(MainLogBuf, "Unsupported frame format %d", FrameFormat);
        std::cout << MainLogBuf << std::endl;
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }
    rc = render_initialize(AppWindow, pixelFlag);

    if (rc) {
        sprintf(MainLogBuf, "Unable to initialize rendering engine");
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Initialize processor
    INFO() << "Initializing processor." << FLUSH;
    if (processor_initialize()) {
        sprintf(MainLogBuf, "Unable to initialize processing engine");
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Start processor and renderer
    INFO() << "Starting renderer, processor, and camera." << FLUSH;
    if (render_start()) {
        sprintf(MainLogBuf, "Unable to start rendering engine");
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }
    if (processor_start()) {
        sprintf(MainLogBuf, "Unable to start processing engine");
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    // Finally start up the camera
    do {
        rc = start_camera();
        if (rc == EINVAL || rc == ENOMEM) {
            goto fail;
        } else if (rc) {
            sprintf(MainLogBuf, "start_camera failed. Retrying in 5 seconds.");
            ERROR() << MainLogBuf << FLUSH;
            sleep(5);
        }
    } while(rc != 0);
    // Everything has started. Drop console logging if we aren't keeping it
    //if (!logStdout) LogConfig.sinks &= ~LOG_SINK_CONSOLE;

    rc = pthread_mutex_init(&AppMutex, NULL);
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_mutex_init failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }
    rc = pthread_cond_init(&AppCond, NULL);
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_cond_init failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    signal(SIGHUP, on_quit_requested);
    signal(SIGABRT, on_quit_requested);
    signal(SIGTERM, on_quit_requested);
    signal(SIGQUIT, on_quit_requested);
    signal(SIGINT, on_quit_requested);

    rc = pthread_mutex_lock(&AppMutex);
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_mutex_lock failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }
    printf("N Waiting for quit signal...\n");
    rc = pthread_cond_wait(&AppCond, &AppMutex);
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_cond_wait failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }
    printf("N Quit requested...\n");
    rc = pthread_mutex_unlock(&AppMutex);
    if (rc != EOK) {
        sprintf(MainLogBuf, "pthread_mutex_unlock failed: %d", rc);
        ERROR() << MainLogBuf << FLUSH;
        goto fail;
    }

    signal(SIGHUP, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    rval = EXIT_SUCCESS;

fail:
    processor_destroy();
    render_destroy();

    stop_camera();
    if (CameraHandle != CAMERA_HANDLE_INVALID) {
        camera_close(CameraHandle);
    }

    if (AppWindow != NULL) {
        screen_destroy_window(AppWindow);
    }
    if (ScreenCtx != NULL) {
        screen_destroy_context(ScreenCtx);
    }

    screen_listener_stop();

    return rval;
}

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif
