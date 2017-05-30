/**
 * @file load_monitor_test.cpp UT for LoadMonitor classes.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

///
///----------------------------------------------------------------------------

#include <string>
#include "gtest/gtest.h"

#include "basetest.hpp"
#include "fakesnmp.hpp"
#include "load_monitor.h"
#include "test_interposer.hpp"

static SNMP::U32Scalar smoothed_latency("", "");
static SNMP::U32Scalar target_latency("", "");
static SNMP::U32Scalar penalties("", "");
static SNMP::U32Scalar token_rate("","");

/// Fixture for LoadMonitorTest.
class LoadMonitorTest : public BaseTest
{
  LoadMonitor* _load_monitor;

  LoadMonitorTest()
  {
    cwtest_completely_control_time();
    _load_monitor = new LoadMonitor(100000,
                                    20,
                                    10,
                                    10,
                                    &SNMP::FAKE_CONTINUOUS_ACCUMULATOR_TABLE,
                                    &smoothed_latency,
                                    &target_latency,
                                    &penalties,
                                    &token_rate);
  }

  virtual ~LoadMonitorTest()
  {
    delete _load_monitor;
    cwtest_reset_time();
  }

  void request_with_latency(int latency)
  {
    _load_monitor->admit_request(0);
    _load_monitor->request_complete(latency);
  }
};

class TokenBucketTest : public BaseTest
{
  TokenBucket _token_bucket;

  TokenBucketTest() :
    _token_bucket(20, 10)
  {
  }

  virtual ~TokenBucketTest()
  {
  }
};

TEST_F(LoadMonitorTest, RequestComplete)
{
  float initial_rate = _load_monitor->bucket.rate;

  // Keep the latency at the expected value.
  for (int ii = 0; ii < 20; ii++)
  {
    request_with_latency(100000);
  }

  // The token rate is unchanged, because although we've seen 20 requests, 2 seconds haven't passed
  EXPECT_EQ(_load_monitor->bucket.rate, initial_rate);

  // Move time forwards 2 seconds and inject another request.
  cwtest_advance_time_ms(2000);

  request_with_latency(100000);

  // Bucket fill rate should still be at the initial rate, because the latency is as expected.
  EXPECT_EQ(_load_monitor->bucket.rate, initial_rate);
  initial_rate = _load_monitor->bucket.rate;

  // Keep the latency low, but without a penalty.
  for (int ii = 0; ii < 20; ii++)
  {
   request_with_latency(1000);
  }

  // The token rate is unchanged, because although we've seen 20 requests, 2 seconds haven't passed
  EXPECT_EQ(_load_monitor->bucket.rate, initial_rate);

  // Move time forwards 2 seconds and inject another request.
  cwtest_advance_time_ms(2000);
  request_with_latency(1000);

  // Bucket fill rate should have increased due to the low latency.
  EXPECT_GT(_load_monitor->bucket.rate, initial_rate);

  float changed_rate = _load_monitor->bucket.rate;

  // Keep the latency low, but incur a penalty.
  _load_monitor->incr_penalties();

  for (int ii = 0; ii < 20; ii++)
  {
    request_with_latency(1000);
  }

  // The token rate is unchanged, because although we've seen 20 requests, 2 seconds haven't passed
  EXPECT_EQ(_load_monitor->bucket.rate, changed_rate);

  // Move time forwards 2 seconds and inject another request.
  cwtest_advance_time_ms(2000);
  request_with_latency(1000);

  // Bucket fill rate should have decreased due to the penalty.
  EXPECT_LT(_load_monitor->bucket.rate, changed_rate);

}

TEST_F(LoadMonitorTest, NoRateDecreaseBelowMinimum)
{
  float initial_rate = _load_monitor->bucket.rate;

  for (int ii = 0; ii < 20; ii++)
  {
    request_with_latency(100000000);
  }

  // Move time forwards 2 seconds and inject another request.
  cwtest_advance_time_ms(2000);
  request_with_latency(100000000);

  // Bucket fill rate should be unchanged at the minimum value.
  EXPECT_EQ(_load_monitor->bucket.rate, initial_rate);
}

TEST_F(LoadMonitorTest, NoRateIncreaseWithoutGoodEvidence)
{
  float initial_rate = _load_monitor->bucket.rate;

  for (int ii = 0; ii < 20; ii++)
  {
    // Handle one request very quickly, but only do so every five seconds - as would happen on a
    // Sprout at non-peak hours, for example.
    request_with_latency(1);
    cwtest_advance_time_ms(5000);
  }

  // Bucket fill rate should be unchanged - we aren't using the token bucket to capacity, so the
  // small number of infrequent requests aren't enough evidence for us to increase the rate.
  EXPECT_EQ(initial_rate, _load_monitor->bucket.rate);
}


TEST_F(LoadMonitorTest, AdmitRequest)
{
  // Test that initially the load monitor admits requests, but after a large number
  // of attempts in quick succession it has run out.
  EXPECT_EQ(_load_monitor->admit_request(0), true);

  for (int ii = 0; ii <= 50; ii++)
  {
    _load_monitor->admit_request(0);
  }

  EXPECT_EQ(_load_monitor->admit_request(0), false);
}

TEST_F(TokenBucketTest, GetToken)
{
  // Test that initially the token bucket gives out tokens, but after a large number
  // of attempts in quick succession it has run out.
  bool got_token = _token_bucket.get_token();
  EXPECT_EQ(got_token, true);

  for (int ii = 0; ii <= 50; ii++)
  {
    got_token = _token_bucket.get_token();
  }

  EXPECT_EQ(got_token, false);

}


TEST_F(LoadMonitorTest, CorrectStatistics)
{
  // Scalars should report values from last update, not current values.
  // Initialisation should count as an update though
  // Observe these values are the values that the load monitor has been
  // initialised with
  EXPECT_EQ(target_latency.value, (uint32_t)100000);
  EXPECT_EQ(smoothed_latency.value, (uint32_t)100000);
  EXPECT_EQ(penalties.value, (uint32_t)0);
  EXPECT_EQ(token_rate.value, (uint32_t)10);

  // Give low latency value and force rate update through penalty
  _load_monitor->incr_penalties();
  _load_monitor->request_complete(100);

  _load_monitor->request_complete(100000000);

  // Scalar value should be reported as less than current value
  // as the current smoothed latency will have increased
  EXPECT_GT((uint32_t)_load_monitor->smoothed_latency, smoothed_latency.value);
}

