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

#include <sstream>
#include <iostream>

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/optional_debug_tools.h>
#include <tensorflow/lite/version.h>

#include <Logging.h>
#include <ml/TFLiteExecutor.h>

using bbry::poc::ModelExecutor;
using bbry::poc::TFLiteExecutor;
using bbry::poc::TFLiteExecutorManager;

// MARK: TFLiteExecutor

TFLiteExecutor::TFLiteExecutor(const std::string& modelName, const std::string& modelFile)
                               : ModelExecutor(modelName, modelFile),
                                 interpreter_(nullptr),
                                 model_(nullptr),
                                 resolver_() {
}

bool TFLiteExecutor::loadMLModel() {
  NOTICE() << "loadMLModel()" << FLUSH;

  std::lock_guard<std::mutex> lock(tfLiteMtx_);

  if (interpreter_.get() != nullptr) {
    return true;
  }

  // load ML model from file
  INFO() << "Loading ML model (" << modelName_ << ") from file " << modelFilePath_
         << FLUSH;
  model_ = tflite::FlatBufferModel::BuildFromFile(modelFilePath_.c_str());

  // instantiate TFLite interpreter
  tflite::InterpreterBuilder builder(*model_, resolver_);
  if (builder(&interpreter_) != kTfLiteOk) {
    ERROR() << "Could not build TFLite interpreter" << FLUSH;
    return false;
  }

  for (int index = 0; index < static_cast<int>(shapeMap_[SHAPE_MAP_INPUT].size()); index++) {
    INFO() << "shape map [" << SHAPE_MAP_INPUT << "] dim #" << index << ": " << shapeMap_[SHAPE_MAP_INPUT][index]
           << FLUSH;
  }

  if (shapeMap_[SHAPE_MAP_INPUT].size() > 0) {
    if (interpreter_->ResizeInputTensorStrict(0, shapeMap_[SHAPE_MAP_INPUT]) != kTfLiteOk) {
      ERROR() << "Unable to set the input shape on TFLite interpreter" << FLUSH;
      return false;
    }
  }

  if (interpreter_->AllocateTensors() != kTfLiteOk) {
    ERROR() << "Could not allocate input tensors" << FLUSH;
    return false;
  }

  if (interpreter_->SetNumThreads(-1) != kTfLiteOk) {
    ERROR() << "Unable to set the thread count on TFLite interpreter" << FLUSH;
    return false;
  }

  // map tensor info and log for debugging purposes
  TensorMetadata inTensorMetadata;
  for (int i = 0; i < static_cast<int>(interpreter_->inputs().size()); i++) {
    const TfLiteTensor* inputTensor = interpreter_->input_tensor(i);
    // support only floats for now
    inTensorMetadata.index = i;
    inTensorMetadata.size = inputTensor->bytes / sizeof(float);
    inTensorMetadata.totalSize = inputTensor->bytes;
    inMetadata_.emplace(interpreter_->GetInputName(i), inTensorMetadata);
    INFO() << "input tensor name: " << interpreter_->GetInputName(i)
               << " index: " << inMetadata_[interpreter_->GetInputName(i)].index
               << " # elements: " << inMetadata_[interpreter_->GetInputName(i)].size
               << " total size: " << inMetadata_[interpreter_->GetInputName(i)].totalSize
               << FLUSH;
    // only one input tensor is currently supported
    //break;
  }

  for (int index = 0; index < static_cast<int>(interpreter_->outputs().size()); index++) {
    const TfLiteTensor* outputTensor = interpreter_->output_tensor(index);
    // support only floats for now
    TensorMetadata outTensorMetadata;
    outTensorMetadata.index = index;
    outTensorMetadata.size = outputTensor->bytes / sizeof(float);
    outTensorMetadata.totalSize = outputTensor->bytes;
    outMetadata_.emplace(interpreter_->GetOutputName(index), outTensorMetadata);
    outTensorNames_.push_back(interpreter_->GetOutputName(index));
  }

  return true;
}

void TFLiteExecutor::reset() {
  NOTICE() << "reset()" << FLUSH;
}

bool TFLiteExecutor::setInput(const std::string& inTensorName, const std::vector<float>& input) {
  NOTICE() << "setInput(): " << inTensorName << " with size: " << input.size() << FLUSH;

  std::lock_guard<std::mutex> lock(tfLiteMtx_);

  std::string tensorName = inTensorName;
  if (tensorName.length() == 0) {
    // obtain input tensor name from loaded ML model
    tensorName = interpreter_->GetInputName(0);
  }

  // Copy the input data to the tensor
  // support only floats for now
  void *inputData = malloc(input.size() * sizeof(float));

  if (inputData == nullptr) {
    return false;
  } else {
    memcpy(inputData, &input[0], input.size() * sizeof(float));

    auto it = inTensors_.find(tensorName);
    if (it != inTensors_.end()) {
      // overwrite
      free(it->second);

      it->second = inputData;
    } else {
      inTensors_.emplace(tensorName, inputData);
    }
  }

  return true;
}

bool TFLiteExecutor::updateShapeMap(const std::string& mapEntryName, std::vector<size_t>& tensorShape) {
  NOTICE() << "updateShapeMap()" << FLUSH;

  std::lock_guard<std::mutex> lock(tfLiteMtx_);

  // Copy the input shape data for the input tensor
  std::vector<int> updateShapeMap;
  for (int index = 0; index < static_cast<int>(tensorShape.size()); index++) {
    updateShapeMap.push_back(static_cast<int>(tensorShape[index]));
  }
  shapeMap_[mapEntryName] = updateShapeMap;

  for (size_t index = 0; index < updateShapeMap.size(); index++) {
    INFO() << "shape map [" << mapEntryName << "] dim #" << index << ": " << updateShapeMap[index]
           << FLUSH;
  }

  return true;
}

bool TFLiteExecutor::infer() {
  NOTICE() << "infer()" << FLUSH;

  if (!loadMLModel()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(tfLiteMtx_);

  // create the input tensor
  for (const auto& inTensorPair : inMetadata_) {
    float* input = interpreter_->typed_input_tensor<float>(inTensorPair.second.index);
    memcpy(input, inTensors_[inTensorPair.first], inTensorPair.second.totalSize);
  }

  // perform the inference
  auto result = interpreter_->Invoke();
  if (result != kTfLiteOk) {
    ERROR() << "Error performing inference. Code: " << result << FLUSH;
    return false;
  }

  for (const auto& outTensorPair : outMetadata_) {
    const std::string& tensorName = outTensorPair.first;
    const TfLiteTensor* outputTensor =
      interpreter_->output_tensor(outTensorPair.second.index);
    void *outputData = malloc(outTensorPair.second.totalSize);
    if (outputData == nullptr) {
      return false;
    } else {
      memcpy(outputData, outputTensor->data.f, outputTensor->bytes);
      auto it = outTensors_.emplace(tensorName, outputData);
      if (!it.second) {
        // entry already exists
        free(it.first->second);
        it.first->second = outputData;
      }
    }
  }

  return true;
}

std::vector<float> TFLiteExecutor::getOutput(const std::string& outTensorName) {
  NOTICE() << "getOutput: " << outTensorName << FLUSH;

  std::lock_guard<std::mutex> lock(tfLiteMtx_);

  // Search by name for the desired output tensor
  const auto& it = outMetadata_.find(outTensorName);
  if (it != outMetadata_.end()) {
    // copy tensor data to the return value
    // only support float tensors for now
    std::vector<float> out;
    out.resize(it->second.size);
    memcpy(&out[0], outTensors_[outTensorName],
           it->second.totalSize);
    return out;
  }

  ERROR() << "Failed to find output tensor: " << outTensorName << FLUSH;

  return {};
}

// MARK: TFLiteExecutorManager

TFLiteExecutorManager::TFLiteExecutorManager(size_t poolSize) :
                          ModelExecutorManager(poolSize) {
  NOTICE() << "TFLiteExecutorManager()" << FLUSH;
}

ModelExecutor* TFLiteExecutorManager::createExecutor(const std::string& modelName, const std::string& modelFile) {
  NOTICE() << "createExecutor()" << FLUSH;

  std::lock_guard<std::mutex> lock(tfLiteExecMtx_);

  return new TFLiteExecutor(modelName, modelFile);
}
