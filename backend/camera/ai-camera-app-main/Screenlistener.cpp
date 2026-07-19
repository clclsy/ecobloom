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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <screen/screen.h>
#include <unistd.h>

#include <Logging.h>

#include "Global.h"
#include "Processor.h"
#include "Screenlistener.h"
#include "Video.h"


// Context for our screen event monitoring thread
typedef struct {
    bool                active;             // Indicates if event thread is active
    screen_context_t    context;            // Our screen context
    pthread_t           eventThread;        // Event monitoring thread
    pthread_mutex_t     mutex;              // Mutex for thread safety
    pthread_cond_t      cond;               // Condvar to signal new events
    screen_window_t     videoWin;           // Window to attach the video stream to
    int                 winHeight;          // Dimensions of video window
    int                 winWidth;
    int                 clickSelection;     // Selected style on mouse button down
} listener_thread_info_t;

// State information for event thread
static listener_thread_info_t ListenerThreadInfo;

static char ListenerLogBuf[128] = {0};

static int handle_create_event(listener_thread_info_t *info, screen_event_t event) {
    int rc;
    screen_stream_t inputStream = NULL;
    char id[64];
    int objectType;
    screen_window_t win;

    // Got a create event, a window or stream has attached to my context
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_OBJECT_TYPE, &objectType) == - 1) {
        rc = errno;
        sprintf(ListenerLogBuf, "Failed to get object type of screen event: err = %d\n", rc);
        ERROR() << ListenerLogBuf << FLUSH;
        return rc;
    }
    if (objectType == SCREEN_OBJECT_TYPE_WINDOW) {
        // Make sure it is the video window we were waiting for.
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_WINDOW, (void **)&win) == -1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Failed to get screen event window: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        screen_get_window_property_cv(win, SCREEN_PROPERTY_ID_STRING, sizeof(id), id);
        sprintf(ListenerLogBuf, "Screen reporting create for window %s\n", id);
        INFO() << ListenerLogBuf << FLUSH;
    } else if (objectType == SCREEN_OBJECT_TYPE_STREAM) {
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_STREAM, (void**)&inputStream)) {
            rc = errno;
            sprintf(ListenerLogBuf, "error requesting stream handle: %s\n", strerror(rc));
            ERROR() << ListenerLogBuf << FLUSH;
            screen_destroy_event(event);
            return rc;
        }
        screen_get_stream_property_cv(inputStream, SCREEN_PROPERTY_ID, sizeof(id), id);
        sprintf(ListenerLogBuf, "Screen reporting create for producer stream %s\n", id);
        INFO() << ListenerLogBuf << FLUSH;
    }

    return EOK;
}

static int handle_close_event(listener_thread_info_t *info, screen_event_t event) {
    int rc;
    int objectType;
    screen_window_t win;
    screen_stream_t inputStream;
    char id[64];

    // Something I am tracking closed.
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_OBJECT_TYPE, &objectType) == - 1) {
        rc = errno;
        sprintf(ListenerLogBuf, "Failed to get object type of screen event: err = %d\n", rc);
        ERROR() << ListenerLogBuf << FLUSH;
        return rc;
    }
    if (objectType == SCREEN_OBJECT_TYPE_WINDOW) {
        // Get child window handle
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_WINDOW, (void **)&win) == -1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Failed to get screen event window: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        // Call destroy on this window handle to free a small bit of memory allocated on our behalf
        if (win != NULL) {
            screen_get_window_property_cv(win, SCREEN_PROPERTY_ID, sizeof(id), id);
            sprintf(ListenerLogBuf, "Screen reporting close window %s\n", id);
            INFO() << ListenerLogBuf << FLUSH;
            if (screen_destroy_window(win) == -1) {
                rc = errno;
                sprintf(ListenerLogBuf, "Failed to destroy window remnants: err = %d\n", rc);
                ERROR() << ListenerLogBuf << FLUSH;
                return rc;
            }
        }

    } else if (objectType == SCREEN_OBJECT_TYPE_STREAM) {
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_STREAM, (void**)&inputStream)) {
            rc = errno;
            sprintf(ListenerLogBuf, "error requesting stream handle. Error=%s\n", strerror(rc));
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        if (inputStream) {
            screen_get_stream_property_cv(inputStream, SCREEN_PROPERTY_ID, sizeof(id), id);
            sprintf(ListenerLogBuf, "Screen reporting close for producer stream %s\n", id);
            INFO() << ListenerLogBuf << FLUSH;

            // Call destroy on this stream handle to free a small bit of memory allocated on our behalf
            if (screen_destroy_stream(inputStream)) {
                rc = errno;
                sprintf(ListenerLogBuf, "Failed to destroy stream remnants: err = %d\n", rc);
                ERROR() << ListenerLogBuf << FLUSH;
                return rc;
            }
        }
    }

    return EOK;
}

static int handle_pointer_button_event(listener_thread_info_t *info, screen_event_t event, int down) {
    int rc;
    int objectType;
    screen_window_t win;
    char id[64];
    int pos[2];

    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_OBJECT_TYPE, &objectType) == - 1) {
        rc = errno;
        sprintf(ListenerLogBuf, "Failed to get object type of screen event: err = %d\n", rc);
        ERROR() << ListenerLogBuf << FLUSH;
        return rc;
    }
    if (objectType == SCREEN_OBJECT_TYPE_WINDOW) {
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_WINDOW, (void **)&win) == -1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Failed to get screen event window: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        screen_get_window_property_cv(win, SCREEN_PROPERTY_ID_STRING, sizeof(id), id);
        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos) == - 1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Unable to get touch position: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        sprintf(ListenerLogBuf, "Left mouse button down (%s) @ (%d,%d)", id, pos[0], pos[1]);
        DEBUG() << ListenerLogBuf << FLUSH;
        if (pos[0] < info->winWidth / 10) {
            for (int i = 0; i < 6; i++) {
                if (pos[1] < (i + 1) / 6.0 * info->winHeight) {
                    if (down) {
                        info->clickSelection = i;
                        sprintf(ListenerLogBuf, "Style %d selected with click down.", i);
                        INFO() << ListenerLogBuf << FLUSH;
                    } else if (info->clickSelection == i) { // click release
                        processor_select_style(i);
                        sprintf(ListenerLogBuf, "Style %d selected.", i);
                        INFO() << ListenerLogBuf << FLUSH;
                    }
                    break;
                }
            }
        } else {
            if (down) {
                info->clickSelection = -1;
                sprintf(ListenerLogBuf, "Style -1 selected with click down.");
                INFO() << ListenerLogBuf << FLUSH;
            } else if (info->clickSelection == -1) { // click release
                processor_select_style(-1);
                INFO() << "Face detection selected." << FLUSH;
            }
        }
    }

    return EOK;
}

static int handle_touch_event(listener_thread_info_t *info, screen_event_t event) {
    int rc;
    int objectType;
    screen_window_t win;
    char id[64];
    int pos[2];

    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_OBJECT_TYPE, &objectType) == - 1) {
        rc = errno;
        sprintf(ListenerLogBuf, "Failed to get object type of screen event: err = %d\n", rc);
        ERROR() << ListenerLogBuf << FLUSH;
        return rc;
    }
    if (objectType == SCREEN_OBJECT_TYPE_WINDOW) {
        if (screen_get_event_property_pv(event, SCREEN_PROPERTY_WINDOW, (void **)&win) == -1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Failed to get screen event window: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        screen_get_window_property_cv(win, SCREEN_PROPERTY_ID_STRING, sizeof(id), id);
        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos) == - 1) {
            rc = errno;
            sprintf(ListenerLogBuf, "Unable to get touch position: err = %d\n", rc);
            ERROR() << ListenerLogBuf << FLUSH;
            return rc;
        }
        sprintf(ListenerLogBuf, "D1 MTOUCH(%s) @ (%d,%d)", id, pos[0], pos[1]);
        DEBUG() << ListenerLogBuf << FLUSH;
        if (pos[0] < info->winWidth / 10) {
            for (int i = 0; i < 6; i++) {
                if (pos[1] < (i + 1) / 6.0 * info ->winHeight) {
                    processor_select_style(i);
                    sprintf(ListenerLogBuf, "Style %d selected.", i);
                    INFO() << ListenerLogBuf << FLUSH;
                    break;
                }
            }
        } else {
            INFO() << "Face detection selected." << FLUSH;
            processor_select_style(-1);
        }
    }

    return EOK;
}

/*
 * Event monitoring thread: waits for screen events until it has got child window
 * create events for all cameras, at which point it returns.
 */
static void* listener_thread(void* arg)
{
    listener_thread_info_t* info = (listener_thread_info_t*) arg;
    screen_event_t screenEvent;
    int eventType;
    int err;
    bool active;
    bool touchReleased = true;
    bool mouseDown = false;
    int eventButton;

    if (info == NULL) {
        sprintf(ListenerLogBuf, "NULL argument for event monitoring thread\n");
        ERROR() << ListenerLogBuf << FLUSH;
        return NULL;
    }

    if (screen_create_event(&screenEvent) == -1) {
        err = errno;
        sprintf(ListenerLogBuf, "Failed to create screen event: err = %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
        return NULL;
    }

    // Get events until we are done
    while (1) {
        pthread_mutex_lock(&info->mutex);
        active = info->active;
        pthread_mutex_unlock(&info->mutex);
        if (!active) break;

        if (screen_get_event(info->context, screenEvent, -1) == -1) {
            err = errno;
            // Errors are normal if shutting down and destroy the screen context
            continue;
        }
        if (screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_TYPE, &eventType) == -1) {
            err = errno;
            sprintf(ListenerLogBuf, "Failed to get type of screen event: err = %d\n", err);
            ERROR() << ListenerLogBuf << FLUSH;
            continue;
        }
        if (eventType == SCREEN_EVENT_CREATE) {
            // Got a create event, a window or stream has attached to my context
            err = handle_create_event(info, screenEvent);
            if (err != EOK) {
                sprintf(ListenerLogBuf, "Failed to handle CREATE screen event\n");
                ERROR() << ListenerLogBuf << FLUSH;
                continue;
            }
        } else if (eventType == SCREEN_EVENT_CLOSE) {
            err = handle_close_event(info, screenEvent);
            if (err != EOK) {
                sprintf(ListenerLogBuf, "Failed to handle CLOSE screen event\n");
                ERROR() << ListenerLogBuf << FLUSH;
                continue;
            }
        } else if (eventType == SCREEN_EVENT_MTOUCH_TOUCH) {
            if (touchReleased) {
                touchReleased = false;
                if (!mouseDown) {
                    err = handle_touch_event(info, screenEvent);
                    if (err != EOK) {
                        sprintf(ListenerLogBuf, "Failed to handle MTOUCH screen event\n");
                        ERROR() << ListenerLogBuf << FLUSH;
                        continue;
                    }
                }
            }
        } else if (eventType == SCREEN_EVENT_MTOUCH_RELEASE) {
            touchReleased = true;
        } else if (eventType == SCREEN_EVENT_POINTER) {
            if (screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_BUTTONS, &eventButton) == -1) {
                err = errno;
                sprintf(ListenerLogBuf, "Failed to get screen event buttons: err = %d\n", err);
                ERROR() << ListenerLogBuf << FLUSH;
                continue;
            }
            if (eventButton & SCREEN_LEFT_MOUSE_BUTTON) {
                if (!mouseDown && touchReleased) {
                    mouseDown = true;
                    err = handle_pointer_button_event(info, screenEvent, true);
                    if (err != EOK) {
                        sprintf(ListenerLogBuf, "Failed to handle mouse button down screen event\n");
                        ERROR() << ListenerLogBuf << FLUSH;
                        continue;
                    }
                }
            } else {
                if (mouseDown && touchReleased) {
                    mouseDown = false;
                    err = handle_pointer_button_event(info, screenEvent, false);
                    if (err != EOK) {
                        sprintf(ListenerLogBuf, "Failed to handle mouse button up screen event\n");
                        ERROR() << ListenerLogBuf << FLUSH;
                        continue;
                    }
                }
            }
        }
    }

    screen_destroy_event(screenEvent);
    return NULL;
}

/*
 * Starts a thread to monitor screen events
 */
int screen_listener_start(screen_context_t context, screen_window_t videoWin)
{
    ListenerThreadInfo.clickSelection = -2; // So we know nothing was selected yet.
    int err;
    pthread_condattr_t condAttr;

    // Init state
    ListenerThreadInfo.context = context;
    ListenerThreadInfo.videoWin = videoWin;
    int dims[2];
    err = screen_get_window_property_iv(videoWin, SCREEN_PROPERTY_BUFFER_SIZE, dims);
    if (err != EOK) {
        sprintf(ListenerLogBuf, "Failed to get main window's size, %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
        return err;
    }
    ListenerThreadInfo.winWidth = dims[0];
    ListenerThreadInfo.winHeight = dims[1];

    // Initialize mutex and condvar
    err = pthread_mutex_init(&ListenerThreadInfo.mutex, NULL);
    if (err != EOK) {
        sprintf(ListenerLogBuf, "Failed to initialize mutex: err = %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
        return err;
    }
    err = pthread_condattr_init(&condAttr);
    if (err == EOK) {
        err = pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
        if (err == EOK) {
            err = pthread_cond_init(&ListenerThreadInfo.cond, &condAttr);
            if (err != EOK) {
                sprintf(ListenerLogBuf, "Failed to initialize condvar: err = %d\n", err);
                ERROR() << ListenerLogBuf << FLUSH;
            }
        } else {
            sprintf(ListenerLogBuf, "Failed to set clock of condattr: err = %d\n", err);
            ERROR() << ListenerLogBuf << FLUSH;
        }
        pthread_condattr_destroy(&condAttr);
    } else {
        sprintf(ListenerLogBuf, "Failed to initialize condattr: err = %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
    }
    if (err != EOK) {
        pthread_mutex_destroy(&ListenerThreadInfo.mutex);
        return err;
    }

    // Start thread that we will join later
    ListenerThreadInfo.active = true;
    err = pthread_create(&ListenerThreadInfo.eventThread, NULL, listener_thread,
                         &ListenerThreadInfo);
    if (err != EOK) {
        sprintf(ListenerLogBuf, "Failed to create event monitoring thread: err = %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
        ListenerThreadInfo.active = false;
        pthread_mutex_destroy(&ListenerThreadInfo.mutex);
        pthread_cond_destroy(&ListenerThreadInfo.cond);
    } else {
        pthread_setname_np(ListenerThreadInfo.eventThread, "screenlistener");
    }
    return err;
}

/*
 * Stops the event monitoring thread
 */
int screen_listener_stop(void)
{
    int err;

    // Join the thread
    pthread_mutex_lock(&ListenerThreadInfo.mutex);
    ListenerThreadInfo.active = false;
    pthread_cond_broadcast(&ListenerThreadInfo.cond);
    pthread_mutex_unlock(&ListenerThreadInfo.mutex);
    err = pthread_join(ListenerThreadInfo.eventThread, NULL);
    if (err != EOK) {
        sprintf(ListenerLogBuf, "Failed to join event thread: err = %d\n", err);
        ERROR() << ListenerLogBuf << FLUSH;
        return err;
    }
    pthread_mutex_destroy(&ListenerThreadInfo.mutex);
    pthread_cond_destroy(&ListenerThreadInfo.cond);

    return EOK;
}

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif
