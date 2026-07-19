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

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <ml/ModelExecutor.h>
#include <processors/Processor.h>
#include <processors/FaceProcessingContext.h>


class FaceDetect : public Processor {
 public:
  FaceDetect(bbry::poc::ModelExecutorManager* modelExecutorManager,
            const std::string &modelName,
            const std::string &modelFile,
            const int inputWidth, const int inputHeight,
            const std::string &classifierTensorName,
            const std::string &detectionsTensorName);

  virtual ~FaceDetect();
  FaceDetect(const FaceDetect&) = default;
  FaceDetect& operator=(const FaceDetect&) = default;
  FaceDetect(FaceDetect&&) = default;
  FaceDetect& operator=(FaceDetect&&) = default;

  /**
   * Perform an inference for this ML model
   *
   * Note: an empty vector is returned if an error occurred
   *
   * @param[in] features the input tensor data for the inference operation
   * @return std::vector<std::vector<float>> The returned output tensor data.
   */
  virtual std::vector<std::vector<float>> infer(
    const std::vector<float>& features,
    const unsigned int cameraFrameWidth,
    const unsigned int cameraFrameHeight) = 0;

 protected:
  // Process a unit of work.
  void process() override;

  /**
   * Process results specific to derived face detection model.
   */
  virtual void processResult(FaceProcessingContext &faceProcessingContext,
                             std::vector<std::vector<float>>& result) = 0;

  /**
   * Check inference rate that machine learning task runs and log 
   * the instantaneous and average rates.
   */
  void checkInferenceRate();

  // input size info
  unsigned int inputWidth_ = 0;
  unsigned int inputHeight_ = 0;

  // output tensor names
  std::string classifierTensorName_;
  std::string detectionsTensorName_;

  // manager for ML executors (from which to obtain ML executors)
  bbry::poc::ModelExecutorManager* modelExecutorManager_ = nullptr;
  // ML executor for ML model
  bbry::poc::ModelExecutor* modelExecutor_ = nullptr;

  // info for checking / logging machine learning model inference rate
  std::chrono::time_point<std::chrono::high_resolution_clock> lastInferenceTimestamp_;
  int64_t totalTicks_ = 0;
};
