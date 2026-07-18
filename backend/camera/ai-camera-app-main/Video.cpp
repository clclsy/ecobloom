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
#include <Logging.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#include <pthread.h>
#include <screen/screen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <time.h>

#include "Global.h"
#include "Processor.h"
#include "Render.h"
#include "Video.h"

#define VIDEO_PULSE_STATUS_AVAILABLE    (_PULSE_CODE_MINAVAIL + 0)
#define VIDEO_PULSE_BUFFER_AVAILABLE    (_PULSE_CODE_MINAVAIL + 1)
#define VIDEO_PULSE_STOP                (_PULSE_CODE_MINAVAIL + 2)

// --------------------------------------------------------------------
// Types
// --------------------------------------------------------------------
typedef enum {
    VIDEO_CONFIG_STOPPED = 0,
    VIDEO_CONFIG_STARTING,
    VIDEO_CONFIG_ACTIVE,
    VIDEO_CONFIG_STOPPING
} video_config_state_t;

struct video_srcCamera {
    int chid;
    int coid;
    camera_eventkey_t bufferKey;
    camera_eventkey_t statusKey;
};
typedef struct video_srcCamera video_srcCamera_t;

struct video_config {
    video_config_state_t state;
    video_srcCamera_t srcCamera;
    // References to the config.
    int refCount;
    int cameraHandle;
    int threadValid;
    pthread_t thread;
};
typedef struct video_config video_config_t;

struct video_frame {
    video_config_t *cfg;
    union {
        screen_buffer_t sbuf;
        camera_buffer_t cbuf;
    };
    int refCount;
    uint64_t id;
};


// --------------------------------------------------------------------
// Global Variables
// --------------------------------------------------------------------
static pthread_mutex_t VideoMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t VideoCond = PTHREAD_COND_INITIALIZER;
static video_config_t VideoCfg;
static char VideoLogBuf[100] = {0};

// --------------------------------------------------------------------
// Static Functions
// --------------------------------------------------------------------
// Assumes lock is held
static void locked_video_frame_release(video_frame_t *vf) {
    if (--vf->refCount <= 0) {
        // Buffer is no longer in use. Release it.
        camera_return_buffer(vf->cfg->cameraHandle, &vf->cbuf);

        // And release the reference it held on the configuration
        if (--vf->cfg->refCount <= 0) {
            pthread_cond_broadcast(&VideoCond);
        }
        free(vf);
    }
}

static void sink_frame(video_frame_t *vf) {
    // Distribute the frame to interested sinks
    DEBUG() << "sinking frame" << FLUSH;
    if (processor_get_model_state() == MODEL_FACE) {
        render_new_video_frame(vf);
    } else {
        processor_new_video_frame(vf);
    }

    // No longer need a reference to this frame as the sinks would have
    // added their own reference if required.
    video_frame_release(vf);
}

// Assumes lock is held
static void locked_stop_acquisition_camera_event(video_config_t *cfg) {
    int rc;

    // Notable points about this method of acquisition:
    // 1. Buffer availability is notified through an event. I've choosen
    //    a pulse as the form of notification.
    // 2. The pulse kicks a worker thread that then acquires the next
    //    buffer through a function call.
    // 3. Buffers are valid until either they are explicitly released OR
    //    the viewfinder is stopped.
    // 5. A specific worker thread is used to call the blocking function call
    //
    // The upswing is that I'm unable to actually stop the viewfinder
    // until I'm sure that there are no buffers in the system.

    // Tell the worker thread to shut down. This way no new buffers
    // will be received.
    rc = MsgSendPulse(cfg->srcCamera.coid, -1, VIDEO_PULSE_STOP, 0);
    if (rc) {
        rc = errno;
        WARNING() << "Unable to send shutdown pulse. Error=" << strerror(rc) << FLUSH;
        // Don't treat this as an error as the thread should unblock
        // when I start killing the connection and channel.
    }

    // Unlock the mutex. This way the worker thread will be able
    // to shutdown.
    pthread_mutex_unlock(&VideoMutex);

    // Join with the worker thread
    if (cfg->threadValid) pthread_join(cfg->thread, NULL);

    // Worker has been safely stopped. Now I need to wait until all
    // references to the config have been released. Starting with my own.
    pthread_mutex_lock(&VideoMutex);
    cfg->refCount--;
    while (cfg->refCount > 0) {
        DEBUG() << "There are still " << cfg->refCount << " references on the video config" << FLUSH;
        pthread_cond_wait(&VideoCond, &VideoMutex);
    }

    // Nothing is referencing the config, and hence there are no buffers
    // being used. It is safe to actually stop the viewfinder.
    NOTICE() << "Stopping viewfinder." << FLUSH;
    rc = camera_stop_viewfinder(cfg->cameraHandle);
    if (rc) {
        ERROR() << "camera_stop_viewfinder failed, " << strerror(rc) << FLUSH;
    }

    // camera_stop_viewfinder automatically disables the events however
    // I've found that if I don't explicitly disable them the key values
    // aren't reused. I'm worried this means there is a fixed amount
    // of them and so, it is better to explicitly disable them.
    camera_disable_event(cfg->cameraHandle, cfg->srcCamera.statusKey);
    camera_disable_event(cfg->cameraHandle, cfg->srcCamera.bufferKey);

    // Safe to now destroy the connection as neither these video
    // APIs or camera APIs will be trying to send pulses to it.
    ConnectDetach(cfg->srcCamera.chid);
    ChannelDestroy(cfg->srcCamera.coid);

    memset(cfg, 0, sizeof(*cfg));
    cfg->state = VIDEO_CONFIG_STOPPED;
    pthread_mutex_unlock(&VideoMutex);
}

static void* event_acquisition_thread(void *arg) {
    video_config_t *cfg = (video_config_t*)arg;
    int rc;
    camera_buffer_t cbuf;
    struct _pulse pulse;
    video_frame_t *vf = NULL;
    int active;
    camera_devstatus_t status;
    uint16_t extra;

    NOTICE() << "Camera event acquisition thread started. cfg=" << cfg << FLUSH;

    while (1) {
        if (MsgReceivePulse(cfg->srcCamera.chid, &pulse, sizeof(pulse), NULL)) {
            ERROR() << "MsgReceivePulse failed. Error=" << strerror(errno) << FLUSH;
            break;
        }

        if (pulse.code == VIDEO_PULSE_BUFFER_AVAILABLE) {
            vf = static_cast<video_frame_t*>(calloc(1, sizeof(*vf)));
            if (!vf) {
                ERROR() << "Unable to allocate video_frame" << FLUSH;
                break;
            }

            rc = camera_get_viewfinder_buffers(cfg->cameraHandle, cfg->srcCamera.bufferKey, &cbuf, NULL);
            if (rc) {
                ERROR() << "Unable to get viewfinder buffer. Error=" << strerror(rc) << FLUSH;
                free(vf);
                continue;
            }

            vf->cfg = cfg;
            vf->cbuf = cbuf;
            vf->refCount = 1;
            vf->id = (uint64_t)cbuf.frametimestamp;

            // Lock the config and see if it is still active.
            // If it is, increase the ref count on it to account
            // for this new buffer.
            pthread_mutex_lock(&VideoMutex);
            active = vf->cfg->state == VIDEO_CONFIG_ACTIVE;
            if (active) {
                vf->cfg->refCount++;
            }
            pthread_mutex_unlock(&VideoMutex);

            if (active) {
                sink_frame(vf);
            } else {
                // No longer active. Return the buffer
                DEBUG() << "Dropping new viewfinder buffer as video config is inactive" << FLUSH;
                camera_return_buffer(cfg->cameraHandle, &cbuf);
                free(vf);
            }

        } else if (pulse.code == VIDEO_PULSE_STATUS_AVAILABLE) {
            extra = 0;
            rc = camera_get_status_details(cfg->cameraHandle, pulse.value, &status, &extra);
            if (rc) {
                ERROR() << "Unable to get status. Error=" << strerror(rc) << FLUSH;
                break;
            }
            DEBUG() << "Camera status callback status=" << status <<  "extra=0x" << extra << FLUSH;

        } else if (pulse.code == VIDEO_PULSE_STOP) {
            break;
        } else {
            WARNING() << "Ignoring unknown pulse. Code=" << pulse.code << FLUSH;
        }
    }

    INFO() << "Camera event acquisition thread no longer active. Exiting" << FLUSH;
    return NULL;
}

// --------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------
void video_frame_release(video_frame_t *vf) {
    if (vf) {
        pthread_mutex_lock(&VideoMutex);
        locked_video_frame_release(vf);
        pthread_mutex_unlock(&VideoMutex);
    }
}

void video_frame_acquire(video_frame_t *vf) {
    pthread_mutex_lock(&VideoMutex);
    vf->refCount++;
    pthread_mutex_unlock(&VideoMutex);
}

video_frame_t* video_frame_replace(video_frame_t *oldVf, video_frame_t *newVf) {
    pthread_mutex_lock(&VideoMutex);
    if (oldVf) {
        locked_video_frame_release(oldVf);
    }
    if (newVf) {
        newVf->refCount++;
    }
    pthread_mutex_unlock(&VideoMutex);

    return newVf;
}

uint64_t video_frame_id(video_frame_t *vf) {
    return vf->id;
}

int video_frame_width(video_frame_t *vf) {
    int result;

    switch (vf->cbuf.frametype) {
        case CAMERA_FRAMETYPE_BGR8888:
            result = vf->cbuf.framedesc.bgr8888.width;
            break;
        case CAMERA_FRAMETYPE_RGB8888:
            result = vf->cbuf.framedesc.rgb8888.width;
            break;
        case CAMERA_FRAMETYPE_RGB888:
            result = vf->cbuf.framedesc.rgb888.width;
            break;
        case CAMERA_FRAMETYPE_GRAY8:
            result = vf->cbuf.framedesc.gray8.width;
            break;
        case CAMERA_FRAMETYPE_CBYCRY:
            result = vf->cbuf.framedesc.cbycry.width;
            break;
        case CAMERA_FRAMETYPE_RGB565:
            result = vf->cbuf.framedesc.rgb565.width;
            break;
        case CAMERA_FRAMETYPE_YCBYCR:
            result = vf->cbuf.framedesc.ycbycr.width;
            break;
        case CAMERA_FRAMETYPE_YCRYCB:
            result = vf->cbuf.framedesc.ycrycb.width;
            break;
        case CAMERA_FRAMETYPE_CRYCBY:
            result = vf->cbuf.framedesc.crycby.width;
            break;
        default:
            result = -1;
            break;
    }

    return result;
}

int video_frame_height(video_frame_t *vf) {
    int result;

    switch (vf->cbuf.frametype) {
        case CAMERA_FRAMETYPE_BGR8888:
            result = vf->cbuf.framedesc.bgr8888.height;
            break;
        case CAMERA_FRAMETYPE_RGB8888:
            result = vf->cbuf.framedesc.rgb8888.height;
            break;
        case CAMERA_FRAMETYPE_RGB888:
            result = vf->cbuf.framedesc.rgb888.height;
            break;
        case CAMERA_FRAMETYPE_GRAY8:
            result = vf->cbuf.framedesc.gray8.height;
            break;
        case CAMERA_FRAMETYPE_CBYCRY:
            result = vf->cbuf.framedesc.cbycry.height;
            break;
        case CAMERA_FRAMETYPE_RGB565:
            result = vf->cbuf.framedesc.rgb565.height;
            break;
        case CAMERA_FRAMETYPE_YCBYCR:
            result = vf->cbuf.framedesc.ycbycr.height;
            break;
        case CAMERA_FRAMETYPE_YCRYCB:
            result = vf->cbuf.framedesc.ycrycb.height;
            break;
        case CAMERA_FRAMETYPE_CRYCBY:
            result = vf->cbuf.framedesc.crycby.height;
            break;
        default:
            result = -1;
            break;
    }

    return result;
}

int video_frame_dimensions(video_frame_t *vf, int *dims) {
    int result = 0;
    switch (vf->cbuf.frametype) {
        case CAMERA_FRAMETYPE_BGR8888:
            dims[0] = vf->cbuf.framedesc.bgr8888.width;
            dims[1] = vf->cbuf.framedesc.bgr8888.height;
            break;
        case CAMERA_FRAMETYPE_RGB8888:
            dims[0] = vf->cbuf.framedesc.rgb8888.width;
            dims[1] = vf->cbuf.framedesc.rgb8888.height;
            break;
        case CAMERA_FRAMETYPE_RGB888:
            dims[0] = vf->cbuf.framedesc.rgb888.width;
            dims[1] = vf->cbuf.framedesc.rgb888.height;
            break;
        case CAMERA_FRAMETYPE_GRAY8:
            dims[0] = vf->cbuf.framedesc.gray8.width;
            dims[1] = vf->cbuf.framedesc.gray8.height;
            break;
        case CAMERA_FRAMETYPE_CBYCRY:
            dims[0] = vf->cbuf.framedesc.cbycry.width;
            dims[1] = vf->cbuf.framedesc.cbycry.height;
            break;
        case CAMERA_FRAMETYPE_RGB565:
            dims[0] = vf->cbuf.framedesc.rgb565.width;
            dims[1] = vf->cbuf.framedesc.rgb565.height;
            break;
        case CAMERA_FRAMETYPE_YCBYCR:
            dims[0] = vf->cbuf.framedesc.ycbycr.width;
            dims[1] = vf->cbuf.framedesc.ycbycr.height;
            break;
        case CAMERA_FRAMETYPE_YCRYCB:
            dims[0] = vf->cbuf.framedesc.ycrycb.width;
            dims[1] = vf->cbuf.framedesc.ycrycb.height;
            break;
        case CAMERA_FRAMETYPE_CRYCBY:
            dims[0] = vf->cbuf.framedesc.crycby.width;
            dims[1] = vf->cbuf.framedesc.crycby.height;
            break;
        default:
            result = -1;
            errno = EINVAL;
            break;
    }

    return result;
}

void* video_frame_ptr(video_frame_t *vf) {
    return vf->cbuf.framebuf;
}

int video_frame_format(video_frame_t *vf) {
    int fmt;

    switch (vf->cbuf.frametype) {
        case CAMERA_FRAMETYPE_BGR8888:
            fmt = SCREEN_FORMAT_BGRX8888;
            break;
        case CAMERA_FRAMETYPE_RGB8888:
            fmt = SCREEN_FORMAT_RGBX8888;
            break;
        case CAMERA_FRAMETYPE_RGB888:
            fmt = SCREEN_FORMAT_RGB888;
            break;
        case CAMERA_FRAMETYPE_GRAY8:
            fmt = SCREEN_FORMAT_BYTE;
            break;
        case CAMERA_FRAMETYPE_CBYCRY:
            fmt = SCREEN_FORMAT_UYVY;
            break;
        case CAMERA_FRAMETYPE_RGB565:
            fmt = SCREEN_FORMAT_RGB565;
            break;
        case CAMERA_FRAMETYPE_YCBYCR:
            fmt = SCREEN_FORMAT_YUY2;
            break;
        case CAMERA_FRAMETYPE_YCRYCB:
            fmt = SCREEN_FORMAT_YVYU;
            break;
        case CAMERA_FRAMETYPE_CRYCBY:
            fmt = SCREEN_FORMAT_V422;
            break;
        default:
            fmt = -1;
            break;
    }

    return fmt;
}

void video_stop_acquisition(void) {
    pthread_mutex_lock(&VideoMutex);
    if (VideoCfg.state == VIDEO_CONFIG_STOPPED) {
        sprintf(VideoLogBuf, "Already stopped\n");
        DEBUG() << VideoLogBuf << FLUSH;
        pthread_mutex_unlock(&VideoMutex);
        return;
    } else if (VideoCfg.state == VIDEO_CONFIG_STOPPING) {
        sprintf(VideoLogBuf, "Currently stopping\n");
        DEBUG() << VideoLogBuf << FLUSH;
        pthread_mutex_unlock(&VideoMutex);
        return;
    } else if (VideoCfg.state != VIDEO_CONFIG_ACTIVE) {
        sprintf(VideoLogBuf, "Invalid state to stop acquisition. State=%d\n", VideoCfg.state);
        ERROR() << VideoLogBuf << FLUSH;
        pthread_mutex_unlock(&VideoMutex);
        return;
    }
    NOTICE() << "Stopping video acquisition.." << FLUSH;
    VideoCfg.state = VIDEO_CONFIG_STOPPING;

    // The method to stop acquisition depends on on video frames are being acquired.
    // Each method has different limitations based on how the API works and the
    // threading model being used. See the individual functions for more details.
    //
    // It is the responsibility of these functions to unlock the mutex
    locked_stop_acquisition_camera_event(&VideoCfg);
}

int video_acquire_from_camera_event(int handle) {
    int rc;
    struct sigevent event;
    int eventsEnabled = 0;
    int statusEnabled = 0;

    pthread_mutex_lock(&VideoMutex);
    if (VideoCfg.state != VIDEO_CONFIG_STOPPED) {
        ERROR() << "Already acquiring! Ignoring" << FLUSH;
        rc = EBUSY;
        goto exit;
    }

    // Create the config
    VideoCfg.cameraHandle = handle;
    VideoCfg.srcCamera.chid = -1;
    VideoCfg.srcCamera.coid = -1;
    VideoCfg.refCount = 1;
    VideoCfg.state = VIDEO_CONFIG_STARTING;

    // Create the channel that pulses will be received on
    VideoCfg.srcCamera.chid = ChannelCreate(_NTO_CHF_PRIVATE);
    if (VideoCfg.srcCamera.chid == -1) {
        rc = errno;
        ERROR() << "Unable to create channel to receive events. Error=" << strerror(rc) << FLUSH;
        goto exit;
    }
    // Connect to the channel
    VideoCfg.srcCamera.coid = ConnectAttach(0, 0, VideoCfg.srcCamera.chid, _NTO_SIDE_CHANNEL, 0);
    if (VideoCfg.srcCamera.coid == -1) {
        rc = errno;
        ERROR() << "Unable to connect to event channel. Error=" << strerror(rc) << FLUSH;
        goto exit;
    }

    // Create the pulse that the camera will send when a buffer is available.
    SIGEV_PULSE_PTR_INIT(&event, VideoCfg.srcCamera.coid, SIGEV_PULSE_PRIO_INHERIT,
                         VIDEO_PULSE_BUFFER_AVAILABLE, &VideoCfg);
    // And register it with camera so it will be sent
    rc = camera_enable_viewfinder_event(handle, CAMERA_EVENTMODE_READONLY, &VideoCfg.srcCamera.bufferKey, &event);
    if (rc != CAMERA_EOK) {
        ERROR() << "Unable to enable viewfinder events. Error=" << strerror(rc) << FLUSH;
        goto exit;
    }
    DEBUG() << "Camera vf events enabled. Key=" << VideoCfg.srcCamera.bufferKey << FLUSH;
    eventsEnabled = 1;

    // Create the pulse that the camera will send when a status is available
    SIGEV_PULSE_PTR_INIT(&event, VideoCfg.srcCamera.coid, SIGEV_PULSE_PRIO_INHERIT,
                         VIDEO_PULSE_STATUS_AVAILABLE, &VideoCfg);
    // And register it with camera so it will be sent
    rc = camera_enable_status_event(handle, &VideoCfg.srcCamera.statusKey, &event);
    if (rc != CAMERA_EOK) {
        ERROR() << "Unable to enable status events. Error=" << strerror(rc) << FLUSH;
        goto exit;
    }
    DEBUG() << "Camera status events enabled. Key=" << VideoCfg.srcCamera.statusKey << FLUSH;
    statusEnabled = 1;

    // Start the viewfinder
    rc = camera_start_viewfinder(handle, NULL, NULL, NULL);
    if (rc != CAMERA_EOK) {
        ERROR() << "Unable to enable viewfinder. Error=" << strerror(rc) << FLUSH;
        goto exit;
    }

    // Spin up the thread to receive pulses
    pthread_create(&VideoCfg.thread, NULL, event_acquisition_thread, &VideoCfg);
    pthread_setname_np(VideoCfg.thread, "event_acquisition");
    VideoCfg.threadValid = 1;

    VideoCfg.state = VIDEO_CONFIG_ACTIVE;
    rc = EOK;
exit:
    if (rc) {
        // There was a failure. Delete anything I allocated.
        if (VideoCfg.state == VIDEO_CONFIG_STARTING) {
            if (statusEnabled) camera_disable_event(handle, VideoCfg.srcCamera.statusKey);
            if (eventsEnabled) camera_disable_event(handle, VideoCfg.srcCamera.bufferKey);
            if (VideoCfg.srcCamera.chid != -1) ConnectDetach(VideoCfg.srcCamera.chid);
            if (VideoCfg.srcCamera.coid != -1) ChannelDestroy(VideoCfg.srcCamera.coid);
        }
        VideoCfg.state = VIDEO_CONFIG_STOPPED;
    }
    pthread_mutex_unlock(&VideoMutex);
    return rc;
}

int video_frame_matrix(video_frame_t *vf, std::shared_ptr<cv::Mat> image) {
    if (vf->cbuf.frametype == CAMERA_FRAMETYPE_CBYCRY) {
        // convert CBYCRY to RGB expected by ML model
        cv::Mat yuvImage(video_frame_height(vf),
                        video_frame_width(vf), CV_8UC2,
                        vf->cbuf.framebuf,
                        video_frame_width(vf) * 2);
        cvtColor(yuvImage, *image, cv::COLOR_YUV2BGR_UYVY);
        return EOK;
    } else if (vf->cbuf.frametype == CAMERA_FRAMETYPE_YCBYCR) {
        // convert YCBYCR to RGB expected by ML model
        cv::Mat yuvImage(video_frame_height(vf),
                        video_frame_width(vf), CV_8UC2,
                        vf->cbuf.framebuf,
                        video_frame_width(vf) * 2);
        cvtColor(yuvImage, *image, cv::COLOR_YUV2BGR_YUY2);
        return EOK;
    } else if (vf->cbuf.frametype == CAMERA_FRAMETYPE_BGR8888) {
        // convert RGB8888 to RGB expected by ML model
        cv::Mat bgraImage(video_frame_height(vf),
                        video_frame_width(vf), CV_8UC4,
                        vf->cbuf.framebuf,
                        video_frame_width(vf) * 4);
        cvtColor(bgraImage, *image, cv::COLOR_BGRA2BGR);
        return EOK;
    } else {
        return -1;
    }
}
