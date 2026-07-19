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


#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <camera/camera_api.h>
#include <camera/camera_encoder.h>
#include <camera/camera_3a.h>

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>

#include <processors/CameraFrameIntake.h>

// Number of cameras supported
//static const int MAX_CAMERAS = 10;

#define MAX_SIMULTANEOUS_CAMERAS            64

// need at least one camera available for the synthetic sensor to operate
#define MINIMUM_NUMBER_CAMERAS              1

// Constants for polling while waiting for camera service, pool every 5 ms for a max of 8 seconds
#define CAMERA_POLL_TIME_US                 5000
#define CAMERA_MAX_POLL_LOOPS               800

// Precision value for framerate accuracy
#define PRECISION_VALUE                     0.0001

// interval in seconds for logging frame rate in stream example
#define STREAM_FRAME_RATE_LOG_INTERVAL      3

// Try to consume stream buffers this many times until we give up
#define STREAM_CONSUME_RETRY_COUNT          5

// Exit request is not time sensitive: 100 ms timeout
#define SCREEN_GET_EVENT_TIMEOUT_NS 100000000

// Our maximum size for our window ID string
#define WINDOW_ID_MAX_SIZE                  64

// Maximum number of camera frame buffers for pool
#define MAX_CAMERA_FRAME_BUFFERS            10

// flip camera frame in camera connector
#define FLIP_CAMERA_FRAME  0


// Flags for cleanup on exit
typedef enum {
    CLEANUP_NONE = 0,                       // Nothing to cleanup
    CLEANUP_CAMERA_HANDLE = 1,              // Need to close camera handle
    CLEANUP_VIEWFINDER = 2,                 // Need to stop camera viewfinder
    CLEANUP_APP_WINDOW = 4,                 // Need to destroy the App window
    CLEANUP_CONTEXT = 0x10,                 // Need to destroy screen context
    CLEANUP_EVENT_THREAD = 0x20             // Need to stop event monitoring thread
} cleanupFlags_t;

// Camera Info
typedef struct {
    camera_unit_t cameraUnit = CAMERA_UNIT_1;
    camera_handle_t cameraHandle = 0;
    std::string cameraID;
    unsigned int cameraViewfinderWidth = 0;
    unsigned int cameraViewfinderHeight = 0;
    double cameraViewfinderFramerate = 0;
    camera_frametype_t cameraViewfinderFormat = CAMERA_FRAMETYPE_RGB888;
    uint32_t cameraFlags = 0;
} cameraInfo_t;

class QSFCameraIntake final : public CameraFrameIntake {
 public:
  /**
   * Construct a new Camera Service Intake object
   */
  QSFCameraIntake();

  /**
   * Destroy the Camera Service Intake object.
   *
   * De-registers listener from CameraService if connected.
   */
  ~QSFCameraIntake();

  /**
   * Callback function for when a new camera buffer is available from QSF.
   *
   * The frame buffer bytes are extracted and then queued up for later processing.
   *
   * @param[in] cameraHandle Handle to the camera providing the data
   * @param[in] cameraBuffer Buffer of camera data
   */
  void onFrame(camera_handle_t cameraHandle, camera_buffer_t& cameraBuffer);

  void setFrameRate(int32_t frameRate) override;

 protected:
  bool doStart() override;
  bool doStop() override;

 private:
  // camera frame callback
  static void processCameraData(camera_handle_t cameraHandle,
                                camera_buffer_t* cameraBuffer,
                                void* arg);

  // initialize the intake.
  bool initialize();

  // Clean up the intake.
  bool cleanup();

  // query the info for available cameras.
  void checkCameras();

  // Convert the camera frame data to publishing frame format.
  std::unique_ptr<cv::Mat> convertFrame(camera_buffer_t& cameraBuffer);

  // available camera info
  unsigned int availableCameras_;
  std::vector<cameraInfo_t> availableCameraInfos_;

  // supported camera info
  // only cameras that the class supports go in this list
  std::vector<cameraInfo_t> supportedCameraInfos_;

  // current supported camera index
  unsigned int currentCamera_;

  // whether the class is initialized or not
  bool isInitialized_;

  screen_context_t   screenContext_;

  // frame buffer size
  uint32_t frameSize_ = 0;
};
