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
#ifndef _VIDEO_H_
#define _VIDEO_H_
#include <ml/TFLiteExecutor.h>
#include <opencv2/opencv.hpp>

/**
 * @file
 *
 * Video (camera) management.
 *
 * This file manages access to the camera and the buffers
 * it creates.
 *
 * Buffers are reference counted and as long as that reference
 * count is non-zero the buffer is guaranteed to be valid.
 */

#include <screen/screen.h>


struct video_frame;
/** Opaque type representing a video frame. */
typedef struct video_frame video_frame_t;

/**
 * Increase the reference count to a video frame.
 *
 * Video frames are guaranteed to be valid as long as
 * the reference count is non-zero.
 *
 * @param[in]   vf      The video frame that will have its reference count increased.
 */
void video_frame_acquire(video_frame_t *vf);

/**
 * Decrease the reference count to a video frame.
 *
 * Once a video frame's reference count goes to zero it
 * will be released and hence is no longer valid.
 *
 * @note Stopping video frame acquisition depends on having no
 *       buffers with a non-zero reference count. Thus calling
 *       this API may also cause frame acqusition to complete
 *       a stop request.
 *
 * @param[in]   vf      The video frame that will have its reference count decreased.
 *                      May be NULL in which case this is a no-op.
 */
void video_frame_release(video_frame_t *vf);

/**
 * Replaces oldVf with newVf by releasing oldVf and acquiring newVf
 * all within the same mutex lock.
 *
 * This is a more efficient way of doing:
 * @code
 * video_frame_release(oldVf);
 * if (newVf) video_frame_acquire(newVf);
 * @endcode
 *
 * @param oldVf        The old video frame to release. May be NULL
 * @param newVf        The new video frame to acquire. May be NULL
 *
 * @return newVf
 */
video_frame_t* video_frame_replace(video_frame_t *oldVf, video_frame_t *newVf);

/**
 * Return the unique identifier of a video frame.
 *
 * Each video_frame has a unique identifier.
 * The identifier has no meaning other than for checking
 * (in)equality
 *
 * @param[in]   vf      The video frame
 *
 * @return The unique identifier of the video frame.
 */
uint64_t video_frame_id(video_frame_t *vf);

/**
 * Return the width, in pixels, of the video frame.
 *
 * @note The frame width is not necessarily the same as
 *       the frame's stride. However for the moment,
 *       that is the case.
 *
 * @param[in]   vf      The video frame
 *
 * @return On success, the width of the video frame in pixels.
 * @return On failure, -1
 *
 */
int video_frame_width(video_frame_t *vf);

/**
 * Return the height, in pixels, of the video frame.
 *
 * @param[in]   vf      The video frame
 *
 * @return On success, the height of the video frame in pixels.
 * @return On failure, -1
 */
int video_frame_height(video_frame_t *vf);

/**
 * Return the dimensions, in pixels, of the video frame.
 *
 * The dimensions are a two integer pair with the first
 * integer being the width, and the second being the height.
 *
 * @param[in]   vf      The video frame
 * @param[out]  dims    Array to store the dimensions into.
 *                      Ensure it has space for at least
 *                      two ints.
 *
 * @return On success, 0
 * @return On failure, -1
 */
int video_frame_dimensions(video_frame_t *vf, int *dims);

/**
 * Return the size of the frame in bytes.
 *
 * @param[in]   vf      The video frame
 *
 * @return On success, the size of the frame in bytes.
 * @return On failure, -1
 */
//int video_frame_size(video_frame_t *vf);

/**
 * Get the pixel format of the video frame.
 *
 * @param[in]   vf      The video frame
 *
 * @return On success, the format as represented by one of
 *         the SCREEN_FORMAT_* values.
 * @return On failure, -1
 */
int video_frame_format(video_frame_t *vf);

/**
 * Get a raw pointer to the video frame's data.
 *
 * @note This pointer is only valid for as long
 *       as the video frame itself is valid.
 *
 * @param[in]   vf      The video frame
 *
 * @return On success, a valid pointer to the start of the frame's data
 * @return On failure, NULL
 */
void* video_frame_ptr(video_frame_t *vf);

/**
 * Start acquisition of video frames via camera events.
 *
 * This API will configure camera to notify the video
 * component of camera status and viewfinder events. It
 * will then enable the viewfinder.
 *
 * @param[in]   handle  A valid camera handle.
 *
 * @return On success, 0/EOK
 * @return On failure, one of the errno values.
 */
int video_acquire_from_camera_event(int handle);

/**
 * Stop video frame acquisition
 *
 * Stops video frame acquisition. This process involves waiting for
 * all known video frames to be released. Any resources allocated
 * for acquisition are released by the time this API returns.
 *
 * The exact method used to stop acquisition depends on the acquisition
 * method currently being used. This can change the order of calls
 * to the various systems used for acquisition.
 */
void video_stop_acquisition(void);

/**
 * Convert camera video frame to OpenCV Matrix.
 *
 * The matrix is used to pass the frame to the TFLite models.
 *
 * @param[in]   vf      A video frame.
 * @param[in]   image   shared pointer to the matrix to populate.
 *
 * @return On success, 0/EOK
 * @return On failure, one of the errno values.
 */
int video_frame_matrix(video_frame_t *vf, std::shared_ptr<cv::Mat> image);
#endif
