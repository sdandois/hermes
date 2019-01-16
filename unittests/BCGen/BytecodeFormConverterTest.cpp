#include "hermes/BCGen/HBC/BytecodeFormConverter.h"
#include "TestHelpers.h"
#include "hermes/BCGen/HBC/BytecodeFileFormat.h"
#include "llvm/ADT/SmallVector.h"

#include "gtest/gtest.h"

using namespace hermes;
using namespace hermes::hbc;

namespace {

TEST(BytecodeFormConverterTest, RoundtripTest) {
  auto bytecode = bytecodeForSource("print('Hello World');");
  const auto savedBytecode = bytecode;
  ConstBytecodeFileFields fields;
  bool populated = fields.populateFromBuffer(bytecode, nullptr);
  ASSERT_TRUE(populated);
  EXPECT_EQ(fields.header->magic, hbc::MAGIC);

  bool converted1 =
      convertBytecodeToForm(bytecode, BytecodeForm::Delta, nullptr);
  EXPECT_TRUE(converted1);
  EXPECT_EQ(fields.header->magic, DELTA_MAGIC);

  bool converted2 =
      convertBytecodeToForm(bytecode, BytecodeForm::Execution, nullptr);
  EXPECT_TRUE(converted2);
  EXPECT_EQ(fields.header->magic, MAGIC);
  EXPECT_EQ(savedBytecode, bytecode);
}

TEST(BytecodeFormConverterTest, SourceHashTest) {
  auto bytecode = bytecodeForSource("print('Hello World');");
  ConstBytecodeFileFields fields;
  bool populated = fields.populateFromBuffer(bytecode, nullptr);
  ASSERT_TRUE(populated);
  auto &hash = fields.header->sourceHash;
  SHA1 actualHash;
  std::copy(std::begin(hash), std::end(hash), actualHash.begin());
  SHA1 expectedHash{{0x54, 0x96, 0xc5, 0xa0, 0xb5, 0x9b, 0x38,
                     0x96, 0xbd, 0xd8, 0x66, 0x87, 0xaf, 0xe5,
                     0x9a, 0x3d, 0x51, 0xd2, 0xc7, 0xb2}};
  EXPECT_EQ(expectedHash, actualHash);
}

TEST(BytecodeFormConverterTest, WrongMagicNumberTest) {
  std::string error;
  std::vector<uint8_t> bytecode = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  bool converted = convertBytecodeToForm(bytecode, BytecodeForm::Delta, &error);
  EXPECT_FALSE(converted);
  EXPECT_EQ(error, "Buffer too small");
}

TEST(BytecodeFormConverterTest, NotDeltaFormTest) {
  std::string error;
  auto bytecode = bytecodeForSource("print('Hello World');");
  bool converted =
      convertBytecodeToForm(bytecode, BytecodeForm::Execution, &error);
  EXPECT_FALSE(converted);
  EXPECT_EQ(error, "Incorrect magic number");
}

TEST(BytecodeFormConverterTest, NotExecutionFormTest) {
  std::string error;
  auto bytecode = bytecodeForSource("print('Hello World');");
  bool converted = convertBytecodeToForm(bytecode, BytecodeForm::Delta, &error);
  EXPECT_TRUE(converted);
  EXPECT_TRUE(error.empty());

  converted = convertBytecodeToForm(bytecode, BytecodeForm::Delta, &error);
  EXPECT_FALSE(converted);
  EXPECT_EQ(error, "Incorrect magic number");
}

} // end anonymous namespace
