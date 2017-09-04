/**
 * @file httpclient_test.cpp UT for Sprout HttpClient.
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
#include "httpclient.h"
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

/// Fixture for test.
/// Note that most of HttpClient's function is tested through the
/// HttpConnection tests.
class HttpClientTest : public BaseTest
{
  FakeHttpResolver _resolver;
  AlarmManager* _am = new AlarmManager();
  NiceMock<MockCommunicationMonitor>* _cm = new NiceMock<MockCommunicationMonitor>(_am);
  HttpClient* _http;
  HttpClient* _private_http;
  HttpClientTest() :
    _resolver("10.42.42.42")
  {
    _http = new HttpClient(true,
                           &_resolver,
                           SASEvent::HttpLogLevel::PROTOCOL,
                           _cm);

    _private_http = new HttpClient(true,
                                   &_resolver,
                                   NULL,
                                   NULL,
                                   SASEvent::HttpLogLevel::PROTOCOL,
                                   _cm,
                                   true,
                                   false,
                                   // Override the default timeout for this client
                                   1000,
                                   // Specify a server name for this client
                                   true,
                                   "a_test_server"
                                   );

    fakecurl_responses.clear();
    fakecurl_responses["http://10.42.42.42:80/blah/blah/blah"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";
  }

  virtual ~HttpClientTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
    delete _private_http; _private_http = NULL;
    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }
};

TEST_F(HttpClientTest, SimpleGet)
{
  string output;
  long ret = _http->send_get("http://cyrus/blah/blah/blah", output, "gandalf", 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  // This client will be using the default timeout (550)
  EXPECT_EQ(550, req._timeout_ms);
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

TEST_F(HttpClientTest, BadURL)
{
  string output;
  long ret = _http->send_get("blah blah", output, "gandalf", 0);
  EXPECT_EQ(HTTP_BAD_REQUEST, ret);
}

TEST_F(HttpClientTest, NoTargets)
{
  string output;
  _resolver._targets.clear();
  long ret = _http->send_get("http://cyrus/blah/blah/blah", output, "gandalf", 0);
  EXPECT_EQ(HTTP_NOT_FOUND, ret);
}

TEST_F(HttpClientTest, GetWithHeaders)
{
  std::vector<std::string> headers;
  headers.push_back("HttpClientTest: true");

  std::string output;
  long ret = _http->send_get("http://cyrus/blah/blah/blah", output, headers, 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];

  // The CURL request should contain an "HttpClientTest" header whose value is
  // "true".
  bool found_header = false;
  for(std::list<std::string>::iterator it = req._headers.begin();
      it != req._headers.end();
      ++it)
  {
    if (*it == "HttpClientTest: true")
    {
      found_header = true;
    }
  }

  EXPECT_TRUE(found_header);
}

// Check that the option to omit bodies from SAS logs works.
TEST_F(HttpClientTest, SasOmitBody)
{
  mock_sas_collect_messages(true);
  MockSASMessage* req_event;
  MockSASMessage* rsp_event;

  std::map<std::string, std::string> headers;
  headers["HttpClientTest"] = "true";
  string test_body = "test body";

  _private_http->send_post("http://cyrus/blah/blah/blah", headers, test_body, 0, "gandalf");

    // This client will be using the override timeout (1000)
  Request& req = fakecurl_requests["http://cyrus:80/blah/blah/blah"];
  EXPECT_EQ(1000, req._timeout_ms);

  req_event = mock_sas_find_event(SASEvent::TX_HTTP_REQ);

  EXPECT_TRUE(req_event != NULL);

  bool body_omitted = (req_event->var_params[2].find(BODY_OMITTED) != string::npos);
  EXPECT_TRUE(body_omitted);

  rsp_event = mock_sas_find_event(SASEvent::RX_HTTP_RSP);
  EXPECT_TRUE(req_event != NULL);

  bool rsp_body_omitted = (rsp_event->var_params[2].find(BODY_OMITTED) != string::npos);
  EXPECT_TRUE(rsp_body_omitted);

  mock_sas_collect_messages(false);
}

// Check that the request is logged as normal if there is no body in the request to obscure.
TEST_F(HttpClientTest, SasNoBodyToOmit)
{
  mock_sas_collect_messages(true);
  std::map<std::string, std::string> headers;
  headers["HttpClientTest"] = "true";
  _private_http->send_post("http://cyrus/blah/blah/blah", headers, "", 0, "gandalf");

  MockSASMessage* req_event = mock_sas_find_event(SASEvent::TX_HTTP_REQ);
  EXPECT_TRUE(req_event != NULL);

  bool body_omitted = (req_event->var_params[2].find(BODY_OMITTED) != string::npos);
  EXPECT_FALSE(body_omitted);

  mock_sas_collect_messages(false);
}
