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
#include <unordered_set>
#include <vector>

#define DEFAULT_MODEL_EXECUTOR_POOL_SIZE  10
#define SHAPE_MAP_INPUT "input"

namespace bbry { namespace poc {

/**
 * An interface for ML model execution.
 */
class ModelExecutor {
 public:
  virtual ~ModelExecutor() = default;

  /**
   * Construct a ML Model Executor.
   * 
   * @param[in] modelName     The name of the associated ML model
   * @param[in] modelFilePath The name of the associated ML model file
   *                          (pass empty string if not required)
   * @return std::unique_ptr<ModelExecutor> 
   */
  ModelExecutor(const std::string& modelName, const std::string& modelFilePath);
  ModelExecutor(const ModelExecutor&) = default;
  ModelExecutor& operator=(const ModelExecutor&) = default;
  ModelExecutor(ModelExecutor&&) = default;
  ModelExecutor& operator=(ModelExecutor&&) = default;


  /**
   * Set an input tensor (tensor name is obtained from the ML model metadata).
   *
   * @param[in] input The tensor data
   * @return true if successful
   * @return false if unsuccessful
   */
  virtual bool setInput(const std::vector<float>& input);

  /**
   * Set an input tensor with a specific tensor name.
   *
   * @param[in] inTensorName The name of the tensor
   * @param[in] input The tensor data
   * @return true if successful
   * @return false if unsuccessful
   */
  virtual bool setInput(const std::string& inTensorName,
                        const std::vector<float>& input) = 0;

  /**
   * Update tensor shape map.
   *
   * @param[in] mapEntryName The name of the tensor
   * @param[in] tensorShape The tensor shape (dimensions)
   * @return true if successful
   * @return false if unsuccessful
   */
  virtual bool updateShapeMap(const std::string& mapEntryName, std::vector<size_t>& tensorShape) = 0;

  /**
   * Load the ML model.
   *
   * @return true if successful
   * @return false if unsuccessful
   */
  virtual bool loadMLModel() = 0;

 /**
  * Reset the executor (if required), usually called upon return
  */
  virtual void reset() = 0;

  /**
   * Run inference on the model, using the data supplied by setInput. Results can be accessed with getOutput.
   *
   * @return true if successful
   * @return false if unsuccessful
   **/
  virtual bool infer() = 0;

  /**
   * Get the output tensor.
   *
   * @param[in] outTensorName The name of the output tensor to retrieve.
   * @return std::vector<float> The retrieved tensor data.
   */
  virtual std::vector<float> getOutput(const std::string& outTensorName) = 0;

  /**
   * Return the model name.
   * 
   * This will be an alias for a model that is loaded for easier lookup.
   * 
   * @return std::string The model name.
   */
  std::string getModelName();

  /**
   * Return the model file name.
   * 
   * Path to the ML model file, if required.
   * 
   * @return std::string The model name.
   */
  std::string getModelFilePath();

 protected:
    // The name of the ML model to execute
  std::string modelName_;

    // The path of the ML model to load
  std::string modelFilePath_;
};

/**
 * A interface for managing loans of a model executor. 
 * A user may request to "borrow" ModelExecutors from the Manager, and return it when done.
 */
class ModelExecutorManager {
 public:
  virtual ~ModelExecutorManager() = default;

  /**
   * Construct a new ModelExecutorManager object
   *
   * @param[in] poolSize The default size of the executor pool.
   */
  explicit ModelExecutorManager(size_t poolSize = DEFAULT_MODEL_EXECUTOR_POOL_SIZE);
  ModelExecutorManager(const ModelExecutorManager&) = default;
  ModelExecutorManager& operator=(const ModelExecutorManager&) = default;
  ModelExecutorManager(ModelExecutorManager&&) = default;
  ModelExecutorManager& operator=(ModelExecutorManager&&) = default;

  /**
   * Get exclusive access to an ML Executor.
   * 
   * If no executor is available, this function will block until one is available.
   * 
   * @param[in] modelName The name of the ML model associated with the ML executor to borrow
   * @param[in] modelFile The name of the ML model file associated with the ML executor to borrow
   *                      (pass empty string if not required)
   * @return ModelExecutor* pointer to model executor 
   */
  virtual ModelExecutor* borrowExecutor(const std::string& modelName, const std::string& modelFile);

  /**
   * Return the ML Executor acquired through borrowExecutor.
   * 
   * Returning an executor multiple times will have no effect.
   * 
   * @param[in] executor The ML executor to return
   */
  virtual void returnExecutor(ModelExecutor* executor);

 protected:
  struct TrackedModelExecutor {
    bool isLoaned = false;
    std:: unique_ptr<ModelExecutor> modelExecutor = nullptr;
  };

  // create executor (to be delegated to derived class)
  virtual ModelExecutor* createExecutor(const std::string& modelName, const std::string& modelFile) = 0;

  // build model executor pool
  virtual void buildModelExecutorPool();

  // check if any model executors are available
  virtual bool anyModelExecutorsAvailable();

  // check if any model executors are available for the model name / file supplied
  virtual bool anyModelExecutorsAvailable(const std::string& modelName, const std::string& modelFile);

  // access controls
  std::mutex mutex_;
  std::condition_variable cv_;

  // pool related fields
  const size_t poolSize_ = 0;
  std::vector<TrackedModelExecutor> pool_;
};

}}  // namespace bbry::poc
