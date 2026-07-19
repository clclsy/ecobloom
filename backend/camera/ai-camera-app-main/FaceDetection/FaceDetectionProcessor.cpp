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
#include <processors/Processor.h>

std::atomic<uint64_t> Processor::processingEventCounter_{0};

Processor::Processor() : nextProcessor_(nullptr),
                                          processingThread_(nullptr),
                                          stopProcessing_(false) {
  NOTICE() << "Processor()" << FLUSH;
}

Processor::~Processor() {
  NOTICE() << "~Processor()" << FLUSH;
}

void Processor::startup() {
  NOTICE() << "startup()" << FLUSH;

  std::lock_guard<std::mutex> lock(processingLock_);
  if (processingThread_.get() == nullptr) {
    processingThread_ = std::make_unique<std::thread>(&Processor::process, this);
  }
}

void Processor::shutdown() {
  NOTICE() << "shutdown()" << FLUSH;

  std::lock_guard<std::mutex> lock(processingLock_);
  stopProcessing_ = true;
  if (processingThread_.get() != nullptr) {
    if (processingThread_->joinable()) {
      processingThread_->join();
      processingThread_ = nullptr;
    }
  }
}

Processor& Processor::chainProcessor(std::unique_ptr<Processor> nextProcessor) {
  NOTICE() << "chainProcessor()" << FLUSH;

  std::lock_guard<std::mutex> lock(processingLock_);

  if (nextProcessor.get() == nullptr) {
    throw std::logic_error("No next processor passed in.  Aborting ...");
  }

  nextProcessor->startup();
  nextProcessor_ = std::move(nextProcessor);

  return *(nextProcessor_.get());
}

void Processor::passData(std::shared_ptr<ProcessingContext> processingContext) {
  NOTICE() << "passData()" << FLUSH;

  std::lock_guard<std::mutex> lock(contextLock_);
  processingData_ = std::move(processingContext);
}

uint64_t Processor::getNextProcessingEventSequenceNumber() {
  return ++Processor::processingEventCounter_;
}

uint64_t Processor::getNextProcessingEventTimestamp() {
  auto tp = std::chrono::system_clock::now().time_since_epoch();
  auto value = std::chrono::duration_cast<std::chrono::milliseconds>(tp);

  return value.count();
}

