/**
 * @file diameterstack_test.cpp UT for Homestead diameterstack module.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <time.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils.hpp"
#include "test_interposer.hpp"
#include "mockfreediameter.hpp"
#include "mockcommunicationmonitor.h"

#include "diameterstack.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::WithArgs;

class UTDictionary : public Diameter::Dictionary
{
public:
  UTDictionary() :
    TGPP("3GPP"),
    TGPP2("3GPP2"),
    CX("Cx"),
    USER_AUTHORIZATION_REQUEST("3GPP/User-Authorization-Request"),
    USER_AUTHORIZATION_ANSWER("3GPP/User-Authorization-Answer"),
    LOCATION_INFO_REQUEST("3GPP/Location-Info-Request"),
    LOCATION_INFO_ANSWER("3GPP/Location-Info-Answer"),
    MULTIMEDIA_AUTH_REQUEST("3GPP/Multimedia-Auth-Request"),
    MULTIMEDIA_AUTH_ANSWER("3GPP/Multimedia-Auth-Answer"),
    SERVER_ASSIGNMENT_REQUEST("3GPP/Server-Assignment-Request"),
    SERVER_ASSIGNMENT_ANSWER("3GPP/Server-Assignment-Answer"),
    REGISTRATION_TERMINATION_REQUEST("3GPP/Registration-Termination-Request"),
    REGISTRATION_TERMINATION_ANSWER("3GPP/Registration-Termination-Answer"),
    PUSH_PROFILE_REQUEST("3GPP/Push-Profile-Request"),
    PUSH_PROFILE_ANSWER("3GPP/Push-Profile-Answer"),
    PUBLIC_IDENTITY("3GPP", "Public-Identity"),
    SIP_AUTH_DATA_ITEM("3GPP", "SIP-Auth-Data-Item"),
    SIP_AUTH_SCHEME("3GPP", "SIP-Authentication-Scheme"),
    SIP_AUTHORIZATION("3GPP", "SIP-Authorization"),
    SIP_NUMBER_AUTH_ITEMS("3GPP", "SIP-Number-Auth-Items"),
    SERVER_NAME("3GPP", "Server-Name"),
    SIP_DIGEST_AUTHENTICATE("3GPP", "SIP-Digest-Authenticate"),
    CX_DIGEST_HA1("3GPP", "Digest-HA1"),
    CX_DIGEST_REALM("3GPP", "Digest-Realm"),
    VISITED_NETWORK_IDENTIFIER("3GPP", "Visited-Network-Identifier"),
    SERVER_CAPABILITIES("3GPP", "Server-Capabilities"),
    MANDATORY_CAPABILITY("3GPP", "Mandatory-Capability"),
    OPTIONAL_CAPABILITY("3GPP", "Optional-Capability"),
    SERVER_ASSIGNMENT_TYPE("3GPP", "Server-Assignment-Type"),
    USER_AUTHORIZATION_TYPE("3GPP", "User-Authorization-Type"),
    ORIGINATING_REQUEST("3GPP", "Originating-Request"),
    USER_DATA_ALREADY_AVAILABLE("3GPP", "User-Data-Already-Available"),
    USER_DATA("3GPP", "User-Data"),
    CX_DIGEST_QOP("3GPP", "Digest-QoP"),
    SIP_AUTHENTICATE("3GPP", "SIP-Authenticate"),
    CONFIDENTIALITY_KEY("3GPP", "Confidentiality-Key"),
    INTEGRITY_KEY("3GPP", "Integrity-Key"),
    ASSOCIATED_IDENTITIES("3GPP", "Associated-Identities"),
    DEREGISTRATION_REASON("3GPP", "Deregistration-Reason"),
    REASON_CODE("3GPP", "Reason-Code"),
    IDENTITY_WITH_EMERGENCY_REGISTRATION("3GPP", "Identity-with-Emergency-Registration"),
    CHARGING_INFORMATION("3GPP", "Charging-Information"),
    PRIMARY_CHARGING_COLLECTION_FUNCTION_NAME("3GPP", "Primary-Charging-Collection-Function-Name"),
    SECONDARY_CHARGING_COLLECTION_FUNCTION_NAME("3GPP", "Secondary-Charging-Collection-Function-Name"),
    PRIMARY_EVENT_CHARGING_FUNCTION_NAME("3GPP", "Primary-Event-Charging-Function-Name"),
    SECONDARY_EVENT_CHARGING_FUNCTION_NAME("3GPP", "Secondary-Event-Charging-Function-Name")
  {
  }

  const Diameter::Dictionary::Vendor TGPP;
  const Diameter::Dictionary::Vendor TGPP2;
  const Diameter::Dictionary::Application CX;
  const Diameter::Dictionary::Message USER_AUTHORIZATION_REQUEST;
  const Diameter::Dictionary::Message USER_AUTHORIZATION_ANSWER;
  const Diameter::Dictionary::Message LOCATION_INFO_REQUEST;
  const Diameter::Dictionary::Message LOCATION_INFO_ANSWER;
  const Diameter::Dictionary::Message MULTIMEDIA_AUTH_REQUEST;
  const Diameter::Dictionary::Message MULTIMEDIA_AUTH_ANSWER;
  const Diameter::Dictionary::Message SERVER_ASSIGNMENT_REQUEST;
  const Diameter::Dictionary::Message SERVER_ASSIGNMENT_ANSWER;
  const Diameter::Dictionary::Message REGISTRATION_TERMINATION_REQUEST;
  const Diameter::Dictionary::Message REGISTRATION_TERMINATION_ANSWER;
  const Diameter::Dictionary::Message PUSH_PROFILE_REQUEST;
  const Diameter::Dictionary::Message PUSH_PROFILE_ANSWER;
  const Diameter::Dictionary::AVP PUBLIC_IDENTITY;
  const Diameter::Dictionary::AVP SIP_AUTH_DATA_ITEM;
  const Diameter::Dictionary::AVP SIP_AUTH_SCHEME;
  const Diameter::Dictionary::AVP SIP_AUTHORIZATION;
  const Diameter::Dictionary::AVP SIP_NUMBER_AUTH_ITEMS;
  const Diameter::Dictionary::AVP SERVER_NAME;
  const Diameter::Dictionary::AVP SIP_DIGEST_AUTHENTICATE;
  const Diameter::Dictionary::AVP CX_DIGEST_HA1;
  const Diameter::Dictionary::AVP CX_DIGEST_REALM;
  const Diameter::Dictionary::AVP VISITED_NETWORK_IDENTIFIER;
  const Diameter::Dictionary::AVP SERVER_CAPABILITIES;
  const Diameter::Dictionary::AVP MANDATORY_CAPABILITY;
  const Diameter::Dictionary::AVP OPTIONAL_CAPABILITY;
  const Diameter::Dictionary::AVP SERVER_ASSIGNMENT_TYPE;
  const Diameter::Dictionary::AVP USER_AUTHORIZATION_TYPE;
  const Diameter::Dictionary::AVP ORIGINATING_REQUEST;
  const Diameter::Dictionary::AVP USER_DATA_ALREADY_AVAILABLE;
  const Diameter::Dictionary::AVP USER_DATA;
  const Diameter::Dictionary::AVP CX_DIGEST_QOP;
  const Diameter::Dictionary::AVP SIP_AUTHENTICATE;
  const Diameter::Dictionary::AVP CONFIDENTIALITY_KEY;
  const Diameter::Dictionary::AVP INTEGRITY_KEY;
  const Diameter::Dictionary::AVP ASSOCIATED_IDENTITIES;
  const Diameter::Dictionary::AVP DEREGISTRATION_REASON;
  const Diameter::Dictionary::AVP REASON_CODE;
  const Diameter::Dictionary::AVP IDENTITY_WITH_EMERGENCY_REGISTRATION;
  const Diameter::Dictionary::AVP CHARGING_INFORMATION;
  const Diameter::Dictionary::AVP PRIMARY_CHARGING_COLLECTION_FUNCTION_NAME;
  const Diameter::Dictionary::AVP SECONDARY_CHARGING_COLLECTION_FUNCTION_NAME;
  const Diameter::Dictionary::AVP PRIMARY_EVENT_CHARGING_FUNCTION_NAME;
  const Diameter::Dictionary::AVP SECONDARY_EVENT_CHARGING_FUNCTION_NAME;
};

class DiameterTestTransaction : public Diameter::Transaction
{
public:
  DiameterTestTransaction(Diameter::Dictionary* dict) :
    Diameter::Transaction(dict, 0)
  {}

  virtual ~DiameterTestTransaction() {}

  void check_latency(unsigned long expected_latency_us)
  {
    unsigned long actual_latency_us;
    bool rc;

    rc = get_duration(actual_latency_us);
    EXPECT_TRUE(rc);
    EXPECT_EQ(expected_latency_us, actual_latency_us);

    cwtest_advance_time_ms(1);

    rc = get_duration(actual_latency_us);
    EXPECT_TRUE(rc);
    EXPECT_EQ(expected_latency_us, actual_latency_us);
  }

  MOCK_METHOD1(on_response, void(Diameter::Message& rsp));
  MOCK_METHOD0(on_timeout, void());
};

class DiameterRequestTest : public ::testing::Test
{
public:
  DiameterRequestTest()
  {
    _stack = Diameter::Stack::get_instance();
    _stack->initialize();
    _stack->configure(UT_DIR + "/diameterstack.conf", NULL);

    _dict = new UTDictionary();

    cwtest_completely_control_time();

    // Mock out free diameter.  By default mock out all attempts to create new
    // messages, read data out of them, or bufferize them.
    mock_free_diameter(&_mock_fd);

    EXPECT_CALL(_mock_fd, fd_msg_new(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>((struct msg*)NULL), Return(0)));

    _mock_fd.hdr.msg_code = 123;
    EXPECT_CALL(_mock_fd, fd_msg_hdr(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&_mock_fd.hdr), Return(0)));

    EXPECT_CALL(_mock_fd, fd_msg_bufferize(_, _, _))
      .WillRepeatedly(DoAll(WithArgs<1, 2>(Invoke(create_dummy_diameter_buffer)),
                            Return(0)));

    EXPECT_CALL(_mock_fd, fd_hook_get_pmd(_, _))
      .WillRepeatedly(Return(&_mock_per_msg_data));
  }

  virtual ~DiameterRequestTest()
  {
    unmock_free_diameter();
    cwtest_reset_time();

    delete _dict; _dict = NULL;

    _stack->stop();
    _stack->wait_stopped();
  }

  DiameterTestTransaction* make_trx()
  {
    return new DiameterTestTransaction(_dict);
  }

  static void create_dummy_diameter_buffer(uint8_t **buffer, size_t *len)
  {
    char* str = strdup("A fake diameter message");
    *len = strlen(str);
    *buffer = (uint8_t*)str;
  }

private:
  Diameter::Stack* _stack;
  UTDictionary* _dict;
  MockFreeDiameter _mock_fd;
  fd_hook_permsgdata _mock_per_msg_data;
};


class DiameterRequestCommMonMockTest : public ::testing::Test
{
public:
  DiameterRequestCommMonMockTest()
  {
    _am = new AlarmManager();
    _cm = new MockCommunicationMonitor(_am);
    _stack = Diameter::Stack::get_instance();
    _stack->initialize();
    _stack->configure(UT_DIR + "/diameterstack.conf", NULL, _cm);

    _dict = new UTDictionary();
  }

  virtual ~DiameterRequestCommMonMockTest()
  {
    delete _dict; _dict = NULL;

    _stack->stop();
    _stack->wait_stopped();

    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }

  DiameterTestTransaction* make_trx()
  {
    return new DiameterTestTransaction(_dict);
  }

private:
  Diameter::Stack* _stack;
  UTDictionary* _dict;
  AlarmManager* _am;
  MockCommunicationMonitor* _cm;
};


TEST(DiameterStackTest, SimpleMainline)
{
  Diameter::Stack* stack = Diameter::Stack::get_instance();
  stack->initialize();
  stack->configure(UT_DIR + "/diameterstack.conf", NULL);
  stack->stop();
  stack->wait_stopped();
}

TEST(DiameterStackTest, AdvertizeApplication)
{
  Diameter::Stack* stack = Diameter::Stack::get_instance();
  stack->initialize();
  stack->configure(UT_DIR + "/diameterstack.conf", NULL);
  Diameter::Dictionary::Application app("Cx");
  stack->advertize_application(Diameter::Dictionary::Application::AUTH, app);
  stack->stop();
  stack->wait_stopped();
}

ACTION_P(AdvanceTimeMs, ms) { cwtest_advance_time_ms(ms); }
ACTION_P2(CheckLatency, trx, ms) { trx->check_latency(ms * 1000); }

TEST_F(DiameterRequestTest, NormalRequestTimesLatency)
{
  Diameter::Message req(_dict, _dict->MULTIMEDIA_AUTH_REQUEST, _stack);
  struct msg *fd_rsp = NULL;
  DiameterTestTransaction *trx = make_trx();

  EXPECT_CALL(_mock_fd, fd_msg_send(_, _, _)).WillOnce(Return(0));
  req.send(trx);

  cwtest_advance_time_ms(12);

  EXPECT_CALL(*trx, on_response(_)).WillOnce(CheckLatency(trx, 12));
  Diameter::Transaction::on_response(trx, &fd_rsp); trx = NULL;
}


TEST_F(DiameterRequestTest, TimedoutRequestTimesLatency)
{
  Diameter::Message req(_dict, _dict->MULTIMEDIA_AUTH_REQUEST, _stack);
  struct msg *fd_rsp = NULL;
  DiameterTestTransaction *trx = make_trx();

  EXPECT_CALL(_mock_fd, fd_msg_send_timeout(_, _, _, _, _)).WillOnce(Return(0));
  req.send(trx, 1000);

  cwtest_advance_time_ms(15);

  EXPECT_CALL(*trx, on_timeout()).WillOnce(CheckLatency(trx, 15));
  Diameter::Transaction::on_timeout(trx,
                                    (DiamId_t)"DiameterIdentity",
                                    0,
                                    &fd_rsp);
  trx = NULL;
}


TEST_F(DiameterRequestCommMonMockTest, ResponseOk)
{
  DiameterTestTransaction *trx = make_trx();

  Diameter::Message rsp(_dict, _dict->MULTIMEDIA_AUTH_ANSWER, _stack);
  rsp.revoke_ownership();
  rsp.set_result_code("DIAMETER_SUCCESS");

  struct msg* fd_rsp = rsp.fd_msg();

  EXPECT_CALL(*trx, on_response(_));
  EXPECT_CALL(*_cm, inform_success(_));
  Diameter::Transaction::on_response(trx, &fd_rsp); trx = NULL;
}


TEST_F(DiameterRequestCommMonMockTest, ResponseError)
{
  DiameterTestTransaction *trx = make_trx();

  Diameter::Message rsp(_dict, _dict->MULTIMEDIA_AUTH_ANSWER, _stack);
  rsp.revoke_ownership();
  rsp.set_result_code("DIAMETER_UNABLE_TO_DELIVER");

  struct msg* fd_rsp = rsp.fd_msg();

  EXPECT_CALL(*trx, on_response(_));
  EXPECT_CALL(*_cm, inform_failure(_));
  Diameter::Transaction::on_response(trx, &fd_rsp); trx = NULL;
}
