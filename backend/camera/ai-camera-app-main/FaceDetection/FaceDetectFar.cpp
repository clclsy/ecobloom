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
#include <ml/FaceDetectFar.h>

// blazeface_longrange parameters
#define MODEL_NAME "blazeface_fullrange"
#define MODEL_FILE "mlModels/tflite/face_detection_full_range.tflite"
#define INPUT_IMAGE_HEIGHT 192
#define INPUT_IMAGE_WIDTH 192
#define TENSOR_NAME_DETECTIONS "reshaped_regressor_face_4"
#define TENSOR_NAME_CLASSIFIERS "reshaped_classifier_face_4"

// SSD grid size info
#define SSD_GRID_HEIGHT 4
#define SSD_GRID_WIDTH 4

// detection elements size
#define DETECTION_ELEMENTS_SIZE 16

FaceDetectFar::FaceDetectFar(bbry::poc::ModelExecutorManager* modelExecutorManager) :
                             FaceDetect(modelExecutorManager,
                                       MODEL_NAME, MODEL_FILE,
                                       INPUT_IMAGE_WIDTH, INPUT_IMAGE_HEIGHT,
                                       TENSOR_NAME_CLASSIFIERS, TENSOR_NAME_DETECTIONS) {
  NOTICE() << "FaceDetectFar()" << FLUSH;
}

FaceDetectFar::~FaceDetectFar() {
  NOTICE() << "~FaceDetectFar()" << FLUSH;

  shutdown();
}

std::vector<std::vector<float>> FaceDetectFar::infer(
  const std::vector<float> &features,
  const unsigned int cameraFrameWidth,
  const unsigned int cameraFrameHeight) {
  NOTICE() << "infer()" << FLUSH;

  // don't pass the input tensor name so that it is extracted from the ML model itself
  modelExecutor_->setInput(features);
  // pass empty as string for input tensor name so that it is extracted from the ML model itself

  std::vector<std::vector<float>> detectionResults;
  if (modelExecutor_->infer()) {
    // inference succeeded - process and return results
    auto detections  = modelExecutor_->getOutput(detectionsTensorName_);
    auto confidences = modelExecutor_->getOutput(classifierTensorName_);

    float SCALE_FACTOR_Y = static_cast<float>(cameraFrameHeight /
                           static_cast<float>(inputHeight_));
    float SCALE_FACTOR_X = static_cast<float>(cameraFrameWidth /
                           static_cast<float>(inputWidth_));

    // extract detection details from first tensor based on detections indicated in second output tensor
    // second tensor has entries for SSD_GRID_WIDTHxSSD_GRID_HEIGHT pixel buckets for
    // inputWidth_ x inputHeight_ input frame containing confidence scores
    for (int j = 0; j < static_cast<int>(inputHeight_/SSD_GRID_HEIGHT); j++) {
      for (int k = 0; k < static_cast<int>(inputWidth_/SSD_GRID_WIDTH); k++) {
        int index = j * static_cast<int>(inputHeight_/SSD_GRID_HEIGHT) + k;
        // if confidence is greater than 0, extract corresponding detection details from first output tensor
        if (confidences[index] > 0.0) {
          std::vector<float> detectionDetails;
          detectionDetails.push_back(confidences[index]);

          // calculate SSD center point and add to output
          auto dcx = k * SSD_GRID_WIDTH + static_cast<float>(SSD_GRID_WIDTH)/2;
          auto dcy = j * SSD_GRID_HEIGHT + static_cast<float>(SSD_GRID_HEIGHT)/2;
          float dlx = dcx + detections[index * DETECTION_ELEMENTS_SIZE]
                          - detections[index * DETECTION_ELEMENTS_SIZE + 2]/2.0;
          float dty = dcy + detections[index * DETECTION_ELEMENTS_SIZE + 1]
                          - detections[index * DETECTION_ELEMENTS_SIZE + 3]/2.0;
          float drx = dcx + detections[index * DETECTION_ELEMENTS_SIZE]
                          + detections[index * DETECTION_ELEMENTS_SIZE + 2]/2.0;
          float dby = dcy + detections[index * DETECTION_ELEMENTS_SIZE + 1]
                          + detections[index * DETECTION_ELEMENTS_SIZE + 3]/2.0;

          // add bounding box coordinates to output
          detectionDetails.push_back(dlx * SCALE_FACTOR_X);
          detectionDetails.push_back(dty * SCALE_FACTOR_Y);
          detectionDetails.push_back(drx * SCALE_FACTOR_X);
          detectionDetails.push_back(dby * SCALE_FACTOR_Y);

          // add keypoints (last twelve) coordinates to output
          for (int l = index * 16 + 4; l < (index + 1) * 16; l+= 2) {
            float dx = dcx + detections[l];
            float dy = dcy + detections[l+1];
            detectionDetails.push_back(dx * SCALE_FACTOR_X);
            detectionDetails.push_back(dy * SCALE_FACTOR_Y);
          }
          detectionResults.push_back(std::move(detectionDetails));
        }
      }
    }
  }

  return detectionResults;
}

void FaceDetectFar::processResult(FaceProcessingContext &faceProcessingContext,
                   std::vector<std::vector<float>>& result) {
  NOTICE() << "processResult()" << FLUSH;

  if (result.size() > 0) {
    faceProcessingContext.farFaceDetections_ = std::move(result);
  }
}

