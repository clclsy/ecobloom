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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>

#include <ml/ModelExecutor.h>

namespace bbry {
namespace poc {

/**
 * A weak (non-owning) wrapper around a tflite::interpreter that conforms to the ModelExecutor Interface.
 * Expected to be created by an "owning" ModelExecutorManager. A TFLiteExecutor is granted exclusive access to an
 * instance of tflite::interpreter until it either "returns" the interpreter to the owner (via owner->returnExecutor)
 * or until it is destructed (where returnExecutor is called implicitly).
 */
class TFLiteExecutor : public ModelExecutor {
 public:
  /**
   * Construct a new TFLiteExecutor object
   *
   * @param[in] modelName The name of the TFLite ML model to load and execute.
   */
  TFLiteExecutor(const std::string& modelName, const std::string& modelFile);
  ~TFLiteExecutor() = default;

  TFLiteExecutor(const TFLiteExecutor&) = delete;
  TFLiteExecutor& operator=(const TFLiteExecutor&) = delete;
  TFLiteExecutor(TFLiteExecutor&&) = delete;
  TFLiteExecutor& operator=(TFLiteExecutor&&) = delete;

  /**
   * Set the data for the input tensor with name \p inputTensorName.
   * 
   * Data is set on a thread local tflite::Interpreter object. This can therefore be called from different threads with
   * no effect on the other threads.
   * 
   * @param inTensorName The name of the tensor to set
   * @param input The data to apply to \p inputTensorName
   * @return true if success
   * @return false if failure
   */
  bool setInput(const std::string& inTensorName,
                const std::vector<float>& input) override;

  /**
   * Update tensor shape map.
   *
   * @param[in] mapEntryName The name of the tensor
   * @param[in] tensorShape The tensor shape (dimensions)
   * @return true if successful
   * @return false if unsuccessful
   */
  bool updateShapeMap(const std::string& mapEntryName, std::vector<size_t>& tensorShape) override;

  /**
   * Load the ML model.
   *
   * @return true if successful
   * @return false if unsuccessful
   */
  bool loadMLModel() override;

  /**
   * Reset the executor (if required), usually called upon return
   */
  void reset() override;

  /**
   * Run inference on the model using the data stored in inTensors_. Store the result in outTensors_
   *
   * @return true if successful
   * @return false if unsuccessful
   **/
  bool infer() override;

  /**
   * Get the data for the output tensor with name \p outputTensorName.
   * 
   * Data is set on a thread local tflite::Interpreter object. This can therefore be called from different threads with
   * no effect on the other threads.
   * 
   * @param outTensorName 
   * @return std::vector<float> the requested tensor
   */
  std::vector<float> getOutput(const std::string& outTensorName) override;

 private:
  std::mutex tfLiteMtx_;

  struct TensorMetadata {
    int index = 0;
    int size = 0;
    int totalSize = 0;
  };

  // A map of tensor name to tensor data for model inputs
  std::unordered_map<std::string, void*> inTensors_;
    // A map of tensor name to tensor data for model outputs
  std::unordered_map<std::string, TensorMetadata> inMetadata_;

  // A map of tensor name to tensor data for model inputs
  std::unordered_map<std::string, std::vector<int>> shapeMap_;
  // A map of tensor name to tensor data for model outputs
  std::unordered_map<std::string, void*> outTensors_;
    // A map of tensor name to tensor data for model outputs
  std::unordered_map<std::string, TensorMetadata> outMetadata_;
    // A map of tensor name to tensor data for model outputs
  std::vector<std::string> outTensorNames_;

  // TFLite interpreter
  std::unique_ptr<tflite::Interpreter> interpreter_{nullptr};

  // loaded ML model
  std::unique_ptr<tflite::FlatBufferModel> model_{nullptr};

  // TF resolver
  tflite::ops::builtin::BuiltinOpResolver resolver_;
};

/**
 * A class to facilitate "borrowing" of TFLiteExecutors. 
 */
class TFLiteExecutorManager : public ModelExecutorManager {
 public:
  explicit TFLiteExecutorManager(size_t poolSize = DEFAULT_MODEL_EXECUTOR_POOL_SIZE);

 protected:
  // create executor (to be delegated to derived class)
  ModelExecutor* createExecutor(const std::string& modelName, const std::string& modelFile) override;

 private:
  std::mutex tfLiteExecMtx_;
};

}}  // namespace bbry::poc
