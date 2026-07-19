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

#include <Logging.h>
#include <ml/TFLiteExecutor.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "FaceDetection/FaceDetections.h"
#include "Processor.h"
#include "Render.h"
#include "Video.h"
#include "CarbonEstimator/CarbonEstimator.h"

#define PREDICT_MODEL_INPUT_IMAGE_HEIGHT 256
#define PREDICT_MODEL_INPUT_IMAGE_WIDTH 256
#define TRANSFER_MODEL_INPUT_IMAGE_HEIGHT 384
#define TRANSFER_MODEL_INPUT_IMAGE_WIDTH 384

static const std::string TransferModelName = "style_transfer";
static const std::string TransferModelFile = "mlModels/tflite/arbitrary-image-stylization-v1-transfer-int8.tflite";
static const std::string PredictModelName = "style_predict";
static const std::string PredictModelFile = "mlModels/tflite/arbitrary-image-stylization-v1-predict-int8.tflite";
static const std::array<std::string, NUM_MODELS - 1> StyleFiles= {
    "styleImages/Bouquet.jpg",
    "styleImages/Suprematist.jpg",
    "styleImages/Sunday.jpg",
    "styleImages/Studio.jpg",
    "styleImages/Winter.jpg",
    "styleImages/GabrielRobin.jpg",
};

// --------------------------------------------------------------------
// Types
// --------------------------------------------------------------------
typedef enum {
    PROCESSOR_STATE_UNINITIALIZED = 0,
    PROCESSOR_STATE_INITIALIZED,
    PROCESSOR_STATE_ACTIVE,
    PROCESSOR_STATE_STOPPING
} processor_state_t;

// --------------------------------------------------------------------
// Global Variables
// --------------------------------------------------------------------
static int ModelState = MODEL_FACE;
static pthread_t ProcessorThread;
static pthread_mutex_t ProcessorMutes = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ProcessorCond = PTHREAD_COND_INITIALIZER;
static processor_state_t ProcessorState = PROCESSOR_STATE_UNINITIALIZED;
static FaceDetections FaceDetector;
static cv::Mat LoadingImage;
static std::vector<float> ModelInput;
static std::vector<float> ModelOutput;
static std::array<std::vector<float>, NUM_MODELS - 1> Styles;
static std::array<cv::Mat, NUM_MODELS - 1> RgbStyleImages;
static bool NewStyle = false;
static struct timespec SnapTime;
static char ProcessorLogBuf[100] = {0};
static bool ReadyForTouch = true;

// Style tranfer model executers
static std::shared_ptr<bbry::poc::TFLiteExecutor> VideoModelExecutor;
static std::shared_ptr<bbry::poc::TFLiteExecutor> StyleModelExecutor;

// A pending video frame to process
static video_frame_t *NewVideoFrame = NULL;

// --------------------------------------------------------------------
// Static Functions
// --------------------------------------------------------------------
// Assumes the mutex is held and in correct state
static void do_processor_stop(void) {
    ProcessorState = PROCESSOR_STATE_STOPPING;
    FaceDetector.stop();
    pthread_cond_broadcast(&ProcessorCond);

    // I have to give up the mutex so that the thread can
    // wake up and actually stop. Otherwise the join will deadlock
    pthread_mutex_unlock(&ProcessorMutes);
    pthread_join(ProcessorThread, NULL);
    // Relock the mutex as the caller expects that
    pthread_mutex_lock(&ProcessorMutes);
}

// --------------------------------------------------------------------
// The processing thread's routine
// --------------------------------------------------------------------
static void* processor_thread(void *arg) {
    int active;
    struct timespec tpStart;
    struct timespec tpEnd;

    video_frame_t *vf = NULL;
    int format;
    int vfDims[2] = {0, 0};
    uint8_t *vfPtr;

    (void)arg;

    INFO() << "Processor thread started." << FLUSH;

    while (1) {
        pthread_mutex_lock(&ProcessorMutes);
        active = ProcessorState == PROCESSOR_STATE_ACTIVE;
        clock_gettime(CLOCK_MONOTONIC, &tpStart);
        if (NewVideoFrame && NewStyle && tpStart.tv_sec >= SnapTime.tv_sec && tpStart.tv_nsec > SnapTime.tv_nsec) {
            vf = NewVideoFrame;
            NewVideoFrame = NULL;
            NewStyle = false;
        } else if (active) {
            if (NewVideoFrame) {
                //dropping frame
                video_frame_release(NewVideoFrame);
                NewVideoFrame = NULL;
            }
            pthread_cond_wait(&ProcessorCond, &ProcessorMutes);
            continue;
        }
        pthread_mutex_unlock(&ProcessorMutes);

        if (!active) {
            break;
        } else if (!vf) {
            INFO() << "Woken up for no reason, going back to sleep." << FLUSH;
            continue;
        }

        if ((format = video_frame_format(vf)) >= 0) {
            if (!(format == SCREEN_FORMAT_UYVY || format == SCREEN_FORMAT_YUY2 || format == SCREEN_FORMAT_YVYU || format == SCREEN_FORMAT_BGRX8888)) {
                sprintf(ProcessorLogBuf, "Video frame format %d is not supported, Ignoring\n", format);
                ERROR() << ProcessorLogBuf << FLUSH;
                continue;
            }
        } else {
            sprintf(ProcessorLogBuf, "Error getting video frame format: %s\n", strerror(errno));
            ERROR() << ProcessorLogBuf << FLUSH;
            continue;
        }

        video_frame_dimensions(vf, vfDims);
        vfPtr = static_cast<uint8_t*>(video_frame_ptr(vf));
        if (!vfPtr) {
            sprintf(ProcessorLogBuf, "Unable to get pointer to video frame: %s\n", strerror(errno));
            ERROR() << ProcessorLogBuf << FLUSH;
            break;
        }
        std::shared_ptr<cv::Mat> image = std::make_shared<cv::Mat>();
        cv::Mat resizedImg;
        cv::Mat floatImg;
        size_t dataSize;
        std::unique_ptr<cv::Mat> outFloatImg = nullptr;
        cv::Mat outImg;
        cv::Mat outRgb;
        // Convert to format for ml model
        video_frame_matrix(vf, image);

        // Carbon estimation runs off the same decoded frame — no extra
        // camera subscription needed. Internally throttled/cheap to call
        // every frame.
        carbon_estimator_process_frame(*image);

        // resize image for the ML model
        // currently, this has to be redone for each BlazeFace model because the supported
        // frame size is different
        cv::resize(*image, resizedImg, cv::Size(TRANSFER_MODEL_INPUT_IMAGE_WIDTH, TRANSFER_MODEL_INPUT_IMAGE_HEIGHT));
        floatImg.create(TRANSFER_MODEL_INPUT_IMAGE_HEIGHT, TRANSFER_MODEL_INPUT_IMAGE_WIDTH, CV_32FC3);
        // Convert uint8_t data range to tensor input range (0 : 1.0)
        resizedImg.convertTo(floatImg, CV_32FC3, (TENSOR_VAL_MAX - TENSOR_VAL_MIN)/255.0, TENSOR_VAL_MIN);
        dataSize = floatImg.total() * floatImg.elemSize();
        ModelInput.resize(dataSize);
        memcpy(&ModelInput[0], &floatImg.data[0], dataSize);
        VideoModelExecutor->setInput("content_image", ModelInput);
        VideoModelExecutor->infer();
        ModelOutput = VideoModelExecutor->getOutput("transformer/expand/conv3/conv/Sigmoid");
        outFloatImg = std::make_unique<cv::Mat>(TRANSFER_MODEL_INPUT_IMAGE_HEIGHT, TRANSFER_MODEL_INPUT_IMAGE_WIDTH,
                                                CV_32FC3, &ModelOutput[0]);
        outImg.create(TRANSFER_MODEL_INPUT_IMAGE_HEIGHT, TRANSFER_MODEL_INPUT_IMAGE_WIDTH, CV_8UC3);
        outFloatImg->convertTo(outImg, CV_8UC3, 255, 0);
        cvtColor(outImg, outRgb, cv::COLOR_BGR2RGB);

        if (active) {
            render_new_rgb_frame(outRgb);
        }
        ReadyForTouch = true;

        clock_gettime(CLOCK_MONOTONIC, &tpEnd);

        // Done with this frame, release it.
        video_frame_release(vf);
        vf = NULL;
    }

    INFO() << "Processor thread no longer active. Exiting." << FLUSH;

    // Release a possible vf we picked up at the same time we were shutting down.
    video_frame_release(vf);

    pthread_mutex_lock(&ProcessorMutes);
    ProcessorState = PROCESSOR_STATE_INITIALIZED;

    return NULL;
}

// --------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------
int processor_initialize(void) {

    pthread_mutex_lock(&ProcessorMutes);
    if (ProcessorState != PROCESSOR_STATE_UNINITIALIZED) {
        ERROR() << "processor is already initialized." << FLUSH;
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &SnapTime);

    CarbonEstimatorConfig carbonConfig; // edit fields here, or load from a config file
    carbon_estimator_initialize(carbonConfig);

    cv::Mat image = cv::imread("styleImages/loading.jpg");
    cvtColor(image, LoadingImage, cv::COLOR_BGR2RGB);
    // Initialize ML models
    StyleModelExecutor = std::make_shared<bbry::poc::TFLiteExecutor>(PredictModelName, PredictModelFile);
    VideoModelExecutor = std::make_shared<bbry::poc::TFLiteExecutor>(TransferModelName, TransferModelFile);
    std::cout << "Loading style model..." << std::endl;
    if (!StyleModelExecutor->loadMLModel()) {
        sprintf(ProcessorLogBuf, "Failed to load ML model for generating style.");
        ERROR() << ProcessorLogBuf << FLUSH;
        return -1;
    }
    std::cout << "Loading video model..." << std::endl;
    if (!VideoModelExecutor->loadMLModel()) {
        sprintf(ProcessorLogBuf, "Failed to load ML model for stylizing video.");
        ERROR() << ProcessorLogBuf << FLUSH;
        return -1;
    }
    // Generate style
    for (int i = 0; i < NUM_MODELS - 1; i++) {
        cv::Mat styleImage = cv::imread(StyleFiles[i]);
        cvtColor(styleImage, RgbStyleImages[i], cv::COLOR_BGR2RGB);
        cv::Mat syleInput;
        std::vector<float> modelInput;
        syleInput.create(PREDICT_MODEL_INPUT_IMAGE_HEIGHT, PREDICT_MODEL_INPUT_IMAGE_WIDTH, CV_32FC3);
        // Convert uint8_t data range to tensor input range (-1.0 : 1.0)
        styleImage.convertTo(syleInput, CV_32FC3, (TENSOR_VAL_MAX - TENSOR_VAL_MIN)/255.0, TENSOR_VAL_MIN);
        size_t dataSize = syleInput.total() * syleInput.elemSize();
        modelInput.resize(dataSize);
        // Copy matrix into float vector
        memcpy(&modelInput[0], &syleInput.data[0], dataSize);
        std::cout << "Preloading style " << i << "..." << std::endl;
        // Set input to model
        if (!StyleModelExecutor->setInput("style_image", modelInput)) {
            sprintf(ProcessorLogBuf, "Failed to set style image input %d.", i);
            ERROR() << ProcessorLogBuf << FLUSH;
            return -1;
        }
        // Infer
        if (!StyleModelExecutor->infer()) {
            sprintf(ProcessorLogBuf, "Failed to infer style %d.", i);
            ERROR() << ProcessorLogBuf << FLUSH;
            return -1;
        }
        Styles[i] = StyleModelExecutor->getOutput("mobilenet_conv/Conv/BiasAdd");
    }

    ProcessorState = PROCESSOR_STATE_INITIALIZED;
    pthread_mutex_unlock(&ProcessorMutes);
    return 0;
}

void processor_destroy(void) {
    pthread_mutex_lock(&ProcessorMutes);
    if (ProcessorState == PROCESSOR_STATE_ACTIVE) {
        do_processor_stop();
    }

    carbon_estimator_destroy();

    ProcessorState = PROCESSOR_STATE_UNINITIALIZED;
    pthread_mutex_unlock(&ProcessorMutes);
}

int processor_start(void) {
    int result = 0;

    pthread_mutex_lock(&ProcessorMutes);
    if (ProcessorState == PROCESSOR_STATE_ACTIVE) {
        WARNING() << "processor is already running" << FLUSH;
        goto exit;
    } else if (ProcessorState != PROCESSOR_STATE_INITIALIZED) {
        ERROR() << "processor is not in initialized state." << FLUSH;
        result = -1;
        goto exit;
    }

    NewVideoFrame = NULL;
    FaceDetector.start();
    // Spin up the processing thread.
    pthread_create(&ProcessorThread, NULL, processor_thread, NULL);
    pthread_setname_np(ProcessorThread, "processor");
    ProcessorState = PROCESSOR_STATE_ACTIVE;

exit:
    pthread_mutex_unlock(&ProcessorMutes);
    return result;
}

void processor_stop() {
    pthread_mutex_lock(&ProcessorMutes);
    if (ProcessorState == PROCESSOR_STATE_ACTIVE) {
        do_processor_stop();
    } else if (ProcessorState == PROCESSOR_STATE_INITIALIZED) {
        WARNING() << "processor is already stopped" << FLUSH;
    } else {
        ERROR() << "processor is not running" << FLUSH;
    }

    pthread_mutex_unlock(&ProcessorMutes);
}

void processor_new_video_frame(video_frame_t *vf) {
    int format;

    // Make sure the format is one I understand.
    if ((format = video_frame_format(vf)) >= 0) {
        if (format == SCREEN_FORMAT_UYVY || format == SCREEN_FORMAT_YUY2 || format == SCREEN_FORMAT_YVYU || format == SCREEN_FORMAT_BGRX8888) {
            pthread_mutex_lock(&ProcessorMutes);
            if (ProcessorState == PROCESSOR_STATE_ACTIVE) {
                NewVideoFrame = video_frame_replace(NewVideoFrame, vf);
                pthread_cond_signal(&ProcessorCond);
            } else {
                WARNING() << "Processor not active. Ignoring new video frame." << FLUSH;
            }
            pthread_mutex_unlock(&ProcessorMutes);
        } else {
            ERROR() << "Video frame format " << format << " is not supported, Ignoring" << FLUSH;
        }
    } else {
        ERROR() << "Error getting video frame format: " << strerror(errno) << FLUSH;
    }
}

void processor_select_style(int styleNum) {
    pthread_mutex_lock(&ProcessorMutes);
    int lastModelState = ModelState;
    if (ReadyForTouch) {
        ModelState = styleNum + 1;
        if (ModelState == MODEL_FACE) {
            FaceDetector.start();
        } else if (lastModelState == MODEL_FACE && ModelState != MODEL_FACE) {
            FaceDetector.stop();
        }
        if (ModelState != MODEL_FACE) {
            ReadyForTouch = false;
            render_new_rgb_frame(LoadingImage);
            // Update style input from preloaded styles
            VideoModelExecutor->setInput("mobilenet_conv/Conv/BiasAdd", Styles[ModelState - 1]);
            clock_gettime(CLOCK_MONOTONIC, &SnapTime);
            // Delay time that picutre is taken by 1 second so user can pose
            SnapTime.tv_sec++;
            NewStyle = true;
        }
    }
    pthread_mutex_unlock(&ProcessorMutes);
}

int processor_get_model_state() {
    return ModelState;
}
