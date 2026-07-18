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

/*
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
*/

#include <Logging.h>
#include <ml/FaceDetectPublisher.h>
#include <processors/FaceProcessingContext.h>

using nlohmann::json;

FaceDetectPublisher::FaceDetectPublisher(PublishDelegate *publishDelegate) :
                                         publishDelegate_(publishDelegate) {
  NOTICE() << "FaceDetectPublisher()" << FLUSH;
}

FaceDetectPublisher::~FaceDetectPublisher() {
  NOTICE() << "~FaceDetectPublisher()" << FLUSH;

  shutdown();
}

bool doRectanglesOverlap(float r1lx, float r1ty, float r1rx, float r1by,
                         float r2lx, float r2ty, float r2rx, float r2by) {
    // check for non-overlapping X coordinate overlap
    if (r1lx > r2rx) {
        return false;
    }

    if (r2lx > r1rx) {
        return false;
    }

    // check for non-overlapping Y coordinate overlap
    if (r1ty > r2by) {
        return false;
    }

    if (r2ty > r1by) {
        return false;
    }

    return true;
}

void FaceDetectPublisher::process() {
  NOTICE() << "process()" << FLUSH;

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

    std::shared_ptr<FaceProcessingContext> faceProcessingContext =
      std::move(std::static_pointer_cast<FaceProcessingContext>(processingContext));

    // process face detection results from ML models into an insight JSON
    std::vector<std::vector<float>> allDetections =
      std::move(faceProcessingContext->nearFaceDetections_);

    allDetections.reserve(allDetections.size()+faceProcessingContext->farFaceDetections_.size());
    for (auto& detection : faceProcessingContext->farFaceDetections_) {
      allDetections.push_back(std::move(detection));
    }

    std::vector<std::vector<float>> prunedDetections;

    // find best bounding boxes for insight
    // post-process detection data to find all non-overlapping bounding boxes
    // with the highest confidence.  This is to ensure that multiple faces can be
    // reported with unique labels and only the highest confidence bounding boxes
    // are included.
    while (allDetections.size() > 0) {
      auto& detection = allDetections[allDetections.size()-1];
      float dlx = detection[1];
      float dty = detection[2];
      float drx = detection[3];
      float dby = detection[4];

      // eliminate zero area rectangles and restart loop
      if (dlx == drx || dby == dty) {
        allDetections.pop_back();
        continue;
      }

      if (prunedDetections.size() == 0) {
        // always move first found detection to pruned list and restart loop
        prunedDetections.push_back(std::move(detection));
        allDetections.pop_back();
        continue;
      } else {
        int overlaps = 0, replaces = 0;
        size_t lastPrunedSize;

        do {
          // remove pruned detection that overlaps if newer detection has higher confidence
          lastPrunedSize = prunedDetections.size();

          for (auto it1 = std::begin(prunedDetections); it1 != std::end(prunedDetections); ++it1) {
            auto& prunedDetection = *it1;

            float pdlx = prunedDetection[1];
            float pdty = prunedDetection[2];
            float pdrx = prunedDetection[3];
            float pdby = prunedDetection[4];

            // check if current detection overlaps pruned detection
            if (doRectanglesOverlap(dlx, dty, drx, dby, pdlx, pdty, pdrx, pdby)) {
              // only move detection to pruned detections to replace overlapping pruned detection
              // if it has higher confidence
              overlaps++;
              if (detection[0] > prunedDetection[0]) {
                prunedDetections.erase(it1);
                replaces++;
                break;
              }
            }
          }
        } while (prunedDetections.size() != lastPrunedSize);

        if (overlaps == 0 || replaces > 0) {
          // move non-overlapping detection to pruned detections
          prunedDetections.push_back(std::move(detection));
        }

        // erase processed detection
        allDetections.pop_back();
        break;
      }
    }

    // convert detections to insight JSON
    json insightJSON = json::object();
    json detections = json::array();
    int faceIndex = 0;
    for (auto it = std::begin(prunedDetections); it != std::end(prunedDetections); ++it) {
      auto& prunedDetection = *it;
      json detection;
      json detectBox;
      json detectKeypoints;
      std::stringstream label;

      label << "Face " << faceIndex;
      faceIndex++;

      detection["confidence"] = prunedDetection[0];
      detection["label"] = label.str();

      // bounding box consists of top / left, bottom / right coordinates
      detectBox["top"] = static_cast<int>(prunedDetection[2]);
      detectBox["left"] = static_cast<int>(prunedDetection[1]);
      detectBox["bottom"] = static_cast<int>(prunedDetection[4]);
      detectBox["right"] = static_cast<int>(prunedDetection[3]);
      detection["box"] = std::move(detectBox);

      static const char* KEYPOINT_NAME[] = {
        "leftEyeX", "leftEyeY", "rightEyeX", "rightEyeY", "noseTipX", "noseTipY",
        "mouthX", "mouthY", "leftEyeTragionX", "leftEyeTragionY",
        "rightEyeTragionX", "rightEyeTragionY" };

      for (std::size_t index = 0; index < sizeof(KEYPOINT_NAME)/sizeof(KEYPOINT_NAME[0]); ++index) {
        detectKeypoints[KEYPOINT_NAME[index]] = static_cast<int>(prunedDetection[5+index]);
      }
      detection["keypoints"] = std::move(detectKeypoints);

      detections.push_back(std::move(detection));
    }
    insightJSON["detections"] = std::move(detections);

    if (publishDelegate_ != nullptr) {
      publishDelegate_->onPublish(insightJSON);
    }
  }
}
