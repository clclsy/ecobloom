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

#ifndef __SENSOR_REARVIEW_GLOBAL_H
#define __SENSOR_REARVIEW_GLOBAL_H

/**
 * @file
 * Globals shared across AICE that don't really have an obvious home.
 */

#include <screen/screen.h>

#define MAX_RETRIES 5

/**
 * Start the camera.
 *
 * This will initialize the connection to camera, pick the camera to use
 * and turn on the viewfinder.
 *
 * @return 0/EOK if successful
 * @return An errno code if there is a failure.
 */
int start_camera(void);

/**
 * Stops the camera viewfinder.
 *
 * This does NOT de-initialize the connection to the camera.
 * Hence the next time start_camera() is called, the same
 * camera will be reused.
 *
 * @return 0/EOK if successful
 * @return An errno code if there is a failure.
 */
int stop_camera(void);

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif

#endif /*__SENSOR_REARVIEW_GLOBAL_H */
