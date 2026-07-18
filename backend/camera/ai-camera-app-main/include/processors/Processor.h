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
#include <mutex>
#include <thread>

#include <processors/ProcessingContext.h>


/**
 * A Processor is the building block of the data pipeline.
 *
 * Each processor provides a single passData method which takes a ProcessingContext as an argument.
 */
class Processor {
 public:
  virtual ~Processor();

  Processor();
  Processor(const Processor &) = default;
  Processor &operator=(const Processor &) = default;
  Processor(Processor &&) noexcept = default;
  Processor &operator=(Processor &&) noexcept = default;

  /**
   * Start the processing thread.
   */
  virtual void startup();

  /**
   * Stop the processing thread.
   */
  virtual void shutdown();

  /**
   * Chain the next processor to receive output from this processor.
   *
   * @param[in] nextProcessor unique pointer to next processor to act on data from this processor.
   * return Processor& reference to chained processor.
   */
  virtual Processor& chainProcessor(std::unique_ptr<Processor> nextProcessor);

  /**
   * Pass data to this processor.
   *
   * @param[in] processingContext Contains data about the current state of processing.
   */
  virtual void passData(std::shared_ptr<ProcessingContext> processingContext);

 protected:
  // Process a unit of work.
  virtual void process() = 0;

  // a mutex to protect access to state
  std::mutex processingLock_;

  // a mutex to protect access to the processing context data
  std::mutex contextLock_;

  // pointer to next Processor (if any)
  std::unique_ptr<Processor> nextProcessor_;

  // data to process
  std::shared_ptr<ProcessingContext> processingData_;

  // A thread for running the ML method
  std::unique_ptr<std::thread> processingThread_{nullptr};

  // stop the processing thread
  std::atomic<bool> stopProcessing_{false};

  // processing event counter
  static std::atomic<uint64_t> processingEventCounter_;

 protected:
  // shared static methods
  // Obtain next processing event sequence number.
  static uint64_t getNextProcessingEventSequenceNumber();

  // Obtain next processing event sequence number.
  static uint64_t getNextProcessingEventTimestamp();
};
