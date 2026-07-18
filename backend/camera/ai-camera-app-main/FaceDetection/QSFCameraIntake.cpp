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

#include <libgen.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <unistd.h>

#include <Logging.h>
#include <processors/CameraFrameProcessingContext.h>
#include <processors/QSFCameraIntake.h>

#define DEBUG_INTAKE 0   // set to 1 to enable extra logging

static const int NUM_BUFFERS = 3;

// Status callback - called for asynchronous events
void statusCallback(camera_handle_t handle, camera_devstatus_t devstatus, uint16_t extra, void *arg) {
  DEBUG() << "status params: handle: " << handle << " devstatus: "
          << devstatus << " extra: " << extra << " arg: " << arg
          << FLUSH;
}

using std::string;
using std::vector;
using std::ostringstream;

using std::literals::chrono_literals::operator""s;

using cv::Mat;
using cv::COLOR_YUV2RGB_UYVY;
using cv::COLOR_YUV2RGB_YUY2;
using cv::COLOR_RGBA2RGB;
using cv::COLOR_BGRA2RGB;

QSFCameraIntake::QSFCameraIntake() : CameraFrameIntake(),
                                     availableCameras_(0), currentCamera_(-1),
                                     isInitialized_(false) {
  NOTICE() << "QSFCameraIntake()" << FLUSH;
}

QSFCameraIntake::~QSFCameraIntake() {
  NOTICE() << "~QSFCameraIntake()" << FLUSH;
  if (isStarted_) {
    stop();
  }

  shutdown();
}

void QSFCameraIntake::processCameraData(camera_handle_t cameraHandle,
                                        camera_buffer_t* cameraBuffer,
                                        void* arg) {
  DEBUG() << "processCameraData()" << FLUSH;

  QSFCameraIntake *intake = reinterpret_cast<QSFCameraIntake*>(arg);
  intake->onFrame(cameraHandle, *cameraBuffer);
}

// initialize QNX Camera library
bool QSFCameraIntake::initialize() {
  int err = EOK;
  int i;

  NOTICE() << "initialize()" << FLUSH;

  // ensure that initialize() has not already been called
  if (isInitialized_) {
      WARNING() << "QSF Camera Intake has already been initialized." << FLUSH;

      return true;
  }

  // query number of available camera inputs
  INFO() << "Query number of available cameras ..." << FLUSH;

  // Wait for cameras to be ready
  i = 0;
  while (availableCameras_ < MINIMUM_NUMBER_CAMERAS) {
    err = camera_get_supported_cameras(0, &availableCameras_, nullptr);
    if (err != EOK) {
      ERROR() << "Error getting available cameras: err=" << err << FLUSH;
      break;
    }
    // Poll and try again until we timeout
    INFO() << "Number of available cameras: " << availableCameras_ << FLUSH;
    if (availableCameras_ < MINIMUM_NUMBER_CAMERAS) {
      if (i++ >= CAMERA_MAX_POLL_LOOPS) {
        err = ETIMEDOUT;
        ERROR() << "Timed out waiting for at least " << MINIMUM_NUMBER_CAMERAS
                << " cameras to be available." << FLUSH;
        break;
      }
      usleep(CAMERA_POLL_TIME_US);
    }
  }

  if (err == EOK) {
    // check for available cameras on first startup only ...
    // query available camera info to find one camera to connect to
    checkCameras();

    // check if we have at least one camera with the supported colour format.
    if (supportedCameraInfos_.size() < MINIMUM_NUMBER_CAMERAS) {
      ERROR() << "No cameras with the supported colour format." << FLUSH;
      err = ENOTSUP;
    }
  }

  // Setup each camera
  if (err == EOK) {
    for (auto it = supportedCameraInfos_.begin();
         it != supportedCameraInfos_.end(); ++it) {
      // Open the camera
      err = camera_open(it->cameraUnit, CAMERA_MODE_RO | CAMERA_MODE_ROLL, &(it->cameraHandle));
      if (err != EOK) {
        ERROR() << "Failed to open the camera " << it->cameraUnit << " : err = " << err << FLUSH;
        break;
      } else {
        it->cameraFlags |= CLEANUP_CAMERA_HANDLE;
        INFO() << "opened camera handle: " << it->cameraHandle << FLUSH;
      }

      if (err == EOK) {
        // We do not require a window be created for us
        err = camera_set_vf_property(it->cameraHandle, CAMERA_IMGPROP_CREATEWINDOW, false);
        if (err != EOK) {
          ERROR() << "Failed to disable creating a window for camera "
                  << it->cameraUnit << " : err = " << err << FLUSH;
        }
      }

      if (err == EOK) {
        // Start the viewfinder for each camera if successful so far
        // Start the viewfinder - registers status callback and camera frame callback
        err = camera_start_viewfinder(it->cameraHandle, processCameraData,
                                      statusCallback, this);
        if (err != EOK) {
          ERROR() << "Failed to start viewfinder for camera "
                  << it->cameraUnit << ": err = " << err << FLUSH;
        } else {
          it->cameraFlags |= CLEANUP_VIEWFINDER;
        }
      }

      // Close the camera if a failure occurred above
      if (err != EOK) {
        (void)camera_close(it->cameraHandle);
        if (err != EOK) {
          ERROR() << "Failed to close camera handle: "
                  << it->cameraHandle
                  << " err: = " << err << FLUSH;
        }
        break;
      }
    }
  }

  if (err == EOK) {
    isInitialized_ = true;
  }

  return isInitialized_;
}

bool QSFCameraIntake::cleanup() {
  NOTICE() << "cleanup()" << FLUSH;

  ostringstream infoStr;
  ostringstream errStr;

  // ensure that initialize() has not been previously been called
  if (!isInitialized_) {
    WARNING() << "QSF Camera intake has already been cleaned up." << FLUSH;
    return true;
  }

  int err = EOK;

  // Stop everything we need on exit
  for (auto it = supportedCameraInfos_.begin();
       it != supportedCameraInfos_.end(); ++it) {
    if (it->cameraFlags & CLEANUP_VIEWFINDER) {
      err = camera_stop_viewfinder(it->cameraHandle);
      if (err != EOK) {
        ERROR() << "Failed to stop viewfinder: err = " << err << FLUSH;
      }
    }
    if (it->cameraFlags & CLEANUP_CAMERA_HANDLE) {
      INFO() << "closing camera handle: " << it->cameraHandle << FLUSH;
      err = camera_close(it->cameraHandle);
      if (err != EOK) {
        ERROR() << "Failed to close handle: err = " << err << FLUSH;
      }
    }
  }

  if (err == EOK) {
    // clear camera info lists
    availableCameraInfos_.clear();
    supportedCameraInfos_.clear();

    INFO() << "cleanup() successful." << FLUSH;
    isInitialized_ = false;
  }

  return !isInitialized_;
}

bool QSFCameraIntake::doStart() {
  NOTICE() << "doStart()" << FLUSH;

  int retryCount = 0;
  bool isStarted = false;
  do {
    INFO() << "Initializing ..." << FLUSH;

    // delay a little to prevent QSF from locking up ...
    std::this_thread::sleep_for(0.25s);

    if (initialize()) {
      INFO() << "Initialization was successful." << FLUSH;

      // (there should only be one for the time being but we will loop anyways)
      for (auto it = supportedCameraInfos_.begin();
           it != supportedCameraInfos_.end(); ++it) {
        // set up camera frame properties from supported camera
        cameraFrameWidth_  = it->cameraViewfinderWidth;
        cameraFrameHeight_ = it->cameraViewfinderHeight;
        frameRate_         = it->cameraViewfinderFramerate;

        // calculate frame size based on the supported frame formats
        if (it->cameraViewfinderFormat == CAMERA_FRAMETYPE_CBYCRY ||
            it->cameraViewfinderFormat == CAMERA_FRAMETYPE_YCBYCR) {
          frameSize_ = it->cameraViewfinderWidth * 2 * it->cameraViewfinderHeight;
        } else if (it->cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB888) {
          frameSize_ = it->cameraViewfinderWidth * 3 * it->cameraViewfinderHeight;
        } else if (it->cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB8888 || it->cameraViewfinderFormat == CAMERA_FRAMETYPE_BGR8888) {
          frameSize_ = it->cameraViewfinderWidth * 4 * it->cameraViewfinderHeight;
        }

        INFO() << "Camera frame width: " << cameraFrameWidth_ << ", height: " << cameraFrameHeight_ << FLUSH;
        INFO() << "Frame size: " << frameSize_ << FLUSH;

        isStarted = true;
      }
    } else {
      retryCount++;
      if (retryCount > 5) {
        ERROR() << "Intake start retries exhausted." << FLUSH;
        break;
      }

      // Failed to initialize. Retry after 5 seconds.
      ERROR() << "Failed to initialize. Retrying in 5 seconds ..." << FLUSH;
      std::this_thread::sleep_for(5s);
    }
  } while (!isStarted);

  if (!isStarted) {
    ERROR() << "Failed to start QSF Camera Intake" << FLUSH;
  }

  return isStarted;
}

bool QSFCameraIntake::doStop() {
  NOTICE() << "doStop()" << FLUSH;

  return !cleanup();
}

// helper function to populate camInfos_ vector
void QSFCameraIntake::checkCameras() {
  NOTICE() << "checkCameras()" << FLUSH;

  INFO() << "Retrieving info for available cameras ..." << FLUSH;

  int           err = EOK;
  uint32_t      numSupported;

  for (uint32_t i = 0; i < availableCameras_; ++i) {
    INFO() << "Checking camera " << i << " ..." << FLUSH;

    cameraInfo_t checkCamera;
    checkCamera.cameraUnit = static_cast<camera_unit_t>(static_cast<int>(CAMERA_UNIT_1) + i);
    checkCamera.cameraFlags = 0;

    // temporarily open a camera to query available camera info
    err = camera_open(checkCamera.cameraUnit, CAMERA_MODE_RO | CAMERA_MODE_ROLL, &(checkCamera.cameraHandle));
    if (err != EOK) {
      ERROR() << "Failed to open the camera" << checkCamera.cameraUnit << " : err = " << err << FLUSH;
      return;
    }
    INFO() << "checking camera handle: " << checkCamera.cameraHandle << FLUSH;

    if (err == EOK) {
      // Retrieve current viewfinder configuration
      err = camera_get_vf_property(checkCamera.cameraHandle,
                                   CAMERA_IMGPROP_FORMAT, &checkCamera.cameraViewfinderFormat,
                                   CAMERA_IMGPROP_WIDTH, &checkCamera.cameraViewfinderWidth,
                                   CAMERA_IMGPROP_HEIGHT, &checkCamera.cameraViewfinderHeight,
                                   CAMERA_IMGPROP_FRAMERATE, &checkCamera.cameraViewfinderFramerate);
      if (err != EOK) {
        ERROR() << "Failed to obtain vf properties: err = " << err << FLUSH;
      }
    }

    if (err == EOK) {
      // Log current camera properties for debugging purposes ...
      INFO() << "Current resolution: width: " << checkCamera.cameraViewfinderWidth
             << " height: " << checkCamera.cameraViewfinderHeight << FLUSH;
      INFO() << "Current frame rate: " << checkCamera.cameraViewfinderFramerate << FLUSH;
      INFO() << "Current frame format: " << checkCamera.cameraViewfinderFormat << FLUSH;

      // Log supported camera properties for debugging purposes ...
      camera_res_t* supportedResolutions;
      err = camera_get_supported_vf_resolutions(checkCamera.cameraHandle, 0, &numSupported, nullptr);
      if (err != EOK) {
        ERROR() << "Failed to get number of supported resolutions: err = " << err << FLUSH;
      } else {
        supportedResolutions = reinterpret_cast<camera_res_t*>(calloc(sizeof(camera_res_t), numSupported));
        if (supportedResolutions == nullptr) {
          ERROR() << "Failed to allocate supported resolutions" << FLUSH;
        } else {
          err = camera_get_supported_vf_resolutions(checkCamera.cameraHandle, numSupported,
                                                    &numSupported, supportedResolutions);
          if (err != EOK) {
            ERROR() << "Failed to get list of supported resolutions: err = " << err << FLUSH;
            free(supportedResolutions);
          } else {
            for (uint32_t j = 0; j < numSupported; j++) {
              INFO() << "Supported resolution: width: " << supportedResolutions[j].width
                     << " height: " << supportedResolutions[j].height << FLUSH;
            }
            free(supportedResolutions);

            // Log supported frame rates for debugging purposes ...
            double* supportedFramerates;
            bool maxMin;
            err = camera_get_supported_vf_framerates(checkCamera.cameraHandle, checkCamera.cameraViewfinderFormat,
                                                     0, &numSupported, nullptr, &maxMin);
            if (err != EOK) {
              ERROR() << "Failed to get number of supported framerates: err = " << err << FLUSH;
            } else {
              supportedFramerates = reinterpret_cast<double*>(calloc(sizeof(double), numSupported));
              if (supportedFramerates == nullptr) {
                ERROR() << "Failed to allocate supported framerates" << FLUSH;
              } else {
                err = camera_get_supported_vf_framerates(checkCamera.cameraHandle, checkCamera.cameraViewfinderFormat,
                                                         numSupported, &numSupported, supportedFramerates, &maxMin);
                if (err != EOK) {
                  ERROR() << "Failed to get list of supported framerates: err = " << err << FLUSH;
                  free(supportedFramerates);
                } else {
                  for (uint32_t j = 0; j < numSupported; j++) {
                    INFO() << "Supported frame rate: " << supportedFramerates[j] << FLUSH;
                  }
                  free(supportedFramerates);
                }
              }
            }
          }
        }
      }
    }

    if (err == EOK) {
      // currently we are only configuring one camera to query for camera frames
      // and it is the first one we find with a supported frame format
      // (this will likely change in the future to support more advanced use cases)
      if (supportedCameraInfos_.size() < MINIMUM_NUMBER_CAMERAS) {
        cameraInfo_t selectedCamera = checkCamera;
        // Init cleanup flags - we will destroy the context and stop event thread only on the last camera
        // Only one camera is currently supported so it will be for the first camera by default
        selectedCamera.cameraFlags = CLEANUP_CONTEXT | CLEANUP_EVENT_THREAD;

        selectedCamera.cameraID = ("QNXCamera_" + std::to_string(i));

        // For now, only support the color format CAMERA_FRAMETYPE_CBYCRY, CAMERA_FRAMETYPE_YCBYCR and
        // CAMERA_FRAMETYPE_RGB8888, and CAMERA_FRAMETYPE_RGB888
        if (selectedCamera.cameraViewfinderFormat == CAMERA_FRAMETYPE_CBYCRY ||
          selectedCamera.cameraViewfinderFormat == CAMERA_FRAMETYPE_YCBYCR ||
          selectedCamera.cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB888 ||
          selectedCamera.cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB8888 ||
          selectedCamera.cameraViewfinderFormat == CAMERA_FRAMETYPE_BGR8888) {
          supportedCameraInfos_.push_back(selectedCamera);
          currentCamera_ = i;

          // Log current viewfinder / camera properties
          INFO() << "Supported Camera Unit: " << selectedCamera.cameraHandle << FLUSH;
          INFO() << "Supported Camera Handle: " << selectedCamera.cameraHandle << FLUSH;
          INFO() << "Supported Camera ID: " << selectedCamera.cameraID << FLUSH;
          INFO() << "Supported Camera Resolution: width: " << selectedCamera.cameraViewfinderWidth << " height: "
                 << selectedCamera.cameraViewfinderHeight << FLUSH;
          INFO() << "Supported Camera frame rate: " << selectedCamera.cameraViewfinderFramerate << FLUSH;
          INFO() << "Supported Camera image format: " << selectedCamera.cameraViewfinderFormat << FLUSH;
          INFO() << "Supported Camera format type: "
                 << static_cast<int>(selectedCamera.cameraViewfinderFormat) << FLUSH;
        } else {
          WARNING() << "Available camera at index " << i << " has an unsupported color format, skipping..."
                    << FLUSH;
        }
      }
    }

    // Close the camera for now.  It will be reopened in the following steps
    // if a suitable camera is found.
    err = camera_close(checkCamera.cameraHandle);
    if (err != EOK) {
      ERROR() << "Failed to close handle: err = " << err << FLUSH;
      return;
    }
    INFO() << "finished checking camera handle: " << checkCamera.cameraHandle << FLUSH;

    availableCameraInfos_.push_back(checkCamera);
  }
}

std::unique_ptr<cv::Mat> QSFCameraIntake::convertFrame(camera_buffer_t& cameraBuffer) {
#if DEBUG_INTAKE
  NOTICE() << "convertFrame()" << FLUSH;
#endif

  std::unique_ptr<cv::Mat> image = nullptr;
  if (cameraBuffer.frametype != supportedCameraInfos_[currentCamera_].cameraViewfinderFormat) {
    // this shouldn't happen ...
    ERROR() << "Frame buffer and camera frame type don't match!" << FLUSH;
    return image;
  }

  void* frameData = cameraBuffer.framebuf;
  if (frameData == nullptr) {
    ERROR() << "Frame buffer pointer returned is null." << FLUSH;
    return image;
  }

  // Convert frame to RGB. Note: assumes that there is no padding and the rows of pixels are store consecutively.
  if (supportedCameraInfos_[currentCamera_].cameraViewfinderFormat == CAMERA_FRAMETYPE_CBYCRY) {
    // convert CBYCRY to RGB expected by ML model
    cv::Mat yuvImage(cameraFrameHeight_,
                     cameraFrameWidth_, CV_8UC2,
                     frameData,
                     cameraFrameWidth_ * 2);
    image = std::make_unique<cv::Mat>();
    cvtColor(yuvImage, *image, cv::COLOR_YUV2RGB_UYVY);
  } else if (supportedCameraInfos_[currentCamera_].cameraViewfinderFormat == CAMERA_FRAMETYPE_YCBYCR) {
    // convert YCBYCR to RGB expected by ML model
    cv::Mat yuvImage(cameraFrameHeight_,
                     cameraFrameWidth_, CV_8UC2,
                     frameData,
                     cameraFrameWidth_ * 2);
    image = std::make_unique<cv::Mat>();
    cvtColor(yuvImage, *image, COLOR_YUV2RGB_YUY2);
  } else if (supportedCameraInfos_[currentCamera_].cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB888) {
    // since this is a one step bundling process, we need to copy the frame buffer for RGB frame buffers
    image = std::make_unique<cv::Mat>();
    image->create(cameraFrameHeight_, cameraFrameWidth_, CV_8UC3);
    memcpy(reinterpret_cast<char*>(image->data),
           reinterpret_cast<char*>(frameData),
           frameSize_);
  } else if (supportedCameraInfos_[currentCamera_].cameraViewfinderFormat == CAMERA_FRAMETYPE_RGB8888) {
    // convert RGB8888 to RGB expected by ML model
    const int fromTo[8] { 0, 2, 1, 1, 2, 0, 3, 3 };
    cv::Mat argbImage(cameraFrameHeight_,
                      cameraFrameWidth_, CV_8UC4,
                      frameData,
                      cameraFrameWidth_ * 4);
    cv::Mat bgraImage(argbImage.size(), argbImage.type());
    cv::mixChannels(&argbImage, 1, &bgraImage, 1, fromTo, 4);
    image = std::make_unique<cv::Mat>();
    cvtColor(bgraImage, *image, COLOR_RGBA2RGB);
  } else if (supportedCameraInfos_[currentCamera_].cameraViewfinderFormat == CAMERA_FRAMETYPE_BGR8888) {
    // convert BGRA8888 to RGB expected by ML model
    cv::Mat bgraImage(cameraFrameHeight_,
                      cameraFrameWidth_, CV_8UC4,
                      frameData,
                      cameraFrameWidth_ * 4);
    image = std::make_unique<cv::Mat>();
    cvtColor(bgraImage, *image, COLOR_BGRA2RGB);
  }

  return std::move(image);
}

void QSFCameraIntake::onFrame(camera_handle_t cameraHandle, camera_buffer_t& cameraBuffer) {
  DEBUG() << "onFrame()" << FLUSH;

  for (auto it = supportedCameraInfos_.begin();
       it != supportedCameraInfos_.end(); ++it) {
    if (it->cameraHandle == cameraHandle) {
      // convert camera frame if it is from supported camera
      std::unique_ptr<cv::Mat> image = convertFrame(cameraBuffer);

      // pass the camera frame to the next processor
      if (image.get() != nullptr && nextProcessor_ != nullptr) {
        auto cameraFrameProcessingContext =
          std::make_shared<CameraFrameProcessingContext>();
        cameraFrameProcessingContext->type_ = TYPE_CAMERA_FRAME_PROCESSING;
        cameraFrameProcessingContext->startTime_ = Processor::getNextProcessingEventTimestamp();
        cameraFrameProcessingContext->eventNumber_ = Processor::getNextProcessingEventSequenceNumber();
        cameraFrameProcessingContext->processedCount_ = 1;
        cameraFrameProcessingContext->frame_ = std::move(image);

        nextProcessor_->passData(cameraFrameProcessingContext);
      } else {
        WARNING() << "No supported frame format to convert.  Aborting." << FLUSH;
      }

      checkFrameRate();
    }
    return;
  }

  WARNING() << "Received camera frame from an unexpected source." << FLUSH;
}

void QSFCameraIntake::setFrameRate(int32_t frameRate) {
  NOTICE() << "setFrameRate(" << frameRate << ")" << FLUSH;

  CameraFrameIntake::setFrameRate(frameRate);
}
