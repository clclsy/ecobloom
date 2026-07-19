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

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <Logging.h>
#include <ml/FaceDetectFar.h>
#include <ml/FaceDetectPublisher.h>
#include <ml/TFLiteExecutor.h>
#include <processors/ProcessingContext.h>
#include <FaceDetections.h>
#include "Render.h"

using nlohmann::json;

FaceDetections::FaceDetections() : PublishDelegate() {
  NOTICE() << "FaceDetections()" << FLUSH;
}

FaceDetections::~FaceDetections() {
  NOTICE() << "~FaceDetections()" << FLUSH;
}

void FaceDetections::start() {
  NOTICE() << "start()" << FLUSH;

  // create the processing pipeline the first time through
  if (intake_.get() == nullptr) {
    // create camera frame intake to obtain camera frames directly from QSF (QNX Sensor Framework)
    intake_ = std::make_unique<QSFCameraIntake>();

    // create the ML model executor
    modelExecutorManager_ = std::make_unique<bbry::poc::TFLiteExecutorManager>();

    // create long range face detection model and chain it to the intake
    std::unique_ptr<Processor> nextProcessor(new FaceDetectFar(modelExecutorManager_.get()));
    // create face detection insight publisher and chain it to the intake
    std::unique_ptr<Processor> publisher(new FaceDetectPublisher(this));

    // assemble the rest of the processing pipeline
    intake_->chainProcessor(std::move(nextProcessor))
           .chainProcessor(std::move(publisher));
  }

  if (intake_.get() != nullptr) {
    INFO() << "Start the camera frame intake ..." << FLUSH;

    if (!intake_->start()) {
      ERROR() << "Error starting the camera frame intake!" << FLUSH;
    }
  }

  // get camera frame dimensions
  frameWidth_ = intake_->getCameraFrameWidth();
  frameHeight_ = intake_->getCameraFrameHeight();
}

void FaceDetections::stop() {
  NOTICE() << "stop()" << FLUSH;
  // stop the camera frame intake
  INFO() << "Stopping the camera frame intake ..." << FLUSH;

  if (intake_.get() != nullptr) {
    if (!intake_->stop()) {
      ERROR() << "Error stopping the camera frame intake!" << FLUSH;
    }
    // force processor cleanup before this class is destructed to avoid model
    // executor cleanup race condition
    intake_.reset();
  }
}

void FaceDetections::onPublish(nlohmann::json& insightJSON) {
  DEBUG() << "onPublish()" << FLUSH;

  DEBUG() << "face detection insight json: " << insightJSON << FLUSH;
  render_new_face_detection(insightJSON, frameWidth_, frameHeight_);
}
