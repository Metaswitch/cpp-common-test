/**
 * @file communicationmonitor_test.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#include "communicationmonitor.h"
#include "mockalarm.h"

using ::testing::Return;

/// Fixture for CommunicationMonitor test.
class CommunicationMonitorTest : public ::testing::Test
{
public:
  CommunicationMonitorTest() :
    _alarm_mgr(new AlarmManager()),
    _mock_alarm(new MockAlarm(_alarm_mgr)),
    _cm(_mock_alarm, "sprout", "chronos")
  {
    cwtest_completely_control_time();
  }

  virtual ~CommunicationMonitorTest()
  {
    cwtest_reset_time();
    delete _alarm_mgr; _alarm_mgr = NULL;
  }

private:
  AlarmManager* _alarm_mgr;
  MockAlarm* _mock_alarm;
  CommunicationMonitor _cm;
};

// Tests that the alarm is raised at the corect point as we move up error states.
TEST_F(CommunicationMonitorTest, ErrorsStateIncrement)
{
  // Pass in a success and failure to update the communication monitor at the
  // same time. We do this by setting one, advancing time beyond the 'next_check'
  // interval, and then setting the other. This should clear the alarm.
  _cm.inform_success();
  cwtest_advance_time_ms(16000);

  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_failure();

  // Now we set a failure after the set_confirm interval has passed again.
  // This should raise the alarm.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();
}

// Tests that the alarm is cleared as we move down the error states.
TEST_F(CommunicationMonitorTest, ErrorStateDecrement)
{
  // First pass in a failure after the next_check interval. This should set the alarm.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();

  // Now pass in a success and failure together, after the clear_confirm interval.
  // Again we do this by setting one, advancing time, then setting the other to
  // trigger the update. This should clear the alarm.
  _cm.inform_success();
  cwtest_advance_time_ms(31000);

  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_failure();

  // Pass in a success after the set_confirm interval has passed again.
  // This should re-clear the alarm.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_success();
}

// Tests that the alarm is raised and cleared on moving from NO_ERRORS to ONLY_ERRORS and back.
TEST_F(CommunicationMonitorTest, OnlyErrorsToNoErrorsUpdate)
{
  // First pass in a failure after the next_check interval. This should set the alarm.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();

  // Now pass in a success after the clear_confirm interval.
  // This should clear the alarm.
  cwtest_advance_time_ms(31000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_success();
}

// Test when going from the same state to the same state the alarm state is re-raised.
TEST_F(CommunicationMonitorTest, StableStates)
{
  // Send in two NO_ERROR states
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_success();
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_success();

  // Send in two SOME_ERROR states
  _cm.inform_success();
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_failure();
  _cm.inform_success();
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_failure();

  // Send in two ONLY_ERROR states
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();
  cwtest_advance_time_ms(31000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();
}

// Tests that the set_confirm_ms check is working.
// The communication monitor should only update to set an alarm if the
// time has advanced by the set_confirm_ms interval at the time of update.
TEST_F(CommunicationMonitorTest, TestSetConfirmMs)
{
  // Run through an update with a success and failure together. This should clear the alarm.
  // This will set us to the SOME_ERRORS state, and set the next_check interval to now + set_confirm_ms.
  _cm.inform_success();
  cwtest_advance_time_ms(16000);

  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  _cm.inform_failure();

  // Advance time by less than the set_confirm interval, and set a failure.
  // This should do nothing.
  cwtest_advance_time_ms(10000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();

  // Advance time beyond the set_confirm interval, but less than clear_confirm, and set a failure.
  // This should now set the alarm.
  cwtest_advance_time_ms(10000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();
}

// Tests that the clear_confirm check is working.
// The communication monitor should only update to clear an alarm if the
// time has advanced by the clear_confirm_ms interval at the time of update.
TEST_F(CommunicationMonitorTest, TestClearConfirmMs)
{
  // Pass in a failure after passing the set_confirm interval.
  // This should set the alarm, and set next_check to now + clear_confirm_ms.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_failure();

  // Advance time less than clear_confirm, but more than set_confirm, and inform of a success.
  // This should do nothing.
  cwtest_advance_time_ms(16000);
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);
  _cm.inform_success();

  // Advance time again, beyond clear_confirm, and inform of a success.
  // This should now clear the alarm.
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);
  cwtest_advance_time_ms(16000);
  _cm.inform_success();
}
