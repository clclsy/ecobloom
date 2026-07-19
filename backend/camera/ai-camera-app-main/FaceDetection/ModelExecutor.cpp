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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

#include <Logging.h>
#include <ml/ModelExecutor.h>

using bbry::poc::ModelExecutor;
using bbry::poc::ModelExecutorManager;

ModelExecutor::ModelExecutor(const std::string& modelName, const std::string& modelFilePath) :
                             modelName_(modelName), modelFilePath_(modelFilePath) {
  NOTICE() << "ModelExecutor()" << FLUSH;
}

std::string ModelExecutor::getModelName() {
  return modelName_;
}

std::string ModelExecutor::getModelFilePath() {
  return modelName_;
}

bool ModelExecutor::setInput(const std::vector<float>& input) {
  NOTICE() << "setInput() with size: " << input.size() << FLUSH;

  return setInput("", input);
}

ModelExecutorManager::ModelExecutorManager(size_t poolSize) :
    poolSize_(poolSize) {
  NOTICE() << "ModelExecutorManager()" << FLUSH;
  buildModelExecutorPool();
}

ModelExecutor*  ModelExecutorManager::borrowExecutor(
    const std::string& modelName, const std::string& modelFile) {
  NOTICE() << "borrowExecutor()" << FLUSH;

  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this]() {return this->anyModelExecutorsAvailable();});
  for (auto& exec : pool_) {
    if (!exec.isLoaned) {
      if (exec.modelExecutor.get() == nullptr) {
        exec.modelExecutor.reset(createExecutor(modelName, modelFile));
      } else {
        if (!(exec.modelExecutor->getModelName() == modelName &&
              exec.modelExecutor->getModelFilePath() == modelFile)) {
          continue;
        }
      }

      INFO() << "Borrowing executor with address 0x" << std::hex << exec.modelExecutor.get() << std::dec
             << FLUSH;
      exec.isLoaned = true;
      return exec.modelExecutor.get();
    }
  }

  WARNING() << "Unable to borrow executor! Expected at least one executor to be available."
             << FLUSH;

  return nullptr;
}

void ModelExecutorManager::returnExecutor(ModelExecutor* executor) {
  NOTICE() << "returnExecutor()" << FLUSH;

  std::unique_lock<std::mutex> lock(mutex_);
  for (auto& exec : pool_) {
    if (exec.isLoaned) {
      if (exec.modelExecutor.get() == executor) {
        executor->reset();

        INFO() << "Returned executor with address 0x" << std::hex << exec.modelExecutor.get() << std::dec
               << " to pool" << FLUSH;
        exec.isLoaned = false;
        lock.unlock();
        cv_.notify_all();
        return;
      }
    }
  }
  WARNING() << "Failed to return executor with address 0x" << std::hex << executor << std::dec
            << "; No matching executor found in pool" << FLUSH;
}

bool ModelExecutorManager::anyModelExecutorsAvailable() {
  NOTICE() << "anyModelExecutorsAvailable()" << FLUSH;

  for (const auto& exec : pool_) {
    if (!exec.isLoaned) {
      return true;
    }
  }
  return false;
}

bool ModelExecutorManager::anyModelExecutorsAvailable(const std::string& modelName,
                                                      const std::string& modelFile) {
  NOTICE() << "anySpecificModelExecutorsAvailable()" << FLUSH;

  // search for a specific executor for the model first
  // but also track any free executor as a fallback to return
  // if no specific executor is available
  bool anyExecutorAvailable = false;
  for (const auto& exec : pool_) {
    if (!exec.isLoaned) {
      if (exec.modelExecutor.get() == nullptr) {
        anyExecutorAvailable = true;
      } else {
        if (exec.modelExecutor->getModelName() == modelName &&
            exec.modelExecutor->getModelFilePath() == modelFile) {
          return true;
        }
      }
    }
  }

  return anyExecutorAvailable;
}

void ModelExecutorManager::buildModelExecutorPool() {
  NOTICE() << "buildModelExecutorPool()" << FLUSH;

  pool_.clear();
  pool_.resize(poolSize_);
}
