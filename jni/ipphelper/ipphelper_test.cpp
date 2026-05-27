#include <gtest/gtest.h>
#include <string.h>

extern "C" {
#include "cups.h"
#include "ipphelper.h"
}

class IppHelperTest : public ::testing::Test {
 protected:
  void SetUp() override { memset(&capabilities_, 0, sizeof(capabilities_)); }

  printer_capabilities_t capabilities_;
};

TEST_F(IppHelperTest, ParseGetMediaSupported_AvoidsOverflow) {
  // Initialize the structure as it would be in parse_printerAttributes
  media_supported_t media_supported_;
  for (int i = 0; i < PAGE_STATUS_MAX; i++) {
    media_supported_.media_size[i] = (media_size_t)0;
    media_supported_.idxKeywordTranTable[i] = -1;
  }
  int canary = 0xffffffff;  // Overwritten if media_supported_ overflows.

  // 1. Construct a mock IPP response
  ipp_t* response = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  ASSERT_NE(response, nullptr);

  // Create 1000 values (exceeding PAGE_STATUS_MAX = 200)
  // Use a valid keyword ("na_letter_8.5x11in") so ipp_find_media_size succeeds.
  const int num_values = 1000;
  const char* values[num_values];
  for (int i = 0; i < num_values; i++) {
    values[i] = "na_letter_8.5x11in";
  }

  // Add the "media-supported" attribute with 1000 values to the mock response
  ippAddStrings(response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-supported",
                num_values, nullptr, values);

  // 2. Call the function under test
  // BEFORE the fix: This would trigger a stack buffer overflow / crash.
  // AFTER the fix: This should return safely without crashing.
  parse_getMediaSupported(response, &media_supported_, &capabilities_);

  // 3. Count updated entries.
  int written_count = 0;
  for (int i = 0; i < PAGE_STATUS_MAX; i++) {
    if (media_supported_.media_size[i] != 0) {
      written_count++;
    }
  }

  // Expect exactly PAGE_STATUS_MAX (200) entries to be written and the canary
  // to be intact.
  EXPECT_EQ(written_count, PAGE_STATUS_MAX);
  EXPECT_EQ(capabilities_.numSupportedMediaReadySizes, 0);
  EXPECT_EQ(canary, 0xffffffff);

  ippDelete(response);
}
