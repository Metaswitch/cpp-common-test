/**
 * @file alarm_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
}

class AlarmTest : public ::testing::Test
{
public:
  AlarmTest() :
    _alarm_manager(NULL),
    _alarm_state(NULL),
    _alarm(NULL),
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

    EXPECT_CALL(_mz, zmq_connect(VoidPointeeEqualsInt(_s),_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm_manager = new AlarmManager();
    _alarm_state = new AlarmState(_alarm_manager->alarm_req_agent(), issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL);
    _alarm = new Alarm(_alarm_manager, issuer, AlarmDef::CPP_COMMON_FAKE_ALARM2);

    // Wait until the AlarmReRaiser is waiting before we start (so it doesn't
    // try and reraise any alarms unexpectedly).
    _alarm_manager->alarm_re_raiser()->_condition->block_till_waiting();
  }

  virtual ~AlarmTest()
  {
    delete _alarm; _alarm = NULL;
    delete _alarm_state; _alarm_state = NULL;

    EXPECT_CALL(_mz, zmq_close(VoidPointeeEqualsInt(_s)))
      .Times(1)
      .WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_ctx_destroy(VoidPointeeEqualsInt(_c)))
      .Times(1)
      .WillOnce(Return(0));
    
    delete _alarm_manager; _alarm_manager = NULL;
    cwtest_restore_zmq();
  }

private:
  MockZmqInterface _mz;
  AlarmManager* _alarm_manager;
  AlarmState* _alarm_state;
  Alarm* _alarm;
  int _c;
  int _s;
};

class AlarmQueueErrorTest : public ::testing::Test
{
public:
  AlarmQueueErrorTest() :
    _alarm_manager(new AlarmManager()),
    _alarm_state(_alarm_manager->alarm_req_agent(), issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL)
  {
    // Wait until the AlarmReRaiser is waiting before we start (so it doesn't
    // try and reraise any alarms unexpectedly).
    _alarm_manager->alarm_re_raiser()->_condition->block_till_waiting();
  }

  virtual ~AlarmQueueErrorTest()
  {
    delete _alarm_manager; _alarm_manager = NULL;
  }

private:
  AlarmManager* _alarm_manager;
  AlarmState _alarm_state;
};

class AlarmZmqErrorTest : public ::testing::Test
{
public:
  AlarmZmqErrorTest() :
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
  int _c;
  int _s;
};

MATCHER_P(VoidPointeeEqualsStr, value, "") 
{
  return (strcmp((char*)arg, value) == 0);
}

// Tests that get_alarm_state returns the correct states
TEST_F(AlarmTest, GetAlarmState)
{
  // The alarm should start in UNKNOWN state
  ASSERT_EQ(AlarmState::UNKNOWN, _alarm->get_alarm_state());

  // Raise alarm at one severity, and assert it is now ALARMED
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm->set_major();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
  ASSERT_EQ(AlarmState::ALARMED, _alarm->get_alarm_state());

  // Raise alarm at another severity, and assert is is still ALARMED
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm->set_critical();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
  ASSERT_EQ(AlarmState::ALARMED, _alarm->get_alarm_state());

  // Clear alarm, and assert it is now CLEARED
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm->clear();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
  ASSERT_EQ(AlarmState::CLEARED, _alarm->get_alarm_state());
}

// Raises an Alarm in its possible states and then clears it. We
// expect three ZMQ messages to be sent to the Alarm Agent notifying it of each
// state change.
TEST_F(AlarmTest, AlarmClearing)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.2"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.6"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.5"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.3"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }

  _alarm->set(AlarmDef::INDETERMINATE);
  _alarm->set(AlarmDef::WARNING);
  _alarm->set(AlarmDef::MINOR);
  _alarm->set(AlarmDef::MAJOR);
  _alarm->set(AlarmDef::CRITICAL);
  _alarm->set(AlarmDef::CLEARED);
  
  // Causes the test thread to wait until we receive three zmq_recv messages (to
  // satisfy the expect_call above). We wait for a maximum of five seconds for
  // each message.
  for (unsigned int i = 0; i < 6; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

// Raises an Alarm and then simulates time moving forward by thirty seconds.
// We should expect two ZMQ messages: the first caused by the
// alarm being raised, the second caused by the function to re-send
// alarms every 30 seconds. This function reraises the alarm which was raised.
TEST_F(AlarmTest, ResendingAlarm)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    // We should expect the alarm we raised above to be reraised.
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }

  _alarm->set_major();

  // Simulate 30 seconds of time passing to trigger the alarms being re-raised.
  cwtest_advance_time_ms(30000);
  _alarm_manager->alarm_re_raiser()->_condition->signal();

  // Causes the test thread to wait until we receive two zmq_recv messages (to
  // satisfy the expect_call above). We wait for a maximum of five seconds for
  // each message.
  for (unsigned int i = 0; i < 2; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

// Raises an Alarm, clears it and then simulates time moving forward by thirty
// seconds. We should expect three ZMQ messages: the first two caused by the
// alarm being raised and cleared respectively, the third caused by the
// function to re-send alarms every 30 seconds.
TEST_F(AlarmTest, ResendingClearedAlarm)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));

    // We should expect the alarm we cleared above to be recleared.
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.1"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
  }

  _alarm->set_major();
  _alarm->clear();

  // Simulate 30 seconds of time passing to trigger the alarms being re-raised.
  cwtest_advance_time_ms(30000);
  _alarm_manager->alarm_re_raiser()->_condition->signal();

  // Causes the test thread to wait until we receive three zmq_recv messages (to
  // satisfy the expect_call above). We wait for a maximum of five seconds for
  // each message.
  for (unsigned int i = 0; i < 3; i++)
  {
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }
}

// Raises an Alarm and then raises it again, we should only expect
// one ZMQ message as the second would be a repeat.
TEST_F(AlarmTest, ResendingAlarmRepeatedSeverity)
{
  {
    InSequence s;
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("issue-alarm"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr(issuer),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_send(_,VoidPointeeEqualsStr("9998.4"),_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(_mz, zmq_recv(_,_,_,_)).Times(1).WillOnce(Return(0));
    
  }
  // Raises an alarm twice with only one possible raised state.
  _alarm->set_major();
  _alarm->set_major();
  
  // Causes the test thread to wait until we receive a zmq_recv message (to
  // satisfy the expect_call above). We wait for a maximum of five seconds for
  // the message.
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
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
  _alarm_state->issue();
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

  _alarm->set_major();
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
}

// Tests that the first time an alarm is set the right zmq messages are sent,
// but that subsequent settings send no zmq messages.
// The 'Times(3)' on the first call is to cover the three separate messages
// seen in the tests above: 'issue-alarm', 'issuer', and 'index/severity'.
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

    _alarm->set_major();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }

  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).Times(0);
  _alarm->set_major();
}

// Tests that the first time an alarm is cleared the right zmq messages are sent,
// but that subsequent clearings send no zmq messages.
TEST_F(AlarmTest, PairClearNotAlarmed)
{
  {
    InSequence s;

    EXPECT_CALL(_mz, zmq_send(_,_,_,_))
      .Times(3)
      .WillRepeatedly(Return(0));

    EXPECT_CALL(_mz, zmq_recv(_,_,_,_))
      .Times(1)
      .WillOnce(Return(0));

    _alarm->clear();
    _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  }

  EXPECT_CALL(_mz, zmq_send(_,_,_,_)).Times(0);
  _alarm->clear();
}

//Tests that clearing a set alarm sends the right zmq messages.
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

    _alarm->set_major();
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

    _alarm->clear();
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

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;

  EXPECT_TRUE(log.contains("zmq_ctx_new failed"));
}

TEST_F(AlarmZmqErrorTest, CreateSocket)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)) .WillOnce(ReturnNull());
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;

  EXPECT_TRUE(log.contains("zmq_socket failed"));
}

TEST_F(AlarmZmqErrorTest, SetSockOpt)
{
  CapturingTestLogger log;

  EXPECT_CALL(_mz, zmq_ctx_new()).WillOnce(Return(&_c));
  EXPECT_CALL(_mz, zmq_socket(_,_)).WillOnce(Return(&_s));
  EXPECT_CALL(_mz, zmq_setsockopt(_,_,_,_)).WillOnce(Return(-1));
  EXPECT_CALL(_mz, zmq_ctx_destroy(_)).WillOnce(Return(0));

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;

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

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;

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

  AlarmManager* alarm_manager = new AlarmManager();
  AlarmState alarm_state(alarm_manager->alarm_req_agent(), issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL);
  alarm_state.issue();
  _mz.call_complete(ZmqInterface::ZMQ_SEND, 5);
  delete alarm_manager; alarm_manager = NULL;

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

  AlarmManager* alarm_manager = new AlarmManager();
  AlarmState alarm_state(alarm_manager->alarm_req_agent(), issuer, AlarmDef::CPP_COMMON_FAKE_ALARM1, AlarmDef::CRITICAL);
  alarm_state.issue();
  _mz.call_complete(ZmqInterface::ZMQ_RECV, 5);
  delete alarm_manager; alarm_manager = NULL;

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

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;
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

  AlarmManager* alarm_manager = new AlarmManager();
  delete alarm_manager; alarm_manager = NULL;
  EXPECT_TRUE(log.contains("zmq_ctx_destroy failed"));
}

