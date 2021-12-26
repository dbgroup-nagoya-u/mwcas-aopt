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

#include "aopt/component/word_descriptor.hpp"

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::aopt::component::test
{
template <class Target>

class WordDescriptorFixture : public ::testing::Test
{
 protected:
  /*################################################################################################
   * Setup/Teardown
   *##############################################################################################*/

  void
  SetUp() override
  {
    dummy_aopt_desc_ = nullptr;
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      old_val_ = new uint64_t{1};
      new_val_ = new uint64_t{2};
    } else {
      old_val_ = 1;
      new_val_ = 2;
    }
    target_ = old_val_;

    word_desc_ = WordDescriptor{&target_, old_val_, new_val_, dummy_aopt_desc_};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete old_val_;
      delete new_val_;
    }
  }

  /*################################################################################################
   * Functions for verification
   *##############################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    MwCASField expected{(expect_fail) ? new_val_ : old_val_, false};
    MwCASField desc_field{&word_desc_, true};

    const bool success = word_desc_.EmbedDescriptor(expected);

    if (expect_fail) {
      EXPECT_FALSE(success);
      EXPECT_NE(CASTargetConverter<MwCASField>{desc_field}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target_}.converted_data);        // NOLINT
    } else {
      EXPECT_TRUE(success);
      EXPECT_EQ(CASTargetConverter<MwCASField>{desc_field}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target_}.converted_data);        // NOLINT
    }
  }

  void
  VerifyCompleteMwCAS(const bool mwcas_success)
  {
    MwCASField expected{old_val_, false};
    ASSERT_TRUE(word_desc_.EmbedDescriptor(expected));

    word_desc_.CompleteMwCAS((mwcas_success) ? SUCCESSFUL : FAILED);

    if (mwcas_success) {
      EXPECT_EQ(new_val_, target_);
    } else {
      EXPECT_EQ(old_val_, target_);
    }
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  void *dummy_aopt_desc_{};
  WordDescriptor word_desc_{};

  Target target_{};
  Target old_val_{};
  Target new_val_{};
};

/*##################################################################################################
 * Preparation for typed testing
 *################################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t *, MyClass>;
TYPED_TEST_SUITE(WordDescriptorFixture, Targets);

/*##################################################################################################
 * Unit test definitions
 *################################################################################################*/

TYPED_TEST(WordDescriptorFixture, EmbedDescriptorWithExpectedValueSucceed)
{
  TestFixture::VerifyEmbedDescriptor(false);
}

TYPED_TEST(WordDescriptorFixture, EmbedDescriptorWithUnexpectedValueFail)
{
  TestFixture::VerifyEmbedDescriptor(true);
}

TYPED_TEST(WordDescriptorFixture, CompleteMwCASWithSucceededFlagUpdateToDesiredValue)
{
  TestFixture::VerifyCompleteMwCAS(true);
}

TYPED_TEST(WordDescriptorFixture, CompleteMwCASWithFailedFlagRevertToExpectedValue)
{
  TestFixture::VerifyCompleteMwCAS(false);
}

}  // namespace dbgroup::atomic::aopt::component::test
