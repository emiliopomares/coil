#include <coil/foundation/error.hpp>

#include <gtest/gtest.h>

#include <format>
#include <type_traits>
#include <utility>

namespace {

using coil::foundation::Error;
using coil::foundation::ErrorCode;
using coil::foundation::Expected;
using coil::foundation::makeUnexpected;
using coil::foundation::toString;

Expected<int> failWithInvalidArgument() {
    return makeUnexpected(ErrorCode::InvalidArgument, "size must be positive");
}

Expected<void> failWithUnavailable() {
    return makeUnexpected(ErrorCode::Unavailable);
}

TEST(Error, AC1_StoresCodeAndMessage) {
    const Error error{ErrorCode::NotFound, "window 3"};
    EXPECT_EQ(error.code(), ErrorCode::NotFound);
    EXPECT_EQ(error.message(), "window 3");
}

TEST(Error, AC2_MessageIsOptional) {
    const Error error{ErrorCode::Internal};
    EXPECT_EQ(error.code(), ErrorCode::Internal);
    EXPECT_EQ(error.message(), "");
}

TEST(Error, AC3_MakeUnexpectedFillsExpectedOfValue) {
    const Expected<int> result = failWithInvalidArgument();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(result.error().message(), "size must be positive");
}

TEST(Error, AC4_MakeUnexpectedFillsExpectedOfVoid) {
    const Expected<void> result = failWithUnavailable();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::Unavailable);
    EXPECT_EQ(result.error().message(), "");
}

TEST(Error, AC5_ToStringNamesEveryCode) {
    EXPECT_EQ(toString(ErrorCode::InvalidArgument), "InvalidArgument");
    EXPECT_EQ(toString(ErrorCode::NotFound), "NotFound");
    EXPECT_EQ(toString(ErrorCode::Unavailable), "Unavailable");
    EXPECT_EQ(toString(ErrorCode::Internal), "Internal");
    EXPECT_EQ(toString(static_cast<ErrorCode>(999)), "Unknown");
}

TEST(Error, AC6_FormatsCodeAndMessage) {
    EXPECT_EQ(std::format("{}", Error{ErrorCode::NotFound, "window 3"}), "NotFound: window 3");
    EXPECT_EQ(std::format("{}", Error{ErrorCode::NotFound, "ventana «Coïl»"}),
              "NotFound: ventana «Coïl»");
    EXPECT_EQ(std::format("{}", Error{static_cast<ErrorCode>(999), "x"}), "Unknown: x");
}

TEST(Error, AC7_FormatsWithoutMessageAndFormatsCode) {
    EXPECT_EQ(std::format("{}", Error{ErrorCode::Internal}), "Internal");
    EXPECT_EQ(std::format("{}", ErrorCode::Unavailable), "Unavailable");
}

TEST(Error, AC8_CodesHaveStableNumericValues) {
    EXPECT_EQ(std::to_underlying(ErrorCode::InvalidArgument), 1);
    EXPECT_EQ(std::to_underlying(ErrorCode::NotFound), 2);
    EXPECT_EQ(std::to_underlying(ErrorCode::Unavailable), 3);
    EXPECT_EQ(std::to_underlying(ErrorCode::Internal), 4);
}

// SPEC-0024 AC-8: the code for cancelled operations.
TEST(Error, AC8_CancelledCode) {
    EXPECT_EQ(toString(ErrorCode::Cancelled), "Cancelled");
    EXPECT_EQ(std::format("{}", Error{ErrorCode::Cancelled}), "Cancelled");
    EXPECT_EQ(std::to_underlying(ErrorCode::Cancelled), 5);
}

TEST(Error, CoilNamespaceAliasesFoundation) {
    EXPECT_TRUE((std::is_same_v<coil::Error, coil::foundation::Error>));
    EXPECT_TRUE((std::is_same_v<coil::ErrorCode, coil::foundation::ErrorCode>));
    EXPECT_TRUE((std::is_same_v<coil::Expected<int>, coil::foundation::Expected<int>>));
    EXPECT_EQ(&coil::makeUnexpected, &coil::foundation::makeUnexpected);
}

} // namespace
