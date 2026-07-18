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

#include <vector>

#include <ml/FaceDetect.h>

class FaceDetectFar : public FaceDetect {
 public:
  explicit FaceDetectFar(bbry::poc::ModelExecutorManager* modelExecutorManager);

  ~FaceDetectFar() override;
  FaceDetectFar(const FaceDetectFar&) = default;
  FaceDetectFar& operator=(const FaceDetectFar&) = default;
  FaceDetectFar(FaceDetectFar&&) = default;
  FaceDetectFar& operator=(FaceDetectFar&&) = default;

  /**
   * Perform an inference for this ML model
   *
   * @param[in] features the input tensor data for the inference operation
   * @return std::vector<std::vector<float>> The returned output tensor data.
   */
  std::vector<std::vector<float>> infer(
    const std::vector<float> &features,
    const unsigned int cameraFrameWidth,
    const unsigned int cameraFrameHeight) override;

 protected:
  /**
   * Process results specific to derived face detection model.
   */
  void processResult(FaceProcessingContext &faceProcessingContext,
                     std::vector<std::vector<float>>& result) override;
};
