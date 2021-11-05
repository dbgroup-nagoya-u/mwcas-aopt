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

#ifndef MWCAS_AOPT_AOPT_COMPONENT_WORD_DESCRIPTOR_H_
#define MWCAS_AOPT_AOPT_COMPONENT_WORD_DESCRIPTOR_H_

#include <atomic>

#include "mwcas_field.hpp"

namespace dbgroup::atomic::aopt::component
{
/**
 * @brief A class to represent a word descriptor.
 *
 */
class WordDescriptor
{
 public:
  /*################################################################################################
   * Public constructors and assignment operators
   *##############################################################################################*/

  /**
   * @brief Construct an empty word descriptor.
   *
   */
  constexpr WordDescriptor() : addr_{}, old_val_{}, new_val_{}, parent_{} {}

  /**
   * @brief Construct a new word descriptor based on given information.
   *
   * @tparam T a class of MwCAS targets.
   * @param addr a target memory address.
   * @param old_val an expected value of the target address.
   * @param new_val an desired value of the target address.
   * @param parent_aopt an AOPT descriptor that has this object.
   */
  template <class T>
  constexpr WordDescriptor(  //
      void *addr,
      const T old_val,
      const T new_val,
      void *parent_aopt)
      : addr_{static_cast<std::atomic<MwCASField> *>(addr)},
        old_val_{old_val},
        new_val_{new_val},
        parent_{parent_aopt}
  {
  }

  constexpr WordDescriptor(const WordDescriptor &) = default;
  constexpr WordDescriptor &operator=(const WordDescriptor &obj) = default;
  constexpr WordDescriptor(WordDescriptor &&) = default;
  constexpr WordDescriptor &operator=(WordDescriptor &&) = default;

  /*################################################################################################
   * Public destructor
   *##############################################################################################*/

  /**
   * @brief Destroy the WordDescriptor object.
   *
   */
  ~WordDescriptor() = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  /**
   * @return void*: the target address of this descriptor.
   */
  void *
  GetAddress() const
  {
    return addr_;
  }

  /**
   * @return MwCASField: the expected value of this descriptor.
   */
  MwCASField
  GetOldValue() const
  {
    return old_val_;
  }

  /**
   * @brief Get the current value based on given status.
   *
   * @param status the current status of the parent AOPT descriptor.
   * @return MwCASField: the current value in the target address.
   */
  MwCASField
  GetCurrentValue(const Status status) const
  {
    return (status == SUCCESSFUL) ? new_val_ : old_val_;
  }

  /**
   * @return AOPTDescriptor*: the address of the parent AOPT descriptor.
   */
  void *
  GetParent() const
  {
    return parent_;
  }

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  /**
   * @brief Embed a descriptor into this target address to linearlize MwCAS operations.
   *
   * @param content a current word in the target address.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */
  bool
  EmbedDescriptor(const MwCASField content)
  {
    const MwCASField desc{this, true};

    MwCASField expected = content;
    addr_->compare_exchange_strong(expected, desc, mo_relax);

    return expected == content;
  }

  /**
   * @brief Update/revert a value of this target address.
   *
   * @param status the current status of the parent AOPT descriptor.
   */
  void
  CompleteMwCAS(const Status status)
  {
    const MwCASField desc{this, true};
    const MwCASField desired = (status == SUCCESSFUL) ? new_val_ : old_val_;

    MwCASField expected = desc;
    addr_->compare_exchange_strong(expected, desired, mo_relax);
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// A target memory address
  std::atomic<MwCASField> *addr_;

  /// An expected value of a target field
  MwCASField old_val_;

  /// An inserting value into a target field
  MwCASField new_val_;

  /// An address of the corresponding AOPT descriptor
  void *parent_;
};

}  // namespace dbgroup::atomic::aopt::component

#endif  // MWCAS_AOPT_AOPT_COMPONENT_WORD_DESCRIPTOR_H_
