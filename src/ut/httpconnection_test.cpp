/**
 * @file httpconnection_test.cpp UT for Sprout HttpConnection.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
using ::testing::Return;
using ::testing::StrEq;

/// Fixture for test.
class HttpConnectionTest : public BaseTest
{
  LoadMonitor _lm;
  FakeHttpResolver _resolver;
  AlarmManager* _am = new AlarmManager();
  NiceMock<MockCommunicationMonitor>* _cm = new NiceMock<MockCommunicationMonitor>(_am);
  HttpConnection* _http;
  HttpConnectionTest() :
    _lm(100000, 20, 10, 10),
    _resolver("10.42.42.42")
  {
    _http = new HttpConnection("cyrus",
                               true,
                               &_resolver,
                               &SNMP::FAKE_IP_COUNT_TABLE,
                               &_lm,
                               SASEvent::HttpLogLevel::PROTOCOL,
                               _cm);

    fakecurl_responses.clear();
    fakecurl_responses["http://10.42.42.42:80/blah/blah/blah"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";
    fakecurl_responses["http://10.42.42.42:80/blah/blah/wot"] = CURLE_REMOTE_FILE_NOT_FOUND;
    fakecurl_responses["http://10.42.42.42:80/blah/blah/503"] = 503;
    fakecurl_responses["http://10.42.42.42:80/blah/blah/recv_error"] = CURLE_RECV_ERROR;
    fakecurl_responses["http://10.42.42.42:80/up/up/up"] = "<message>ok, whatever...</message>";
    fakecurl_responses["http://10.42.42.42:80/up/up/down"] = CURLE_REMOTE_ACCESS_DENIED;
    fakecurl_responses["http://10.42.42.42:80/down/down/down"] = "<message>WHOOOOSH!!</message>";
    fakecurl_responses["http://10.42.42.42:80/down/down/up"] = CURLE_RECV_ERROR;
    fakecurl_responses["http://10.42.42.42:80/down/around"] = Response(CURLE_SEND_ERROR, "<message>Gotcha!</message>");
    fakecurl_responses["http://10.42.42.42:80/delete_id"] = CURLE_OK;
    fakecurl_responses["http://10.42.42.42:80/put_id"] = CURLE_OK;
    fakecurl_responses["http://10.42.42.42:80/put_id_response"] = Response({"response"});
    fakecurl_responses["http://10.42.42.42:80/post_id"] = Response({"Location: test"});
    fakecurl_responses["http://10.42.42.42:80/path"] = CURLE_OK;
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

/// Fixture for blacklist test.
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
    fakecurl_responses["http://3.0.0.0:80/http_success"] = "<message>success</message>";

    fakecurl_responses["http://3.0.0.0:80/tcp_success"] = CURLE_REMOTE_FILE_NOT_FOUND;

    fakecurl_responses["http://3.0.0.0:80/one_failure"] = CURLE_COULDNT_RESOLVE_HOST;
    fakecurl_responses["http://3.0.0.1:80/one_failure"] = "<message>success</message>";

    fakecurl_responses["http://3.0.0.0:80/all_failure"] = CURLE_COULDNT_RESOLVE_HOST;
    fakecurl_responses["http://3.0.0.1:80/all_failure"] = CURLE_COULDNT_RESOLVE_HOST;

    std::list<std::string> retry_after_header;
    retry_after_header.push_back("Retry-After: 30");
    fakecurl_responses["http://3.0.0.0:80/one_503_failure"] = Response(503, retry_after_header);
    fakecurl_responses["http://3.0.0.1:80/one_503_failure"] = "<message>success</message>";

    std::list<std::string> date_retry_after_header;
    date_retry_after_header.push_back("Retry-After: Fri, 07 Nov 2014 23:59:59 GMT");
    fakecurl_responses["http://3.0.0.0:80/one_date_503_failure"] = Response(503, date_retry_after_header);
    fakecurl_responses["http://3.0.0.1:80/one_date_503_failure"] = "<message>success</message>";

    fakecurl_responses["http://3.0.0.0:80/one_503_failure_no_retry_after"] = 503;
    fakecurl_responses["http://3.0.0.1:80/one_503_failure_no_retry_after"] = "<message>success</message>";
  }

  ~HttpConnectionBlacklistTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }

  /// Creates a vector of count AddrInfo targets, beginning from 3.0.0.0 and
  /// incrementing by one each time.
  std::vector<AddrInfo> create_targets(int count)
  {
    std::vector<AddrInfo> targets;
    AddrInfo ai;
    ai.port = 80;
    ai.transport = IPPROTO_TCP;
    std::stringstream os;
    for (int i = 0; i < count; ++i)
    {
      os << "3.0.0." << i;
      BaseResolver::parse_ip_target(os.str(), ai.address);
      targets.push_back(ai);
      os.str(std::string());
    }
    return targets;
  }
};

TEST_F(HttpConnectionBlacklistTest, BlacklistTestHttpSuccess)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  string output;
  _http->send_get("/http_success", output, "", 0);
}

TEST_F(HttpConnectionBlacklistTest, BlacklistTestTcpSuccess)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  string output;
  _http->send_get("/tcp_success", output, "", 0);
}

TEST_F(HttpConnectionBlacklistTest, BlacklistTestOneFailure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  string output;
  _http->send_get("/one_failure", output, "", 0);
}

TEST_F(HttpConnectionBlacklistTest, BlacklistTestOne503Failure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0], 30)).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  string output;
  _http->send_get("/one_503_failure", output, "", 0);
}

// Note that the current impementation ignores the date in a Retry-After header
TEST_F(HttpConnectionBlacklistTest, BlacklistTestOneDate503Failure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  string output;
  _http->send_get("/one_date_503_failure", output, "", 0);
}
//
// Note that the current impementation ignores the date in a Retry-After header
TEST_F(HttpConnectionBlacklistTest, BlacklistTestOne503FailureNoRetryAfter)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  string output;
  _http->send_get("/one_503_failure_no_retry_after", output, "", 0);
}

TEST_F(HttpConnectionBlacklistTest, BlacklistTestAllFailure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);
  EXPECT_CALL(_resolver, blacklist(targets[1])).Times(1);

  string output;
  _http->send_get("/all_failure", output, "", 0);
}

TEST_F(HttpConnectionTest, SimpleKeyAuthGet)
{
  string output;
  long ret = _http->send_get("/blah/blah/blah", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

TEST_F(HttpConnectionTest, GetWithHeadersAndUsername)
{
  std::map<std::string, std::string> headers;
  string output;
  std::vector<std::string> headers_to_add;
  long ret = _http->send_get("/blah/blah/blah", headers, output, "gandalf", headers_to_add, 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

TEST_F(HttpConnectionTest, SimpleIPv6Get)
{
  string output;
  FakeHttpResolver _resolver("1::1");
  HttpConnection http2("[1::1]:80",
                       true,
                       &_resolver,
                       &SNMP::FAKE_IP_COUNT_TABLE,
                       &_lm,
                       SASEvent::HttpLogLevel::PROTOCOL,
                       _cm);

  fakecurl_responses["http://[1::1]:80/ipv6get"] = CURLE_OK;
  long ret = http2.send_get("/ipv6get", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimpleGetFailure)
{
  EXPECT_CALL(*_cm, inform_failure(_)).Times(2);

  string output;
  long ret = _http->send_get("/blah/blah/wot", output, "gandalf", 0);

  EXPECT_EQ(404, ret);

  ret = _http->send_get("/blah/blah/503", output, "gandalf", 0);

  EXPECT_EQ(503, ret);
}

TEST_F(HttpConnectionTest, SimpleGetRetry)
{
  string output;

  // Warm up the connection.
  long ret = _http->send_get("/blah/blah/blah", output, "gandalf", 0);

  EXPECT_EQ(200, ret);

  // Get a failure on the connection and retry it.
  ret = _http->send_get("/down/around", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<message>Gotcha!</message>", output);
}

TEST_F(HttpConnectionTest, GetWithUsername)
{
  string output;
  std::map<std::string, std::string> headers_in_rsp;

  long ret = _http->send_get("/path", headers_in_rsp, output, "username", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, ReceiveError)
{
  EXPECT_CALL(*_cm, inform_failure(_));

  string output;
  long ret = _http->send_get("/blah/blah/recv_error", output, "gandalf", 0);

  EXPECT_EQ(500, ret);
}

TEST_F(HttpConnectionTest, SimplePost)
{
  std::map<std::string, std::string> head;
  std::string response;

  long ret = _http->send_post("/post_id", head, response, "", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimplePut)
{
  EXPECT_CALL(*_cm, inform_success(_));

  long ret = _http->send_put("/put_id", "", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SimplePutWithResponse)
{
  EXPECT_CALL(*_cm, inform_success(_));

  std::string response;
  long ret = _http->send_put("/put_id_response", response, "", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("response", response);
}

TEST_F(HttpConnectionTest, PutWithHeadersAndUsername)
{
  EXPECT_CALL(*_cm, inform_success(_));

  std::map<std::string, std::string> headers;
  std::string response;
  std::vector<std::string> extra_req_headers;
  long ret = _http->send_put("/put_id_response", headers, response, "", extra_req_headers, 0, "");

  EXPECT_EQ(200, ret);
  EXPECT_EQ("response", response);
}

TEST_F(HttpConnectionTest, SimpleDelete)
{
  long ret = _http->send_delete("/delete_id", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, DeleteBody)
{
  long ret = _http->send_delete("/delete_id", 0, "body");

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, DeleteBodyWithResponse)
{
  std::string response;
  long ret = _http->send_delete("/delete_id", 0, "body", response);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, DeleteBodyWithHeadersAndUsername)
{
  std::map<std::string, std::string> headers;
  std::string response;
  long ret = _http->send_delete("/delete_id", headers, response, 0, "body", "gandalf");

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, SASCorrelationHeader)
{
  mock_sas_collect_messages(true);

  std::string uuid;
  std::string output;

  _http->send_get("/blah/blah/blah", output, "gandalf", 0);

  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];

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
  // Test port-parsing by adding a port
  HttpConnection http2("cyrus:1234",
                       true,
                       &_resolver,
                       &SNMP::FAKE_IP_COUNT_TABLE,
                       &_lm,
                       SASEvent::HttpLogLevel::PROTOCOL,
                       _cm);
  fakecurl_responses["http://10.42.42.42:1234/port-1234"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";

  string output;
  long ret = http2.send_get("/port-1234", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);
}

TEST_F(HttpConnectionTest, ParseHostPortIPv6)
{
  // Test parsing with an IPv6 address
  HttpConnection http2("[1::1]",
                       true,
                       &_resolver,
                       &SNMP::FAKE_IP_COUNT_TABLE,
                       &_lm,
                       SASEvent::HttpLogLevel::PROTOCOL,
                       _cm);

  string output;
  fakecurl_responses["http://[1::1]:80/ipv6get"] = CURLE_OK;
  long ret = http2.send_get("/ipv6get", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpConnectionTest, BasicResolverTest)
{
  // Just check the resolver constructs/destroys correctly.
  HttpResolver resolver(NULL, AF_INET);
}
