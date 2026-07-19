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

#include <processors/CameraFrameIntake.h>
#include <processors/ProcessingContext.h>

CameraFrameIntake::CameraFrameIntake() : Processor(),
                                         frameRate_(0),
                                         isStarted_(false),
                                         totalFrames_(0) {
  NOTICE() << "CameraFrameIntake()" << FLUSH;
}

CameraFrameIntake::~CameraFrameIntake() {
  NOTICE() << "~CameraFrameIntake()" << FLUSH;
}

bool CameraFrameIntake::start() {
  NOTICE() << "start()" << FLUSH;

  if (isStarted_) {
    WARNING() << "Intake has already been started." << FLUSH;
    return true;
  }

  isStarted_ = doStart();

  return isStarted_;
}

bool CameraFrameIntake::stop() {
  NOTICE() << "stop()" << FLUSH;

  if (!isStarted_) {
    WARNING() << "Intake has already been stopped." << FLUSH;
    return true;
  }

  isStarted_ = !doStop();

  return !isStarted_;
}

void CameraFrameIntake::setFrameRate(int32_t frameRate) {
  frameRate_ = frameRate;
}

void CameraFrameIntake::checkFrameRate() {
  const int64_t INTERVAL_IN_MILLIS = 1000;
  auto tp = std::chrono::high_resolution_clock::now();
  auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(tp - lastFrameTimestamp_).count();
  totalFrames_++;
  if (timeDiff > 5 * INTERVAL_IN_MILLIS) {
    double currentFrameRate = totalFrames_ * INTERVAL_IN_MILLIS / timeDiff;
    INFO() << "Current frame rate: " << currentFrameRate << FLUSH;
    lastFrameTimestamp_ = tp;
    totalFrames_ = 0;
  }
}

void CameraFrameIntake::process() {
  NOTICE() << "process()" << FLUSH;
}
