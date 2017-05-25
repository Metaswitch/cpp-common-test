/**
 * @file chronosconnection_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gtest/gtest.h"

#include "utils.h"
#include "sas.h"
#include "fakehttpresolver.hpp"
#include "chronosconnection.h"
#include "basetest.hpp"
#include "curl_interposer.hpp"
#include "fakecurl.hpp"

namespace AlarmDef {
  static const int CPP_COMMON_FAKE_ALARM = 9999;
}

using namespace std;

/// Fixture for ChronosConnectionTest.
class ChronosConnectionTest : public BaseTest
{
  FakeHttpResolver _resolver;
  AlarmManager* _alarm_manager;
  CommunicationMonitor* _cm;
  ChronosConnection* _chronos;

  ChronosConnectionTest() :
    _alarm_manager(new AlarmManager()),
    _cm(new CommunicationMonitor(new CMAlarmAdaptor(new Alarm(_alarm_manager, "sprout", AlarmDef::CPP_COMMON_FAKE_ALARM), AlarmDef::MAJOR, AlarmDef::MAJOR), "sprout", "chronos"))
  {
    _resolver._targets.push_back(FakeHttpResolver::create_target("10.42.42.42"));
    _chronos = new ChronosConnection("narcissus", "localhost:9888", &_resolver, _cm);
    fakecurl_responses.clear();
  }

  virtual ~ChronosConnectionTest()
  {
    fakecurl_responses.clear();
    delete _chronos; _chronos = NULL;
    delete _cm; _cm = NULL;
    delete _alarm_manager; _alarm_manager = NULL;
  }
};

TEST_F(ChronosConnectionTest, SendDelete)
{
  fakecurl_responses["http://10.42.42.42:80/timers/delete_id"] = CURLE_OK;
  HTTPCode status = _chronos->send_delete("delete_id",  0);
  EXPECT_EQ(status, 200);
}

TEST_F(ChronosConnectionTest, SendInvalidDelete)
{
  HTTPCode status = _chronos->send_delete("",  0);
  EXPECT_EQ(status, 405);
}

TEST_F(ChronosConnectionTest, SendPost)
{
  std::list<std::string> headers = {"Location: http://localhost:7253/timers/abcd"};
  fakecurl_responses["http://10.42.42.42:80/timers"] = Response(headers);

  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::string post_identity = "";
  HTTPCode status = _chronos->send_post(post_identity, 300, "/timers", opaque,  0);
  EXPECT_EQ(status, 200);
  EXPECT_EQ(post_identity, "abcd");
}

TEST_F(ChronosConnectionTest, SendPostWithTags)
{
  std::list<std::string> headers = {"Location: http://localhost:7253/timers/abcd"};
  fakecurl_responses["http://10.42.42.42:80/timers"] = Response(headers);

  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::map<std::string, uint32_t> tags = { {"TAG1", 1}, {"TAG2", 1} };
  std::string post_identity = "";
  HTTPCode status = _chronos->send_post(post_identity, 300, "/timers", opaque,  0, tags);
  EXPECT_EQ(status, 200);
  EXPECT_EQ(post_identity, "abcd");
}

TEST_F(ChronosConnectionTest, SendPostWithNoLocationHeader)
{
  std::list<std::string> headers = {"Header: header"};
  fakecurl_responses["http://10.42.42.42:80/timers"] = Response(headers);

  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::string post_identity = "";
  HTTPCode status = _chronos->send_post(post_identity, 300, "/timers", opaque,  0);
  EXPECT_EQ(status, 400);
  EXPECT_EQ(post_identity, "");
}

TEST_F(ChronosConnectionTest, SendPostWithNoHeaders)
{
  std::list<std::string> headers = {""};
  fakecurl_responses["http://10.42.42.42:80/timers"] = Response(headers);

  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::string post_identity = "";
  HTTPCode status = _chronos->send_post(post_identity, 300, "/timers", opaque,  0);
  EXPECT_EQ(status, 400);
  EXPECT_EQ(post_identity, "");
}

TEST_F(ChronosConnectionTest, SendPut)
{
  std::list<std::string> headers = {"Location: http://localhost:7253/timers/efgh"};
  fakecurl_responses["http://10.42.42.42:80/timers/abcd"] = Response(headers);

  // We expect Chronos to change the put identity to the value in the Location
  // header.
  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::string put_identity = "abcd";
  HTTPCode status = _chronos->send_put(put_identity, 300, "/timers", opaque,  0);
  EXPECT_EQ(status, 200);
  EXPECT_EQ(put_identity, "efgh");
}

TEST_F(ChronosConnectionTest, SendPutWithTags)
{
  std::list<std::string> headers = {"Location: http://localhost:7253/timers/efgh"};
  fakecurl_responses["http://10.42.42.42:80/timers/abcd"] = Response(headers);

  // We expect Chronos to change the put identity to the value in the Location
  // header.
  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::map<std::string, uint32_t> tags = { {"TAG1", 1}, {"TAG2", 1} };
  std::string put_identity = "abcd";
  HTTPCode status = _chronos->send_put(put_identity, 300, "/timers", opaque,  0, tags);
  EXPECT_EQ(status, 200);
  EXPECT_EQ(put_identity, "efgh");
}

TEST_F(ChronosConnectionTest, SendPutWithNoLocationHeader)
{
  std::list<std::string> headers = {"Header: header"};
  fakecurl_responses["http://10.42.42.42:80/timers/abcd"] = Response(headers);

  std::string opaque = "{\"aor_id\": \"aor_id\", \"binding_id\": \"binding_id\"}";
  std::string put_identity = "abcd";
  HTTPCode status = _chronos->send_put(put_identity, 300, "/timers", opaque,  0);
  EXPECT_EQ(status, 400);
  EXPECT_EQ(put_identity, "abcd");
}
