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

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Logging.h>
#include <ml/FaceDetect.h>

using bbry::poc::ModelExecutor;
using bbry::poc::ModelExecutorManager;

// tensor value thresholds for input tensor
#define TENSOR_VAL_MAX 1.0
#define TENSOR_VAL_MIN -1.0

FaceDetect::FaceDetect(ModelExecutorManager* modelExecutorManager,
                       const std::string &modelName,
                       const std::string &modelFile,
                       const int inputWidth, const int inputHeight,
                       const std::string &classifierTensorName,
                       const std::string &detectionsTensorName) :
                       inputWidth_(inputWidth), inputHeight_(inputHeight),
                       classifierTensorName_(classifierTensorName),
                       detectionsTensorName_(detectionsTensorName),
                       modelExecutorManager_(modelExecutorManager),
                       totalTicks_(0) {
  NOTICE() << "FaceDetect()" << FLUSH;

#if LOCAL_TFLITE_MODELS
  std::string fullModelFilePath = modelFile;
#else
  std::string fullModelFilePath = std::string(getenv("SYNTHETIC_SENSOR_INSTALL_DIR")) + "/face_detections/" + modelFile;
#endif
  INFO() << "model path: " << fullModelFilePath << FLUSH;

  modelExecutor_ = modelExecutorManager->borrowExecutor(modelName, fullModelFilePath);
  if (modelExecutor_ == nullptr) {
    ERROR() << "Error borrowing a ML model executor!" << FLUSH;
    throw std::logic_error("Error borrowing a ML model executor!");
  } else {
    // specify an input tensor shape mapping for loading the ML model later in the flow
    std::vector<size_t> shapeMap = {static_cast<size_t>(1),
                                    static_cast<size_t>(inputWidth),
                                    static_cast<size_t>(inputHeight),
                                    static_cast<size_t>(3)};
    modelExecutor_->updateShapeMap(SHAPE_MAP_INPUT, shapeMap);
  }
}

FaceDetect::~FaceDetect() {
  NOTICE() << "~FaceDetect()" << FLUSH;

  modelExecutorManager_->returnExecutor(modelExecutor_);
  modelExecutor_ = nullptr;
}

void FaceDetect::process() {
  NOTICE() << "process()" << FLUSH;

  // attempt to load the ML model before processing data
  if (modelExecutor_ != nullptr) {
    if (!modelExecutor_->loadMLModel()) {
      ERROR() << "Error loading the ML model.  Aborting ..." << FLUSH;
      throw std::logic_error("Error loading the ML model.  Aborting ...");
    }
  }

  while (!stopProcessing_) {
    std::shared_ptr<ProcessingContext> processingContext;

    {
      std::lock_guard<std::mutex> lock(contextLock_);
      processingContext = std::move(processingData_);
      processingData_.reset();
    }

    if (processingContext.get() == nullptr) {
      continue;
    }

    std::shared_ptr<FaceProcessingContext> faceProcessingContext(nullptr);
    if (processingContext->type_ == TYPE_CAMERA_FRAME_PROCESSING) {
      // first time through, create the face processing context
      // from the camera frame processing context
      CameraFrameProcessingContext* cameraFrameProcessingContext =
        static_cast<CameraFrameProcessingContext*>(processingContext.get());
      faceProcessingContext = std::make_shared<FaceProcessingContext>();
      faceProcessingContext->type_ = TYPE_FACE_PROCESSING;
      faceProcessingContext->startTime_ = cameraFrameProcessingContext->startTime_;
      faceProcessingContext->eventNumber_ = cameraFrameProcessingContext->eventNumber_;
      faceProcessingContext->processedCount_ = cameraFrameProcessingContext->processedCount_+1;
      faceProcessingContext->frame_ = std::move(cameraFrameProcessingContext->frame_);
    } else if (processingContext->type_ == TYPE_FACE_PROCESSING) {
      // second or later time through, obtain the face processing context directly
      faceProcessingContext = std::move(std::static_pointer_cast<FaceProcessingContext>(processingContext));
      faceProcessingContext->processedCount_++;
    } else {
      ERROR() << "Unknown Processing context detected.  Aborting ..." << FLUSH;
      throw std::logic_error("Unknown Processing Context detected.  Aborting ...");
    }

    // resize image for the ML model
    // currently, this has to be redone for each BlazeFace model because the supported
    // frame size is different
    cv::Mat resizedImg;
    cv::resize(*faceProcessingContext->frame_, resizedImg, cv::Size(inputWidth_, inputHeight_));
    cv::Mat floatImg;
    floatImg.create(inputHeight_, inputWidth_, CV_32FC3);
    // Convert uint8_t data range to tensor input range (-1.0 : 1.0)
    resizedImg.convertTo(floatImg, CV_32FC3, (TENSOR_VAL_MAX - TENSOR_VAL_MIN)/255.0, TENSOR_VAL_MIN);

    size_t dataSize = floatImg.total() * floatImg.elemSize();
    faceProcessingContext->faceMlModelInput_.resize(dataSize);
    memcpy(&faceProcessingContext->faceMlModelInput_[0], &floatImg.data[0], floatImg.total() * floatImg.elemSize());

    auto result = infer(faceProcessingContext->faceMlModelInput_,
                        faceProcessingContext->frame_->size().width,
                        faceProcessingContext->frame_->size().height);

    processResult(*faceProcessingContext, result);

    // Log inference rate of of ML model
    checkInferenceRate();

    // pass data to the next processor in the chain
    nextProcessor_->passData(faceProcessingContext);
  }
}

void FaceDetect::checkInferenceRate() {
  const int64_t INTERVAL_IN_MILLIS = 1000;
  auto tp = std::chrono::high_resolution_clock::now();
  auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(tp - lastInferenceTimestamp_).count();
  totalTicks_++;
  if (timeDiff > 5 * INTERVAL_IN_MILLIS) {
    double currentFrameRate = totalTicks_ * INTERVAL_IN_MILLIS / timeDiff;
    INFO() << "Current inference rate: " << currentFrameRate << FLUSH;
    lastInferenceTimestamp_ = tp;
    totalTicks_ = 0;
  }
}
