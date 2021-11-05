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
    dummy_aopt_desc = nullptr;
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      old_val = new uint64_t{1};
      new_val = new uint64_t{2};
    } else {
      old_val = 1;
      new_val = 2;
    }
    target = old_val;

    word_desc = WordDescriptor{&target, old_val, new_val, dummy_aopt_desc};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete old_val;
      delete new_val;
    }
  }

  /*################################################################################################
   * Functions for verification
   *##############################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    MwCASField expected{(expect_fail) ? new_val : old_val, false};
    MwCASField desc_field{&word_desc, true};

    const bool success = word_desc.EmbedDescriptor(expected);

    if (expect_fail) {
      EXPECT_FALSE(success);
      EXPECT_NE(CASTargetConverter{desc_field}.converted_data,
                CASTargetConverter{target}.converted_data);
    } else {
      EXPECT_TRUE(success);
      EXPECT_EQ(CASTargetConverter{desc_field}.converted_data,
                CASTargetConverter{target}.converted_data);
    }
  }

  void
  VerifyCompleteMwCAS(const bool mwcas_success)
  {
    MwCASField expected{old_val, false};
    ASSERT_TRUE(word_desc.EmbedDescriptor(expected));

    word_desc.CompleteMwCAS((mwcas_success) ? SUCCESSFUL : FAILED);

    if (mwcas_success) {
      EXPECT_EQ(new_val, target);
    } else {
      EXPECT_EQ(old_val, target);
    }
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  void *dummy_aopt_desc;
  WordDescriptor word_desc;

  Target target;
  Target old_val;
  Target new_val;
};

/*##################################################################################################
 * Preparation for typed testing
 *################################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t *, MyClass>;
TYPED_TEST_CASE(WordDescriptorFixture, Targets);

/*##################################################################################################
 * Unit test definitions
 *################################################################################################*/

TYPED_TEST(WordDescriptorFixture, EmbedDescriptor_TargetHasExpectedValue_EmbeddingSucceed)
{
  TestFixture::VerifyEmbedDescriptor(false);
}

TYPED_TEST(WordDescriptorFixture, EmbedDescriptor_TargetHasUnexpectedValue_EmbeddingFail)
{
  TestFixture::VerifyEmbedDescriptor(true);
}

TYPED_TEST(WordDescriptorFixture, CompleteMwCAS_MwCASSucceeded_UpdateToDesiredValue)
{
  TestFixture::VerifyCompleteMwCAS(true);
}

TYPED_TEST(WordDescriptorFixture, CompleteMwCAS_MwCASFailed_RevertToExpectedValue)
{
  TestFixture::VerifyCompleteMwCAS(false);
}

}  // namespace dbgroup::atomic::aopt::component::test
