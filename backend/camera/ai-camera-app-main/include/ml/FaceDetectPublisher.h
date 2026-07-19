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

#include <PublishDelegate.h>
#include <processors/Processor.h>

class FaceDetectPublisher : public Processor {
 public:
  explicit FaceDetectPublisher(PublishDelegate *publishDelegate);

  virtual ~FaceDetectPublisher();
  FaceDetectPublisher(const FaceDetectPublisher&) = default;
  FaceDetectPublisher& operator=(const FaceDetectPublisher&) = default;
  FaceDetectPublisher(FaceDetectPublisher&&) = default;
  FaceDetectPublisher& operator=(FaceDetectPublisher&&) = default;

 protected:
  // Process a unit of work.
  void process() override;

  // pointer to publishing delegate
  PublishDelegate *publishDelegate_ = nullptr;
};
