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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

#include <processors/Processor.h>
#include <processors/ProcessingContext.h>

class CameraFrameIntake : public Processor {
 public:
  /**
   * Construct a new Camera Frame Intake object
   */
  CameraFrameIntake();

  /**
   * Destroy the Camera Frame Intake object.
   *
   * The intake is stopped, if already started.
   */
  virtual ~CameraFrameIntake();

  /**
   * Check frame rate and update average frame rate.
   *
   */
  void checkFrameRate();

  /**
   * Start the camera frame intake.
   * Some initialization in this base class and derived class will be 
   * triggered by this operation.
   *
   * If not already connected, retries until successful
   * (number of retries varies depending on the derived class).
   *
   * @return bool true if the intake was started successfully, false otherwise
   */
  bool start();

  /**
   * Stop the camera frame intake.
   * Some cleanup in this base class and derived class will be 
   * triggered by this operation.
   *
   * @return bool true if the intake was stopped successfully, false otherwise
   */
  bool stop();

  virtual void setFrameRate(int32_t frameRate);

  /**
   * Getters for camera frame height and width
   */
  inline int getCameraFrameWidth() const noexcept {
    return cameraFrameWidth_;
  }

  inline int getCameraFrameHeight() const noexcept {
    return cameraFrameHeight_;
  }

  /**
   * Process a unit of work.
   */
  void process() override;

 protected:
  // derived class must perform startup logic here and
  // return true if successful, false otherwise
  virtual bool doStart() = 0;

  // derived class must perform stop logic here and
  // return true if successful, false otherwise
  virtual bool doStop() = 0;

  // frame data
  std::vector<uint8_t> frame_ = {};

  // Width property for the camera frame intake
  int32_t cameraFrameWidth_ = 0;

  // Height property for the camera frame intake
  int32_t cameraFrameHeight_ = 0;

  /**
   * The frame rate to request from the source
   */
  int32_t frameRate_ = 0;

  // Tracks whether the intake is started
  std::atomic<bool> isStarted_{false};

  // Fields for logging frame rate
  std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTimestamp_;
  int64_t totalFrames_ = 0;
};
