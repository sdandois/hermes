#include "hermes/VM/DictPropertyMap.h"

#include "TestHelpers.h"

#include "gtest/gtest.h"

using namespace hermes::vm;

namespace {

using DictPropertyMapTest = LargeHeapRuntimeTestFixture;

TEST_F(DictPropertyMapTest, SmokeTest) {
  auto id1 = SymbolID::unsafeCreate(1);
  auto id2 = SymbolID::unsafeCreate(2);
  auto id3 = SymbolID::unsafeCreate(3);
  auto id4 = SymbolID::unsafeCreate(4);

  NamedPropertyDescriptor desc1{};

  MutableHandle<DictPropertyMap> map{runtime,
                                     DictPropertyMap::create(runtime, 2).get()};
  auto saveMap = map.get();

  // Try to find a property in the empty map.
  ASSERT_FALSE(DictPropertyMap::find(map.get(), id1));

  // Add prop1.
  DictPropertyMap::add(map, runtime, id1, desc1);
  ASSERT_EQ(1u, map->size());

  auto found = DictPropertyMap::find(*map, id1);
  ASSERT_TRUE(found);
  ASSERT_EQ(id1, DictPropertyMap::getDescriptorPair(*map, *found)->first);

  // Add prop2.
  DictPropertyMap::add(map, runtime, id2, desc1);
  ASSERT_EQ(2u, map->size());

  // Find prop1, prop2.
  found = DictPropertyMap::find(*map, id1);
  ASSERT_TRUE(found);
  ASSERT_EQ(id1, DictPropertyMap::getDescriptorPair(*map, *found)->first);
  found = DictPropertyMap::find(*map, id2);
  ASSERT_TRUE(found);
  ASSERT_EQ(id2, DictPropertyMap::getDescriptorPair(*map, *found)->first);

  // Make sure we haven't reallocated.
  ASSERT_EQ(saveMap, map.get());

  // Add a prop3, causing a reallocation.
  DictPropertyMap::add(map, runtime, id3, desc1);
  // Make sure we reallocated.
  ASSERT_NE(saveMap, map.get());
  saveMap = map.get();
  for (unsigned i = 0; i < 3; ++i) {
    auto sym = SymbolID::unsafeCreate(id1.unsafeGetIndex() + i);
    found = DictPropertyMap::find(*map, sym);
    ASSERT_TRUE(found);
    ASSERT_EQ(sym, DictPropertyMap::getDescriptorPair(*map, *found)->first);
  }

  // Add a prop4.
  DictPropertyMap::add(map, runtime, id4, desc1);
  ASSERT_EQ(saveMap, map.get());
  for (unsigned i = 0; i < 4; ++i) {
    auto sym = SymbolID::unsafeCreate(id1.unsafeGetIndex() + i);
    found = DictPropertyMap::find(*map, sym);
    ASSERT_TRUE(found);
    ASSERT_EQ(sym, DictPropertyMap::getDescriptorPair(*map, *found)->first);
  }

  // Verify the enumeration order.
  {
    unsigned symIndex = 1;
    DictPropertyMap::forEachProperty(
        map, runtime, [&](SymbolID id, NamedPropertyDescriptor desc) {
          ASSERT_EQ(symIndex, id.unsafeGetIndex());
          ++symIndex;
        });
  }

  // Delete prop2.
  found = DictPropertyMap::find(*map, id2);
  DictPropertyMap::erase(*map, *found);
  ASSERT_EQ(saveMap, map.get());
  ASSERT_EQ(3u, map->size());

  {
    static const unsigned symbols[] = {1, 3, 4};
    for (unsigned i = 0; i < 3; ++i) {
      auto sym = SymbolID::unsafeCreate(symbols[i]);
      found = DictPropertyMap::find(*map, sym);
      ASSERT_TRUE(found);
      ASSERT_EQ(sym, DictPropertyMap::getDescriptorPair(*map, *found)->first);
    }
    unsigned i = 0;
    DictPropertyMap::forEachProperty(
        map, runtime, [&](SymbolID id, NamedPropertyDescriptor desc) {
          ASSERT_EQ(symbols[i], id.unsafeGetIndex());
          ++i;
        });
  }

  // Add prop2 again.
  DictPropertyMap::add(map, runtime, id2, desc1);
  ASSERT_EQ(4u, map->size());

  {
    static const unsigned symbols[] = {1, 3, 4, 2};
    for (unsigned i = 0; i < 4; ++i) {
      auto sym = SymbolID::unsafeCreate(symbols[i]);
      found = DictPropertyMap::find(*map, sym);
      ASSERT_TRUE(found);
      ASSERT_EQ(sym, DictPropertyMap::getDescriptorPair(*map, *found)->first);
    }
    unsigned i = 0;
    DictPropertyMap::forEachProperty(
        map, runtime, [&](SymbolID id, NamedPropertyDescriptor desc) {
          ASSERT_EQ(symbols[i], id.unsafeGetIndex());
          ++i;
        });
  }
}
} // namespace
