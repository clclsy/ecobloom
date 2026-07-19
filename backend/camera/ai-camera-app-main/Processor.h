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
#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

/**
 * @file
 *
 * APIs to control the processing engine within AICE.
 *
 * The processor is responsible for switching between styles
 * and processing frames with the ML models to sytlize them.
 * The stylized frames are passed to the renderer.
 */

#include "Video.h"

typedef enum {
    MODEL_FACE = 0,
    MODEL_STYLE0 = 1,
    MODEL_STYLE1 = 2,
    MODEL_STYLE2 = 3,
    MODEL_STYLE3 = 4,
    MODEL_STYLE4 = 5,
    MODEL_STYLE5 = 6,
} model_state_t;
#define NUM_MODELS 7

#define TENSOR_VAL_MAX 1.0
#define TENSOR_VAL_MIN 0

/**
 * Switches between different tflite features.
 *
 * @param[in]   styleNum     The new style to use.
*/
void processor_select_style(int styleNum);

/**
 * Get which model we should be using.
*/
int processor_get_model_state();

/**
 * Initialize the processing engine.
 *
 * @return 0 On success, non-zero on failure.
 */
int processor_initialize(void);

/**
 * Destroy the processing engine.
 *
 * This will automatically call processor_stop()
 * if the engine is currently started.
 */
void processor_destroy(void);

/**
 * Start the video processing engine.
 *
 * Must have been previously initialized.
 *
 * This spins up the processing thread.
 *
 * @return 0 On success, non-zero on failure.
 */
int processor_start(void);

/**
 * Stop the video processing engine.
 *
 * Must have been previously initialized.
 *
 * This will stop and join the processing thread.
 *
 * Calling this when processing has not yet started is a no-op.
 */
void processor_stop();

/**
 * Pass a new video frame to the processor.
 *
 * The processor will increment the reference count
 * of the video frame while it is being used by
 * the processor. When it is done with the frame
 * it will then decrement the count.
 *
 * Only the last video frame received is kept by
 * the current processing engine.
 *
 * @param[in]   vf      The new video frame
 */
void processor_new_video_frame(video_frame_t *vf);

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif

#endif
