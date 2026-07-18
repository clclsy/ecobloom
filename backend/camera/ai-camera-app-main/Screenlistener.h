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

#ifndef _SCREEN_LISTENER_H_
#define _SCREEN_LISTENER_H_

/**
 * @file
 *
 * A listener for screen events.
 *
 * Screen events are used to inform AICE when various non-local
 * entities are created and destroyed. A non-local entity is generally
 * a window or a stream that some other context in the system creates
 * but grants AICE's context permissions to access it.
 *
 * Input events, for example from the touchscreen, are also received
 * via screen events. Touchscreen and mouse events are listened to so
 * the user can interact with the UI.
 */

#include <screen/screen.h>

/**
 * Starts a thread to monitor for screen events.
 *
 * @param[in]   context         The screen context that will receive events
 * @param[in]   videoWin        The video window that AICE has created
 *                              to receive video frame data. Once a stream
 *                              has been created it will be attached to this
 *                              window.
 *
 * @return EOK on success
 * @return An errno value on failure.
 */
int screen_listener_start(screen_context_t context, screen_window_t videoWin);

/**
 * Stops the screen event monitoring thread.
 *
 * @return EOK on success
 * @return An errno value on failure.
 */
int screen_listener_stop(void);

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif

#endif
