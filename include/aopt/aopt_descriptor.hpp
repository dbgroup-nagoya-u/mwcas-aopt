/*
 * Copyright 2021 Database Group, Nagoya University
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

#ifndef MWCAS_AOPT_AOPT_COMPONENT_AOPT_DESCRIPTOR_H_
#define MWCAS_AOPT_AOPT_COMPONENT_AOPT_DESCRIPTOR_H_

#include <array>
#include <atomic>
#include <memory>
#include <utility>

#include "component/word_descriptor.hpp"
#include "memory/epoch_based_gc.hpp"

namespace dbgroup::atomic::aopt
{
/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation by using
 * AOPT algorithm.
 *
 */
class alignas(component::kCacheLineSize) AOPTDescriptor
{
  using EpochBasedGC_t = ::dbgroup::memory::EpochBasedGC<AOPTDescriptor>;
  using Status = component::Status;
  using WordDescriptor = component::WordDescriptor;
  using MwCASField = component::MwCASField;

 public:
  /*################################################################################################
   * Public constructors and assignment operators
   *##############################################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr AOPTDescriptor() : status_{Status::ACTIVE}, target_count_{0} {}

  constexpr AOPTDescriptor(const AOPTDescriptor &) = default;
  constexpr AOPTDescriptor &operator=(const AOPTDescriptor &obj) = default;
  constexpr AOPTDescriptor(AOPTDescriptor &&) = default;
  constexpr AOPTDescriptor &operator=(AOPTDescriptor &&) = default;

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  /**
   * @brief Destroy the AOPTDescriptor object.
   *
   */
  ~AOPTDescriptor() = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  /**
   * @return the number of registered MwCAS targets.
   */
  constexpr size_t
  Size() const
  {
    return target_count_;
  }

  /**
   * @return the current status of this descriptor.
   */
  Status
  GetStatus() const
  {
    return status_.load(component::mo_relax);
  }

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  /**
   * @brief Start garbage collection for AOPT descriptors.
   *
   * Note that this function must be called before performing AOPT-based MwCAS.
   *
   * @param gc_interval interval for GC in microseconds.
   * @param gc_thread_num the number of worker threads to release garbages.
   */
  static void
  StartGC(  //
      const size_t gc_interval = 100000,
      const size_t gc_thread_num = 1)
  {
    gc_ = std::make_unique<EpochBasedGC_t>(gc_interval, gc_thread_num, true);
  }

  /**
   * @brief Stop garbage collection for AOPT descriptors.
   *
   */
  static void
  StopGC()
  {
    gc_.reset(nullptr);
  }

  /**
   * @brief Read a value from a given memory address.
   * \e NOTE: if a memory address is included in MwCAS target fields, it must be read via
   * this function.
   *
   * @tparam T an expected class of a target field
   * @param addr a target memory address to read
   * @return a read value
   */
  template <class T>
  static T
  Read(void *addr)
  {
    const auto guard = gc_->CreateEpochGuard();
    return ReadInternal(addr, nullptr).second.GetTargetData<T>();
  }

  /**
   * @brief Add a new MwCAS target to this descriptor.
   *
   * @tparam T a class of a target
   * @param addr a target memory address
   * @param old_val an expected value of a target field
   * @param new_val an inserting value into a target field
   * @retval true if target registration succeeds
   * @retval false if this descriptor is already full
   */
  template <class T>
  constexpr bool
  AddMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val)
  {
    if (target_count_ == kMwCASCapacity) {
      return false;
    } else {
      words_[target_count_++] = WordDescriptor{addr, old_val, new_val, this};
      return true;
    }
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds
   * @retval false if a MwCAS operation fails
   */
  bool
  MwCAS()
  {
    thread_local FinishedDescriptors finished_descriptors{};

    const auto guard = gc_->CreateEpochGuard();

    // serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    for (size_t i = 0; i < target_count_; ++i) {
      auto word_desc = &words_[i];
    retry_word:
      auto [content, value] = ReadInternal(word_desc->GetAddress(), this);

      if (content.GetTargetData<WordDescriptor *>() == word_desc) {
        // this word already points to the right place, move on
        continue;
      }

      if (value != word_desc->GetOldValue()) {
        // the expected value is different, the MwCAS fails
        mwcas_success = false;
        break;
      }

      if (GetStatus() != Status::ACTIVE) {
        // this AOPT descriptor has already finished
        break;
      }

      // try to install the pointer to my descriptor
      if (!word_desc->EmbedDescriptor(content)) {
        // if failed, retry
        goto retry_word;
      }
    }

    // update status of this descriptor
    auto expected = Status::ACTIVE;
    const auto desired = (mwcas_success) ? Status::SUCCESSFUL : Status::FAILED;
    status_.compare_exchange_strong(expected, desired, component::mo_relax);

    if (expected == Status::ACTIVE) {
      // if this thread finalized the descriptor, mark it for reclamation
      finished_descriptors.RetireForCleanUp(this);
    }

    return GetStatus() == Status::SUCCESSFUL;
  }

 private:
  /*################################################################################################
   * Internal classes
   *##############################################################################################*/

  /**
   * @brief A class to manage finished AOPT descriptors.
   *
   */
  class FinishedDescriptors
  {
   public:
    /*##############################################################################################
     * Public constructors and assignment operators
     *############################################################################################*/

    /**
     * @brief Create a new FinishedDescriptors object.
     *
     */
    constexpr FinishedDescriptors() : desc_arr_{}, desc_num_{0} {}

    /**
     * @brief Destroy the FinishedDescriptors object.
     *
     */
    ~FinishedDescriptors() { FinalizeFinishedDescriptors(); }

    /*##############################################################################################
     * Public utility functions
     *############################################################################################*/

    /**
     * @brief Register a finished descriptor with the internal list.
     *
     * If the number of finished descriptors reaches a certain threshold, this function
     * invoke finalization of finished descriptors to release them.
     *
     * @param desc a finished descriptor.
     */
    void
    RetireForCleanUp(AOPTDescriptor *desc)
    {
      if (desc_num_ >= kMaxFinishedDescriptors) {
        FinalizeFinishedDescriptors();
      }
      desc_arr_[desc_num_++] = desc;
    }

   private:
    /*##############################################################################################
     * Internal utility functions
     *############################################################################################*/

    /**
     * @brief Perform finalization for AOPT-based MwCAS.
     *
     * After this function, finished descriptors become targets of internal GC.
     */
    void
    FinalizeFinishedDescriptors()
    {
      for (auto &&desc : desc_arr_) {
        const auto status = desc->GetStatus();
        for (auto &&word : desc->words_) {
          (&word)->CompleteMwCAS(status);
        }
        gc_->AddGarbage(desc);
      }
      desc_num_ = 0;
    }

    /*##############################################################################################
     * Internal member variables
     *############################################################################################*/

    /// pointers to finished descriptors
    std::array<AOPTDescriptor *, kMaxFinishedDescriptors> desc_arr_;

    /// the current number of finished descriptors
    size_t desc_num_;
  };

  /*################################################################################################
   * Internal utility functions
   *##############################################################################################*/

  /**
   * @brief Read a value from a given memory address.
   * \e NOTE: if a memory address is included in MwCAS target fields, it must be read via
   * this function.
   *
   * @tparam T an expected class of a target field
   * @param addr a target memory address to read
   * @return a read value
   */
  static std::pair<MwCASField, MwCASField>
  ReadInternal(  //
      void *addr,
      AOPTDescriptor *self)
  {
    auto target_addr = static_cast<std::atomic<MwCASField> *>(addr);

    MwCASField target_word, act_val;
    while (true) {
      target_word = target_addr->load(component::mo_relax);
      if (!target_word.IsWordDescriptor()) {
        act_val = target_word;
        break;
      }

      // found a word descriptor
      auto word = target_word.GetTargetData<WordDescriptor *>();
      auto parent = static_cast<AOPTDescriptor *>(word->GetParent());
      const auto parent_status = parent->GetStatus();
      if (parent != self && parent_status == Status::ACTIVE) {
        parent->MwCAS();
        continue;
      }
      act_val = word->GetCurrentValue(parent_status);
      break;
    }

    return {target_word, act_val};
  }

  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// a garbage collector for expired descriptors
  inline static std::unique_ptr<EpochBasedGC_t> gc_{nullptr};

  /// a status of this AOPT descriptor
  std::atomic<Status> status_;

  /// The number of registered MwCAS targets
  size_t target_count_;

  /// Target entries of MwCAS
  WordDescriptor words_[kMwCASCapacity];
};

}  // namespace dbgroup::atomic::aopt

#endif  // MWCAS_AOPT_AOPT_COMPONENT_AOPT_DESCRIPTOR_H_
