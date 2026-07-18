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

#include <memory>

#include <nlohmann/json.hpp>

#include <ml/ModelExecutor.h>
#include <ml/FaceDetectFar.h>
#include <processors/QSFCameraIntake.h>
#include <PublishDelegate.h>

class FaceDetections : public PublishDelegate {
 public:
    FaceDetections();
    ~FaceDetections();

    /**
     * Start the face detections.
     */
    void start();

    /**
     * Stop the face detections.
     */
    void stop();

    /**
     * Publish insight method.
     *
     * @param[in] returned detection data to convert to insight
     */
    void onPublish(nlohmann::json& insightJSON);

 private:
    // The camera frame intake
    std::unique_ptr<CameraFrameIntake> intake_;

    // Frame dimensions
    int frameWidth_;
    int frameHeight_;

    // ML model executor manager
    std::unique_ptr<bbry::poc::ModelExecutorManager> modelExecutorManager_;
};
