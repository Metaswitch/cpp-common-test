/**
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils.h"
#include "test_utils.hpp"
#include "test_interposer.hpp"

#include "timer_heap.h"

using ::testing::Return;

class TimerHeapTest : public ::testing::Test
{
public:
  TimerHeapTest()
  {
  }
  
  TimerHeap th;
};

// When I insert a timer into the heap, I should be able to get it back out.
TEST_F(TimerHeapTest, Insert)
{
  SimpleTimer t(1000);

  th.insert(&t);
  HeapableTimer* next = th.get_next_timer();

  ASSERT_EQ(&t, next);
}

// When I insert a timer into the heap and then remove it, it shouldn't still
// be in the heap.
TEST_F(TimerHeapTest, Remove)
{
  SimpleTimer t(1000);

  th.insert(&t);
  bool success = th.remove(&t);
  EXPECT_TRUE(success);
  HeapableTimer* next = th.get_next_timer();

  ASSERT_EQ(nullptr, next);
}

// The remove() method returns false if the provided timer isn't in the heap.
TEST_F(TimerHeapTest, RemoveNonexistentTimer)
{
  SimpleTimer t(1000);

  bool success = th.remove(&t);
  EXPECT_FALSE(success);
}


// If I don't have any timers in the heap, get_next_timer shouldn't return one.
TEST_F(TimerHeapTest, EmptyHeapGivesNull)
{
  HeapableTimer* next = th.get_next_timer();

  ASSERT_EQ(nullptr, next);
}

// If I insert three timers and then pop from the heap, the heap should always
// return me the timer with the lowest pop time.
TEST_F(TimerHeapTest, MultipleInsert)
{
  SimpleTimer t1(1000);
  SimpleTimer t2(1002);
  SimpleTimer t3(1001);

  th.insert(&t2);
  th.insert(&t1);
  th.insert(&t3);

  HeapableTimer* next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t1, next);

  next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t3, next);

  next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t2, next);
}

// If I insert three timers, reduce the pop time of one of them, and then pop
// from the heap, the heap should always return me the timer with the lowest
// pop time (taking the reduced pop time into account).
TEST_F(TimerHeapTest, UpdatePopTime)
{
  SimpleTimer t1(1000);
  SimpleTimer t2(1002);
  SimpleTimer t3(1001);

  th.insert(&t2);
  th.insert(&t1);
  th.insert(&t3);

  t2.update_pop_time(6);

  HeapableTimer* next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t2, next);

  next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t1, next);

  next = th.get_next_timer();
  th.remove(next);
  ASSERT_EQ(&t3, next);
}

// Checks that pop times all the way up to UINT_MAX are handled correctly
TEST_F(TimerHeapTest, UintMaxPopTime)
{
  SimpleTimer t1(120171267);
  SimpleTimer t2(120171269);
  SimpleTimer t3(120171268);

  th.insert(&t2);
  th.insert(&t1);
  th.insert(&t3);

  HeapableTimer* next = th.get_next_timer();
  ASSERT_EQ(&t1, next);
  t1.update_pop_time(UINT_MAX);

  // We've increased t1's pop time, so t3 should now be first to pop
  next = th.get_next_timer();
  ASSERT_EQ(&t3, next);
}


// This is a quite thorough test to make sure that all possible operations on
// the heap preserve the heap property. It:
//
// * generates 10,000 timers with random pop times and inserts them
// * takes 1,000 of those timers and changes their pop time, which requires
//   rebalancing the heap
// * takes 1,000 of those timers and deletes them, which also requires
//   rebalancing the heap
//
// It then pops from the heap until it is empty, and ensures that it got 9,000
// timers out and they are ordered from sonest pop time to furthest-away pop
// time.
TEST_F(TimerHeapTest, ManyTimers)
{
  std::vector<SimpleTimer*> inserted_timers;
  std::vector<HeapableTimer*> popped_timers;
  
  for (int i = 0; i < 10000; i++)
  {
    SimpleTimer* t = new SimpleTimer(rand());
    inserted_timers.push_back(t);
    th.insert(t);
  }

  for (int i = 0; i < 1000; i++)
  {
    unsigned int index = rand() % 10000;
    SimpleTimer* t = inserted_timers[index];
    t->update_pop_time(rand());
  }

  for (int i = 0; i < 1000; i++)
  {
    SimpleTimer* t = inserted_timers.back(); inserted_timers.pop_back();
    th.remove(t);
    delete t;
  }

  HeapableTimer* next;
  while ((next = th.get_next_timer()) != nullptr)
  {
    th.remove(next);
    popped_timers.push_back(next);
  }

  ASSERT_EQ(9000u, popped_timers.size());
  ASSERT_TRUE(std::is_sorted(popped_timers.begin(),
                             popped_timers.end(),
                             [](HeapableTimer* a, HeapableTimer* b) { return a->get_pop_time() < b->get_pop_time(); }));
  for (SimpleTimer* t: inserted_timers)
  {
    delete t;
  }
}

