/**
 * @file alarm_test.cpp
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

#include "alarm.h"
#include "fakezmq.h"
#include "fakelogger.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::InSequence;
using ::testing::SafeMatcherCast;
using ::testing::StrEq;

static const char issuer[] = "sprout";

MATCHER_P(VoidPointeeEqualsInt, value, "") 
{
  return (*((int*)arg) == value);
}

namespace AlarmDef {
static const int CPP_COMMON_FAKE_ALARM1 = 9999;
static const int CPP_COMMON_FAKE_ALARM2 = 9998;
static const int CPP_COMMON_FAKE_ALARM3 = 9997;
static const int CPP_COMMON_FAKE_ALARM4 = 9996;
static const int CPP_COMMON_FAKE_ALARM5 = 9995;
static const int CPP_COMMON_FAKE_ALARM6 = 9994;
static const int CPP_COMMON_FAKE_ALARM7 = 9993;
static const int CPP_COMMON_FAKE_ALARM8 = 9992;
static const int CPP_COMMON_FAKE_ALARM9 = 9991;
static const int CPP_COMMON_FAKE_ALARM10 = 9990;
}

class AlarmTest : public ::testing::Test
{
public:
  AlarmTest() :
    _alarm_state(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL),
    _alarm(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM2, AlarmDef::MAJOR),
    _multi_state_alarm(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM3),
    _c(1),
    _s(2)
  {
    cwtest_intercept_zmq(&_mz);

    EXPECT_CALL(_mz, zmq_ctx_new())
      .Times(1)
      .WillOnce(Return(&_c));

    EXPECT_CALL(_mz, zmq_socket(VoidPointeeEqualsInt(_c),ZMQ_REQ))
      .Times(1)
      .WillOnce(Return(&_s));

    EXPECT_CALL(_mz, zmq_setsockopt(VoidPointeeEqualsInt(_s),ZMQ_LINGER,_,_))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_connect(VoidPointeeEqualsInt(_s),StrEq("ipc:///var/run/clearwater/alarms"))) 
      .Times(1)
      .WillOnce(Return(0));

    AlarmReqAgent::get_instance().start();
  }

  virtual ~AlarmTest()
  {
    EXPECT_CALL(_mz, zmq_close(VoidPointeeEqualsInt(_s)))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_ctx_destroy(VoidPointeeEqualsInt(_c)))
      .Times(1)
      .WillOnce(Return(0));
    
    AlarmManager::get_instance()._alarm_list.clear();
    AlarmReqAgent::get_instance().stop();
    cwtest_restore_zmq();
  }

private:
  MockZmqInterface _mz;
  AlarmState _alarm_state;
  Alarm _alarm;
  MultiStateAlarm _multi_state_alarm;
  int _c;
  int _s;
};

class MultipleAlarmsTest : public ::testing::Test
{
public:
  MultipleAlarmsTest() :
    _alarm_state(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL),
    _alarm(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM2, AlarmDef::MAJOR),
    _multi_state_alarm(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM3),
    _alarm4(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM4, AlarmDef::INDETERMINATE),
    _alarm5(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM5, AlarmDef::WARNING),
    _alarm6(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM6, AlarmDef::MINOR),
    _multi_state_alarm7(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM7),
    _multi_state_alarm8(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM8),
    _multi_state_alarm9(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM9),
    _multi_state_alarm10(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM10),
    _c(1),
    _s(2)
  {
    cwtest_intercept_zmq(&_mz);

    EXPECT_CALL(_mz, zmq_ctx_new())
      .Times(1)
      .WillOnce(Return(&_c));

    EXPECT_CALL(_mz, zmq_socket(VoidPointeeEqualsInt(_c),ZMQ_REQ))
      .Times(1)
      .WillOnce(Return(&_s));

    EXPECT_CALL(_mz, zmq_setsockopt(VoidPointeeEqualsInt(_s),ZMQ_LINGER,_,_))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_connect(VoidPointeeEqualsInt(_s),StrEq("ipc:///var/run/clearwater/alarms"))) 
      .Times(1)
      .WillOnce(Return(0));

    AlarmReqAgent::get_instance().start();
  }

  virtual ~MultipleAlarmsTest()
  {
    EXPECT_CALL(_mz, zmq_close(VoidPointeeEqualsInt(_s)))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_ctx_destroy(VoidPointeeEqualsInt(_c)))
      .Times(1)
      .WillOnce(Return(0));
    
    AlarmManager::get_instance()._alarm_list.clear();
    AlarmReqAgent::get_instance().stop();
    cwtest_restore_zmq();
  }

private:
  MockZmqInterface _mz;
  AlarmState _alarm_state;
  Alarm _alarm;
  MultiStateAlarm _multi_state_alarm;
  Alarm _alarm4;
  Alarm _alarm5;
  Alarm _alarm6;
  MultiStateAlarm _multi_state_alarm7;
  MultiStateAlarm _multi_state_alarm8;
  MultiStateAlarm _multi_state_alarm9;
  MultiStateAlarm _multi_state_alarm10;
  int _c;
  int _s;
};
class AlarmQueueErrorTest : public ::testing::Test
{
public:
  AlarmQueueErrorTest() :
    _alarm_state(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL)
  {
    AlarmReqAgent::get_instance().start();
  }

  virtual ~AlarmQueueErrorTest()
  {
    AlarmReqAgent::get_instance().stop();
  }

private:
  AlarmState _alarm_state;
};

class AlarmZmqErrorTest : public ::testing::Test
{
public:
  AlarmZmqErrorTest() :
    _alarm_state(issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL),
    _c(1),
    _s(2)
  {
    cwtest_intercept_zmq(&_mz);
  }

  virtual ~AlarmZmqErrorTest()
  {
    cwtest_restore_zmq();
  }

private:
  MockZmqInterface _mz;
  AlarmState _alarm_state;
  int _c;
  int _s;
};

MATCHER_P(VoidPointeeEqualsStr, value, "") 
{
  return (strcmp((char*)arg, value) == 0);
}

TEST_F(AlarmTest, MultiStateAlarmRaising)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.3"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }
  // Raises an alarm twice using different states each time. We check above for
  // two ZMQ messages, the second containing the updated raised state.
  _multi_state_alarm.multi_set_critical();
  _multi_state_alarm.multi_set_major();
  // Causes the test thread to wait to ensure the zmq messages get sent before
  // the test terminates.
  for (unsigned int i = 0; i < 2; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(AlarmTest, AlarmAndMultiStateAlarmRaising)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.5"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }
  // Raises an alarm with only one possible raised state and a MultiStateAlarm
  // raised with two of its possible raised states.
  _alarm.set();
  _multi_state_alarm.multi_set_indeterminate();
  _multi_state_alarm.multi_set_minor();
  
  //No further ZMQ messages should be sent for these alarms as they are
  //attempting to raise a state they are already in.
  _alarm.set();
  _multi_state_alarm.multi_set_minor();

  // Causes the test thread to wait to ensure the zmq messages get sent before
  // the test terminates.
  for (unsigned int i = 0; i < 3; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(AlarmTest, MultiStateAlarmClearing)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.6"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.5"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }
  // Raises a MultiStateAlarm with two of its possible states and then clears
  // it.
  _multi_state_alarm.multi_set_warning();
  _multi_state_alarm.multi_set_minor();
  _multi_state_alarm.clear();
  
  // Causes the test thread to wait to ensure the zmq messages get sent before
  // the test terminates.
  for (unsigned int i = 0; i < 3; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(AlarmTest, ResendingAlarm)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    // We should expect the alarm we raised above to be reraised and also the
    // MultiStateAlarm alarm (which was never raised) should be cleared.
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }
  // Raises an alarm with only one possible raised state.
  _alarm.set();
  // Simulates 30 seconds of time passing to trigger the alarms being re-raised.
  cwtest_advance_time_ms(30000);
  // Causes the test thread to wait to ensure the zmq messages get sent before
  // the test terminates.
  for (unsigned int i = 0; i < 3; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(MultipleAlarmsTest, ResendingAlarmsAndMultiStateAlarms)
{
  {
    InSequence s;

    // These are the ZMQ messages we expect to be sent for the alarms we raise
    // and clear manually below.
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9996.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9995.6"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9993.6"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9993.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9992.5"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9991.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9991.3"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    // These are the ZMQ messages we expect to be sent automatically by the
    // function to reraise alarms every thirty seconds.
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9997.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9996.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9995.6"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9994.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9993.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9992.5"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9991.3"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9990.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

  }
  // This test cover a wide range of raising alarm cases to test how they are
  // reraised every thiry seconds with both single state Alarms and
  // MultiStateAlarms. These tests include: purposely reraising an alarm with 
  // the same state (which should only cause one ZMQ message to be sent),
  // reraising an alarm with a different state (which should cause two ZMQ
  // messages to be sent, but every thirty seconds only the most recent state
  // is reraised), not raising the alarm and clearing a raised alarm (which
  // should both result in the CLEARED status being resent every thiry seconds).
  _alarm.set();
  _alarm.clear();
  _multi_state_alarm.multi_set_indeterminate();
  _alarm4.set();
  _alarm5.set();
  _alarm5.set();
  _multi_state_alarm7.multi_set_warning();
  _multi_state_alarm7.clear();
  _multi_state_alarm8.multi_set_minor();
  _multi_state_alarm8.multi_set_minor();
  _multi_state_alarm9.multi_set_major();
  _multi_state_alarm9.multi_set_critical();
  // Simulates 30 seconds of time passing to trigger the alarms being re-raised.
  cwtest_advance_time_ms(30000);
  // Causes the test thread to wait to ensure the zmq messages get sent before
  // the test terminates.
  for (unsigned int i = 0; i < 19; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(AlarmTest, IssueAlarm)
{
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),11,ZMQ_SNDMORE))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),strlen(issuer),ZMQ_SNDMORE))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9999.3"),6,0))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));
  }
  _alarm_state.issue();
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
}

TEST_F(AlarmTest, ClearAlarms)
{
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("clear-alarms"),12,ZMQ_SNDMORE))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),strlen(issuer),0))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));
  }
  AlarmState::clear_all(issuer);
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
}

TEST_F(AlarmTest, PairSetNotAlarmed)
{
  InSequence s;

  EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),11,ZMQ_SNDMORE))
    .Times(1)
    .WillOnce(Return(0));

  EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),strlen(issuer),ZMQ_SNDMORE))
    .Times(1)
    .WillOnce(Return(0));

  EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),6,0))
    .Times(1)
    .WillOnce(Return(0));

  EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
    .Times(1)
    .WillOnce(Return(0));

  _alarm.set();
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
}

TEST_F(AlarmTest, PairSetAlarmed)
{
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm.set();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }

  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).Times(0);
  _alarm.set();
}

TEST_F(AlarmTest, PairClearNotAlarmed)
{
  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).Times(0);
  _alarm.clear();
}

TEST_F(AlarmTest, PairClearAlarmed)
{
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm.set();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }

  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),11,ZMQ_SNDMORE))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),strlen(issuer),ZMQ_SNDMORE))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),6,0))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm.clear();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

TEST_F(AlarmQueueErrorTest, Overflow)
{
  CapturingTestLogger log;

  // Produce 20 more requests than the max queue depth, so that this
  // test isn't fragile on a fast machine which might take a couple of
  // items off the queue.
  for (int idx = 0; idx < AlarmReqAgent::MAX_Q_DEPTH+20; idx++)
  {
    _alarm_state.issue();
  }
  EXPECT_TRUE(log.contains("queue overflowed"));
}

TEST_F(AlarmZmqErrorTest, CreateContext)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(ReturnNull());

  EXPECT_FALSE(AlarmReqAgent::get_instance().start());
  EXPECT_TRUE(log.contains("zmq_ctx_new failed"));
}

TEST_F(AlarmZmqErrorTest, CreateSocket)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)) .WillOnce(ReturnNull());
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());

  AlarmReqAgent::get_instance().stop();
  EXPECT_TRUE(log.contains("zmq_socket failed"));
}

TEST_F(AlarmZmqErrorTest, SetSockOpt)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());

  AlarmReqAgent::get_instance().stop();
  EXPECT_TRUE(log.contains("zmq_setsockopt failed"));
}

TEST_F(AlarmZmqErrorTest, Connect)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_connect(_,_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());

  AlarmReqAgent::get_instance().stop();
  EXPECT_TRUE(log.contains("zmq_connect failed"));
}

TEST_F(AlarmZmqErrorTest, Send)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_connect(_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_close(_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());
  _alarm_state.issue();
  _mz.call_complete(ZmqInterface::ZMQ_SEND, 5);

  AlarmReqAgent::get_instance().stop();
  EXPECT_TRUE(log.contains("zmq_send failed"));
}

TEST_F(AlarmZmqErrorTest, Receive)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_connect(_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).Times(3).WillRepeatedly(Return(0));
  EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_close(_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());
  _alarm_state.issue();
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);

  AlarmReqAgent::get_instance().stop();
  EXPECT_TRUE(log.contains("zmq_recv failed"));
}

TEST_F(AlarmZmqErrorTest, CloseSocket)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_connect(_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_close(_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());
  AlarmReqAgent::get_instance().stop();

  EXPECT_TRUE(log.contains("zmq_close failed"));
}

TEST_F(AlarmZmqErrorTest, DestroyContext)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_connect(_,_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_close(_)).WillOnce(Return(0));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(-1));

  EXPECT_TRUE(AlarmReqAgent::get_instance().start());
  AlarmReqAgent::get_instance().stop();

  EXPECT_TRUE(log.contains("zmq_ctx_destroy failed"));
}

