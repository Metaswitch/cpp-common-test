/**
 * @file httpconnection_test.cpp UT for Sprout HttpConnection.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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
#include <sstream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <boost/algorithm/string.hpp>

#include "utils.h"
#include "sas.h"
#include "fakehttpresolver.hpp"
#include "mockhttpresolver.h"
#include "httpconnection.h"
#include "basetest.hpp"
#include "test_utils.hpp"
#include "load_monitor.h"
#include "mock_sas.h"
#include "mockcommunicationmonitor.h"
#include "fakesnmp.hpp"
#include "curl_interposer.hpp"
#include "fakecurl.hpp"
#include "test_interposer.hpp"

using namespace std;
using ::testing::MatchesRegex;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

// Fixture for tests using fake resolver
class HttpConnectionTest : public BaseTest
{
  LoadMonitor _lm;
  FakeHttpResolver _resolver;
  AlarmManager* _am = new AlarmManager();
  NiceMock<MockCommunicationMonitor>*_cm = new NiceMock<MockCommunicationMonitor>(_am);
  HttpConnection* _http;

  HttpConnectionTest() :
    _lm(100000, 20, 10, 10)
  {
    _http = new HttpConnection("cyrus",
                               true,
                               &_resolver,
                               &SNMP::FAKE_IP_COUNT_TABLE,
                               &_lm,
                               SASEvent::HttpLogLevel::PROTOCOL,
                               _cm);
    fakecurl_responses.clear();
  }

  virtual ~HttpConnectionTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }
};

// Fixture for blacklist tests using mock resolver
class HttpConnectionBlacklistTest : public BaseTest
{
  StrictMock<MockHttpResolver> _resolver;
  HttpConnection* _http;
  LoadMonitor _lm;
  AlarmManager* _am = new AlarmManager();
  NiceMock<MockCommunicationMonitor>*_cm = new NiceMock<MockCommunicationMonitor>(_am);

  HttpConnectionBlacklistTest() :
    _lm(100000, 20, 10, 10)
  {
    _http = new HttpConnection("cyrus",
                               true,
                               &_resolver,
                               &SNMP::FAKE_IP_COUNT_TABLE,
                               &_lm,
                               SASEvent::HttpLogLevel::PROTOCOL,
                               _cm);
    fakecurl_responses.clear();
  }

  ~HttpConnectionBlacklistTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }
};

// Test that a success is reported to the resolver
TEST_F(HttpConnectionBlacklistTest, BlacklistTestHttpSuccess)
{
  std::vector<AddrInfo> targets = MockHttpResolver::create_targets(2);
  _resolver._targets = targets;

  fakecurl_responses["http://3.0.0.0:80/http_success"] = "<message>success</message>";
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  string output;
  _http->send_get("/http_success", output, "", 0);
}

// Test that a success is reported to the resolver if the connection is
// successful, but the request fails at the HTTP level
TEST_F(HttpConnectionBlacklistTest, BlacklistTestTcpSuccess)
{
  std::vector<AddrInfo> targets = MockHttpResolver::create_targets(2);
  _resolver._targets = targets;

  fakecurl_responses["http://3.0.0.0:80/tcp_success"] = CURLE_REMOTE_FILE_NOT_FOUND;
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  string output;
  _http->send_get("/tcp_success", output, "", 0);
}

// Test that a single failure is reported to the resolver, and a following
// success is also reported
TEST_F(HttpConnectionBlacklistTest, BlacklistTestOneFailure)
{
  std::vector<AddrInfo> targets = MockHttpResolver::create_targets(2);
  _resolver._targets = targets;

  // The first request should fail
  fakecurl_responses["http://3.0.0.0:80/one_failure"] = CURLE_COULDNT_RESOLVE_HOST;
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);

  // The second request should succeed
  fakecurl_responses["http://3.0.0.1:80/one_failure"] = "<message>success</message>";
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  string output;
  _http->send_get("/one_failure", output, "", 0);
}

// Test that multiple failures to connect are reported to the resolver
TEST_F(HttpConnectionBlacklistTest, BlacklistTestAllFailure)
{
  std::vector<AddrInfo> targets = MockHttpResolver::create_targets(2);
  _resolver._targets = targets;

  // Both requests should fail
  fakecurl_responses["http://3.0.0.0:80/all_failure"] = CURLE_COULDNT_RESOLVE_HOST;
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);
  fakecurl_responses["http://3.0.0.1:80/all_failure"] = CURLE_COULDNT_RESOLVE_HOST;
  EXPECT_CALL(_resolver, blacklist(targets[1])).Times(1);

  string output;
  _http->send_get("/all_failure", output, "", 0);
}

TEST_F(HttpConnectionTest, SimpleKeyAuthGet)
{
  _resolver._targets = MockHttpResolver::create_targets(1);
  string output;

  fakecurl_responses["http://3.0.0.0:80/doc"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  long ret = _http->send_get("/doc", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://3.0.0.0:80/doc"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

TEST_F(HttpConnectionTest, SimpleIPv6Get)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("1::1"));
  string output;

  fakecurl_responses["http://[1::1]:80/ipv6get"] = CURLE_OK;

  long ret = _http->send_get("/ipv6get", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimpleGetFailure)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  fakecurl_responses["http://3.0.0.0:80/file_not_found"] = CURLE_REMOTE_FILE_NOT_FOUND;
  fakecurl_responses["http://3.0.0.0:80/503"] = 503;

  string output;
  long ret = _http->send_get("/file_not_found", output, "gandalf", 0);

  EXPECT_EQ(404, ret);

  ret = _http->send_get("/503", output, "gandalf", 0);

  EXPECT_EQ(503, ret);
}

TEST_F(HttpConnectionTest, SimpleGetRetry)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  string output;

  // Warm up the connection.
  fakecurl_responses["http://3.0.0.0:80/warm"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  long ret = _http->send_get("/warm", output, "gandalf", 0);

  EXPECT_EQ(200, ret);

  // Get a failure on the connection and retry it.
  fakecurl_responses["http://3.0.0.0:80/get_retry"] = Response(CURLE_SEND_ERROR, "<message>Gotcha!</message>");

  ret = _http->send_get("/get_retry", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<message>Gotcha!</message>", output);
}

TEST_F(HttpConnectionTest, GetWithUsername)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  string output;
  std::map<std::string, std::string> headers_in_rsp;

  fakecurl_responses["http://3.0.0.0:80/path"] = CURLE_OK;

  long ret = _http->send_get("/path", headers_in_rsp, output, "username", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, ReceiveError)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  string output;

  fakecurl_responses["http://3.0.0.0:80/recv_error"] = CURLE_RECV_ERROR;

  long ret = _http->send_get("/recv_error", output, "gandalf", 0);

  EXPECT_EQ(500, ret);
}

TEST_F(HttpConnectionTest, SimplePost)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  std::map<std::string, std::string> head;
  std::string response;

  fakecurl_responses["http://3.0.0.0:80/post_id"] = Response({"Location: test"});

  long ret = _http->send_post("/post_id", head, response, "", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimplePut)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  fakecurl_responses["http://3.0.0.0:80/put_id"] = Response({"response"});

  long ret = _http->send_put("/put_id", "", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimplePutWithResponse)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  std::string response;

  fakecurl_responses["http://3.0.0.0:80/put_id_response"] = Response({"response"});

  long ret = _http->send_put("/put_id_response", response, "", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("response", response);
}

TEST_F(HttpConnectionTest, SimpleDelete)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  fakecurl_responses["http://3.0.0.0:80/delete_id"] = CURLE_OK;

  long ret = _http->send_delete("/delete_id", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, DeleteBody)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  fakecurl_responses["http://3.0.0.0:80/delete_id"] = CURLE_OK;

  long ret = _http->send_delete("/delete_id", 0, "body");

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, DeleteBodyWithResponse)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  std::string response;

  fakecurl_responses["http://3.0.0.0:80/delete_id"] = CURLE_OK;

  long ret = _http->send_delete("/delete_id", 0, "body", response);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SASCorrelationHeader)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  mock_sas_collect_messages(true);

  std::string uuid;
  std::string output;

  fakecurl_responses["http://3.0.0.0:80/doc"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  _http->send_get("/doc", output, "gandalf", 0);

  Request& req = fakecurl_requests["http://3.0.0.0:80/doc"];

  // The CURL request should contain an X-SAS-HTTP-Branch-ID whose value is a
  // UUID.
  bool found_header = false;
  for(std::list<std::string>::iterator it = req._headers.begin();
      it != req._headers.end();
      ++it)
  {
    if (boost::starts_with(*it, "X-SAS-HTTP-Branch-ID"))
    {
      EXPECT_THAT(*it, MatchesRegex(
        "^X-SAS-HTTP-Branch-ID: *[0-9a-fA-F]{8}-"
                                "[0-9a-fA-F]{4}-"
                                "[0-9a-fA-F]{4}-"
                                "[0-9a-fA-F]{4}-"
                                "[0-9a-fA-F]{12}$"));

      // This strips off the header name and leaves just the value.
      uuid = it->substr(std::string("X-SAS-HTTP-Branch-ID: ").length());
      found_header = true;
    }
  }

  EXPECT_TRUE(found_header);

  // Check that we logged a branch ID marker.
  MockSASMessage* marker = mock_sas_find_marker(MARKER_ID_VIA_BRANCH_PARAM);

  EXPECT_TRUE(marker != NULL);
  EXPECT_EQ(marker->var_params.size(), 1u);
  EXPECT_EQ(marker->var_params[0], uuid);

  mock_sas_collect_messages(false);
}

TEST_F(HttpConnectionTest, ParseHostPort)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  // Test port-parsing by adding a port
  HttpConnection http2("cyrus:1234",
                       true,
                       &_resolver,
                       &SNMP::FAKE_IP_COUNT_TABLE,
                       &_lm,
                       SASEvent::HttpLogLevel::PROTOCOL,
                       _cm);
  fakecurl_responses["http://3.0.0.0:1234/port-1234"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  string output;

  long ret = http2.send_get("/port-1234", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);
}

TEST_F(HttpConnectionTest, ParseHostPortIPv6)
{
  _resolver._targets.push_back(MockHttpResolver::create_target("3.0.0.0"));

  // Test parsing with an IPv6 address
  HttpConnection http2("[1::1]",
                       true,
                       &_resolver,
                       &SNMP::FAKE_IP_COUNT_TABLE,
                       &_lm,
                       SASEvent::HttpLogLevel::PROTOCOL,
                       _cm);

  string output;

  fakecurl_responses["http://3.0.0.0:80/doc"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  long ret = http2.send_get("/doc", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);
}

TEST_F(HttpConnectionTest, BasicResolverTest)
{
  // Just check the resolver constructs/destroys correctly.
  HttpResolver resolver(NULL, AF_INET);
}
