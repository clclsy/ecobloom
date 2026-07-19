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

#include <processors/CameraFrameProcessingContext.h>

#define TYPE_FACE_PROCESSING    2

/**
 * Context that extends camera frame processing context to include
 * information about face detection between processors.
 */
struct FaceProcessingContext : public CameraFrameProcessingContext {
  // ML input data to process
  std::vector<float> faceMlModelInput_ = {};

  // face detections (near and far)
  std::vector<std::vector<float>> nearFaceDetections_ = {};
  std::vector<std::vector<float>> farFaceDetections_ = {};
};
