// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-Licence-Identifier: MIT

#include <gtest/gtest.h>

#include "azure/core/amqp/models/amqp_value.hpp"

using namespace Azure::Core::Amqp::Models;

class TestValues : public testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestValues, SimpleCreate)
{
  {
    AmqpValue value;
    EXPECT_EQ(AmqpValueType::Null, value.GetType());
  }
  {
    AmqpValue value{true};
    EXPECT_EQ(AmqpValueType::Bool, value.GetType());
    EXPECT_TRUE(value);
  }
  {
    AmqpValue value{false};
    EXPECT_EQ(AmqpValueType::Bool, value.GetType());
    EXPECT_FALSE(value);
  }
  {
    AmqpValue value{};
    EXPECT_TRUE(value.IsNull());
  }

  {
    AmqpValue value{static_cast<uint8_t>(255)};
    EXPECT_EQ(AmqpValueType::UByte, value.GetType());
    EXPECT_EQ(255, static_cast<uint8_t>(value));
  }

  {
    AmqpValue value{'a'};
    EXPECT_EQ(AmqpValueType::Byte, value.GetType());
    EXPECT_EQ('a', static_cast<char>(value));
  }

  {
    AmqpValue value{static_cast<uint16_t>(65535)};
    EXPECT_EQ(AmqpValueType::UShort, value.GetType());
    EXPECT_EQ(65535, static_cast<uint16_t>(value));
  }
  {
    AmqpValue value{static_cast<int16_t>(32767)};
    EXPECT_EQ(AmqpValueType::Short, value.GetType());
    EXPECT_EQ(32767, static_cast<int16_t>(value));
  }

  {
    AmqpValue value(32);
    EXPECT_EQ(AmqpValueType::Int, value.GetType());
    EXPECT_EQ(32, static_cast<int32_t>(value));
  }

  {
    AmqpValue value(static_cast<int64_t>(32ll));
    EXPECT_EQ(AmqpValueType::Long, value.GetType());
    EXPECT_EQ(32ll, static_cast<int64_t>(value));
  }
  {
    AmqpValue value(static_cast<uint64_t>(39ull));
    EXPECT_EQ(AmqpValueType::ULong, value.GetType());
    EXPECT_EQ(39ull, static_cast<uint64_t>(value));
  }

  {
    AmqpValue value(39.0f);
    EXPECT_EQ(AmqpValueType::Float, value.GetType());
    EXPECT_EQ(39.0f, static_cast<float>(value));
  }
  {
    AmqpValue value(39.0);
    EXPECT_EQ(AmqpValueType::Double, value.GetType());
    EXPECT_EQ(39.0, static_cast<double>(value));
  }

  {
    AmqpValue value(39.0);
    double d{value};
    EXPECT_EQ(39.0, d);
  }

  {
    AmqpValue value(std::string("Fred"));
    std::string fredP(value);
    EXPECT_EQ(AmqpValueType::String, value.GetType());
    EXPECT_EQ(std::string("Fred"), fredP);
  }
  {
    AmqpValue value("Fred");
    std::string fredP(value);
    EXPECT_EQ(AmqpValueType::String, value.GetType());
    EXPECT_EQ(std::string("Fred"), fredP);
  }
}

TEST_F(TestValues, TestList)
{
  {
    AmqpValue list1{AmqpValue::CreateList()};
    EXPECT_EQ(0, list1.GetListItemCount());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetListItemCount());
  }
  // Put some things in the list.
  {
    AmqpValue list1{AmqpValue::CreateList()};
    list1.SetListItemCount(4);
    EXPECT_EQ(4, list1.GetListItemCount());

    list1.SetListItem(0, 123);
    list1.SetListItem(1, 23.97f);
    list1.SetListItem(2, "ABCD");
    list1.SetListItem(3, 'a');

    EXPECT_EQ(123, static_cast<int32_t>(list1.GetListItem(0)));
    EXPECT_EQ(23.97f, static_cast<float>(list1.GetListItem(1)));
  }
}

TEST_F(TestValues, TestMap)
{
  {
    AmqpValue map1{AmqpValue::CreateMap()};
    EXPECT_EQ(0, map1.GetMapValueCount());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetMapValueCount());
  }

  // Put some things in the map.
  {
    AmqpValue map1{AmqpValue::CreateMap()};
    map1.SetMapValue("ABC", 5);
    map1.SetMapValue(3, "ABC");
    EXPECT_EQ(2, map1.GetMapValueCount());

    EXPECT_EQ(5, static_cast<int32_t>(map1.GetMapValue("ABC")));
    EXPECT_EQ(std::string("ABC"), static_cast<std::string>(map1.GetMapValue(3)));

    auto val{map1.GetMapKeyAndValue(1)};
    EXPECT_EQ(AmqpValueType::Int, val.first.GetType());
    EXPECT_EQ(AmqpValueType::String, val.second.GetType());
    EXPECT_EQ(3, static_cast<int32_t>(val.first));
    EXPECT_EQ(std::string("ABC"), static_cast<std::string>(val.second));
  }
}

TEST_F(TestValues, TestArray)
{
  {
    AmqpValue value{AmqpValue::CreateArray()};
    EXPECT_EQ(0, value.GetArrayItemCount());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetArrayItemCount());
  }

  // Put some things in the map.
  {
    AmqpValue val{AmqpValue::CreateArray()};
    val.AddArrayItem("3"); // Array values must all have the same type.
    val.AddArrayItem("Foo");
    val.AddArrayItem("George");
    EXPECT_EQ(3, val.GetArrayItemCount());

    EXPECT_EQ(std::string("3"), static_cast<std::string>(val.GetArrayItem(0)));
  }
}

TEST_F(TestValues, TestChar)
{
  {
    AmqpValue value{AmqpValue::CreateChar(37)};
    EXPECT_EQ(37, value.GetChar());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetChar());
  }
}

TEST_F(TestValues, TestTimestamp)
{
  {
    std::chrono::milliseconds timeNow{std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())};
    AmqpValue value{AmqpValue::CreateTimestamp(timeNow)};
    EXPECT_EQ(timeNow, value.GetTimestamp());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetTimestamp());
  }
}

TEST_F(TestValues, TestSymbol)
{
  {
    AmqpValue value{AmqpValue::CreateSymbol("timeNow")};
    EXPECT_EQ("timeNow", value.GetSymbol());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetSymbol());
  }
}

TEST_F(TestValues, TestCompositeValue)
{
  {
    AmqpValue value{AmqpValue::CreateComposite("My Composite Type", 5)};

    EXPECT_EQ(5, value.GetCompositeItemCount());
  }
  {
    AmqpValue boolValue{false};
    EXPECT_ANY_THROW(boolValue.GetCompositeItemCount());
  }

  // Put some things in the map.
  {
    AmqpValue val{AmqpValue::CreateComposite("CompType", 2)};
    val.SetCompositeItem(0, 25);
    val.SetCompositeItem(1, 25.0f);

    EXPECT_EQ(25, static_cast<int32_t>(val.GetCompositeItem(0)));
    EXPECT_EQ(25.0f, static_cast<float>(val.GetCompositeItem(1)));
  }
  {
    AmqpValue val{AmqpValue::CreateCompositeWithDescriptor(29)};
  }
}

TEST_F(TestValues, TestDescribed)
{
  {
    AmqpValue value{AmqpValue::CreateDescribed("My Composite Type", 5)};
  }
}

TEST_F(TestValues, ValuesFromHeader)
{
  Header header;
  header.IsDurable(true);
  header.SetTimeToLive(std::chrono::milliseconds(512));
  AmqpValue headerValue{AmqpValue::CreateHeader(header)};

  EXPECT_TRUE(header.IsDurable());

  Header headerFromValue{headerValue.GetHeaderFromValue()};
  EXPECT_EQ(header, headerFromValue);
}