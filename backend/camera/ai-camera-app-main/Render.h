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
#ifndef _RENDER_H_
#define _RENDER_H_

/**
 * @file
 *
 * A simple OpenGL backed renderer.
 *
 * The renderer is responsible for rendering UI elements and the video stream to the display.
 */

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <screen/screen.h>

#include "Video.h"

#define RENDER_FLAG_UYVY                1       ///< Renderer frame format
#define RENDER_FLAG_YUYV                2       ///< Renderer frame format
#define RENDER_FLAG_BGRA                4       ///< Renderer frame format

/**
 * Initialize the rendering engine.
 *
 * This must be called before any rendering can take place.
 *
 * @param[in] srcWin    The screen window the renderer will render into
 * @param[in] flags     Flags to control what the renderer does
 *
 * @return 0 On success, non-zero on failure.
 */
int render_initialize(screen_window_t srcWin, uint32_t flags);

/**
 * Destroy the rendering engine.
 *
 * Automatically stops the renderer if it is started.
 * Once this API has been called, the renderer can't be used again
 * until a call to render_initialize() is made.
 */
void render_destroy(void);

/**
 * Start the rendering engine.
 *
 * Must have been previously initialized.
 *
 * This spins up the rendering thread.
 *
 * @return 0 On success, non-zero on failure.
 */
int render_start(void);

/**
 * Stop the rendering engine.
 *
 * Must have been previously initialized.
 *
 * This will stop and join the rendering thread.
 *
 * Calling this when rendering has not yet started is a no-op.
 */
void render_stop();

/**
 * Pass a new video frame to the renderer.
 *
 * The renderer will increment the reference count
 * of the video frame while it is being used by
 * the renderer. When it is done with the frame
 * it will then decrement the count.
 *
 * @param[in]   vf      The new video frame
 */
void render_new_video_frame(video_frame_t *vf);

/**
 * Pass a new RGB video frame to the renderer.
 *
 * @param[in]   data      RGB frame data to render
 */
void render_new_rgb_frame(cv::Mat data);

/**
 * Pass a new set of face bounding boxes to be drawn.
 *
 * @param[in] boxes       Face bounding boxes.
 *
 * @param[in] width       Width of camera frame, used for scaling boxes.
 *
 * @param[in] height      height of camera frame, used for scaling boxes.
 */
void render_new_face_detection(nlohmann::json& boxes, int width, int height);

/**
 * Set scaling factor on the x axis for drawing boxes.
 *
 * @param[in] factor Scales the boxes on the x axis.
*/
void render_set_x_scale(float factor);

/**
 * Set scaling factor on the y axis for drawing boxes.
 *
 * @param[in] factor Scales the boxes on the y axis.
*/
void render_set_y_scale(float factor);

/**
 * Set offset on the x axis for drawing boxes.
 *
 * @param[in] factor offsets the boxes on the x axis.
*/
void render_set_x_offset(int offset);

/**
 * Set offset on the y axis for drawing boxes.
 *
 * @param[in] factor offsets the boxes on the y axis.
*/
void render_set_y_offset(int offset);

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif

#endif
