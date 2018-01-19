/**
 * @file httpclient_test.cpp UT for HttpClient.
 *
 * Copyright (C) Metaswitch Networks 2018
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
#include "mockloadmonitor.hpp"
#include "fakesnmp.hpp"
#include "curl_interposer.hpp"
#include "fakecurl.hpp"
#include "test_interposer.hpp"

using namespace std;
using ::testing::MatchesRegex;
using ::testing::_;
using ::testing::Return;
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
  NiceMock<MockLoadMonitor>* _lm = new NiceMock<MockLoadMonitor>();
  HttpClient* _http;
  HttpClient* _alt_http;
  string _server_display_name;

  HttpClientTest() :
    _resolver("10.42.42.42"),
    _server_display_name("a_test_server")
  {
    // We need two HttpClients in the tests, so that we can verify different
    // configurations, such as asserting user ID, or obscuring SAS bodies
    _http = new HttpClient(true,
                           &_resolver,
                           SASEvent::HttpLogLevel::PROTOCOL,
                           _cm);

    // Alternate HttpClient for testing non-default configuration
    _alt_http = new HttpClient(false, // Don't assert user so we can test the header is not added
                               &_resolver,
                               NULL,  // SNMP Stat table
                               _lm,   // Load monitor
                               SASEvent::HttpLogLevel::PROTOCOL, // SAS log level
                               _cm,   // Comm Monitor
                               true,  // should_omit_body
                               false, // remote_connection
                               1000,  // Override the default timeout for this client
                               true,  // log_display_address
                               _server_display_name // Specify a server name
                               );

    // Set up responses that can be used in the tests
    fakecurl_responses.clear();
    fakecurl_responses["http://10.42.42.42:80/test"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><xmltag>Document</xmltag>";
    fakecurl_responses["http://10.42.42.42:80/test/not_found"] = CURLE_REMOTE_FILE_NOT_FOUND;
    fakecurl_responses["http://10.42.42.42:80/test/503"] = 503;
    fakecurl_responses["http://10.42.42.42:80/test/504"] = 504;
    fakecurl_responses["http://10.42.42.42:80/test/recv_error"] = CURLE_RECV_ERROR;
    fakecurl_responses["http://10.42.42.42:80/test/get_with_retry"] = Response(CURLE_SEND_ERROR, "<message>Test message</message>");
    fakecurl_responses["http://10.42.42.42:80/delete_id"] = CURLE_OK;
    fakecurl_responses["http://10.42.42.42:80/put_id"] = CURLE_OK;
    fakecurl_responses["http://10.42.42.42:80/put_id_response"] = Response({"response"});
    fakecurl_responses["http://10.42.42.42:80/post_id"] = Response({"Location: test"});
  }

  virtual ~HttpClientTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
    delete _alt_http; _alt_http = NULL;
    delete _lm; _lm = NULL;
    delete _cm; _cm = NULL;
    delete _am; _am = NULL;
  }

  // Some empty parameters for use in send_request when not being tested
  std::string default_body;
  std::string default_response;
  std::string default_username;
  uint64_t default_sas_trail = 0;
  std::vector<std::string> default_req_headers;
  std::map<std::string, std::string> default_resp_headers;
  int default_host_state = BaseResolver::ALL_LISTS;

};

// Basic get test. Assert the default settings etc.
TEST_F(HttpClientTest, SimpleGet)
{
  EXPECT_CALL(*_cm, inform_success(_));
  string output;

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/test",
                                 default_body,
                                 output,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><xmltag>Document</xmltag>", output);

  Request& req = fakecurl_requests["http://cyrus:80/test"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  // This client will be using the default timeout (550)
  EXPECT_EQ(550, req._timeout_ms);
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

// Test that calling with headers results in them being sent
TEST_F(HttpClientTest, GetWithHeaders)
{
  EXPECT_CALL(*_cm, inform_success(_));
  std::vector<std::string> headers;
  headers.push_back("HttpClientTest: true");

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/test",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  Request& req = fakecurl_requests["http://cyrus:80/test"];

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

// Test that usernames are set in the X-XCAP-Asserted-Identity header
// Test below mirrors this, so update accordingly.
TEST_F(HttpClientTest, GetWithUsername)
{
  EXPECT_CALL(*_cm, inform_success(_));
  std::string username = "Gandalf";

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/test",
                                 default_body,
                                 default_response,
                                 username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  Request& req = fakecurl_requests["http://cyrus:80/test"];

  // The CURL request should contain an "X-XCAP-Asserted-Identity:" header,
  // whose value is the username - "Gandalf"
  bool found_header = false;
  for(std::list<std::string>::iterator it = req._headers.begin();
      it != req._headers.end();
      ++it)
  {
    if (*it == "X-XCAP-Asserted-Identity: Gandalf")
    {
      found_header = true;
    }
  }

  EXPECT_TRUE(found_header);
}

// Test that usernames are not set in the X-XCAP-Asserted-Identity header if
// the Client has not been set to assert users. Mirror of GetWithUsername, but
// using the other Client, and asserting no header is found.
TEST_F(HttpClientTest, GetWithUsername_NoAssertUser)
{
  EXPECT_CALL(*_cm, inform_success(_));
  std::string username = "Gandalf";

  long ret = _alt_http->send_request(HttpClient::RequestType::GET,
                                     "http://cyrus/test",
                                     default_body,
                                     default_response,
                                     username,
                                     default_sas_trail,
                                     default_req_headers,
                                     &default_resp_headers,
                                     default_host_state);

  EXPECT_EQ(200, ret);
  Request& req = fakecurl_requests["http://cyrus:80/test"];

  // The CURL request should NOT contain an "X-XCAP-Asserted-Identity:" header,
  // whose value is the username - "Gandalf"
  bool found_header = false;
  for(std::list<std::string>::iterator it = req._headers.begin();
      it != req._headers.end();
      ++it)
  {
    if (*it == "X-XCAP-Asserted-Identity: Gandalf")
    {
      found_header = true;
    }
  }

  EXPECT_FALSE(found_header);
}

// Test IPv6 basic functionality
TEST_F(HttpClientTest, IPv6Get)
{
  EXPECT_CALL(*_cm, inform_success(_));
  FakeHttpResolver _resolver("1::1");
  fakecurl_responses["http://[1::1]:80/ipv6get"] = CURLE_OK;

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://[1::1]:80/ipv6get",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpClientTest, GetFailureNotFound)
{
  EXPECT_CALL(*_cm, inform_failure(_)).Times(1);

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus:80/test/not_found",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(404, ret);
}

// Test we inform failure and incur penalty on a request that gets 503s back
TEST_F(HttpClientTest, GetFailure503)
{
  EXPECT_CALL(*_cm, inform_failure(_)).Times(1);
  EXPECT_CALL(*_lm, incr_penalties()).Times(1);

  long ret = _alt_http->send_request(HttpClient::RequestType::GET,
                                     "http://cyrus:80/test/503",
                                     default_body,
                                     default_response,
                                     default_username,
                                     default_sas_trail,
                                     default_req_headers,
                                     &default_resp_headers,
                                     default_host_state);

  EXPECT_EQ(503, ret);
}

// Test that we attempt a retry on a failed request
TEST_F(HttpClientTest, SimpleGetRetry)
{
  EXPECT_CALL(*_cm, inform_success(_));
  // If we hit an error, and it isn't a 504, we shouldn't incur a penalty
  EXPECT_CALL(*_lm, incr_penalties()).Times(0);
  std::string response;

  // Get a failure on the connection and retry it.
  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus:80/test/get_with_retry",
                                 default_body,
                                 response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<message>Test message</message>", response);
}

// Test that we incur a penalty for a single 504, and do not retry
TEST_F(HttpClientTest, Get504)
{
  EXPECT_CALL(*_cm, inform_success(_));
  EXPECT_CALL(*_lm, incr_penalties()).Times(1);

  std::string response;

  // Get the 504 failure on the connection with the mock load monitor
  long ret = _alt_http->send_request(HttpClient::RequestType::GET,
                                     "http://cyrus:80/test/504",
                                     default_body,
                                     response,
                                     default_username,
                                     default_sas_trail,
                                     default_req_headers,
                                     &default_resp_headers,
                                     default_host_state);

  EXPECT_EQ(504, ret);
}

// Test that a receive error properly informs the communication monitor
TEST_F(HttpClientTest, ReceiveError)
{
  EXPECT_CALL(*_cm, inform_failure(_)).Times(1);

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus:80/test/recv_error",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(500, ret);
}

TEST_F(HttpClientTest, SimplePost)
{
  EXPECT_CALL(*_cm, inform_success(_));
  long ret = _http->send_request(HttpClient::RequestType::POST,
                                 "http://cyrus:80/post_id",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);

  Request& req = fakecurl_requests["http://cyrus:80/post_id"];

  EXPECT_EQ("POST", req._method);
  EXPECT_EQ("", req._body);
}

// Test that the message body is built and sent correctly
TEST_F(HttpClientTest, SimplePostWithBody)
{
  std::string test_body = "Test body";

  long ret = _http->send_request(HttpClient::RequestType::POST,
                                 "http://cyrus:80/post_id",
                                 test_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);

  Request& req = fakecurl_requests["http://cyrus:80/post_id"];

  EXPECT_EQ("POST", req._method);
  EXPECT_EQ("Test body", req._body);
}

// Test a post with multiple headers to send actually sends them all
TEST_F(HttpClientTest, SimplePostWithHeaders)
{
  std::vector<std::string> req_headers;
  req_headers.push_back("Content-Type: application/x-www-form-urlencoded");
  req_headers.push_back("X-Test-Header: Testing");

  long ret = _http->send_request(HttpClient::RequestType::POST,
                                 "http://cyrus:80/post_id",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);

  Request& req = fakecurl_requests["http://cyrus:80/post_id"];
  // The CURL request should contain the two headers added above
  bool found_first_header = false;
  bool found_second_header = false;
  for(std::list<std::string>::iterator it = req._headers.begin();
      it != req._headers.end();
      ++it)
  {
    if (*it == "Content-Type: application/x-www-form-urlencoded")
    {
      found_first_header = true;
    }
    else if (*it == "X-Test-Header: Testing")
    {
      found_second_header = true;
    }
  }

  EXPECT_TRUE(found_first_header);
  EXPECT_TRUE(found_second_header);
}

TEST_F(HttpClientTest, SimplePut)
{
  EXPECT_CALL(*_cm, inform_success(_));

  long ret = _http->send_request(HttpClient::RequestType::PUT,
                                 "http://cyrus:80/put_id",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
}

TEST_F(HttpClientTest, SimplePutWithResponse)
{
  EXPECT_CALL(*_cm, inform_success(_));

  std::string response;
  long ret = _http->send_request(HttpClient::RequestType::PUT,
                                 "http://cyrus:80/put_id_response",
                                 default_body,
                                 response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("response", response);
}

TEST_F(HttpClientTest, SimpleDelete)
{
  EXPECT_CALL(*_cm, inform_success(_));
  long ret = _http->send_request(HttpClient::RequestType::DELETE,
                                 "http://cyrus:80/delete_id",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
}

// Test that we correctly create and send SAS correlation headers
TEST_F(HttpClientTest, SASCorrelationHeader)
{
  mock_sas_collect_messages(true);

  std::string uuid;

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus:80/test",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  Request& req = fakecurl_requests["http://cyrus:80/test"];

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

// Test that sending to a non-standard port is handled correctly
TEST_F(HttpClientTest, ParseHostPort)
{
  fakecurl_responses["http://10.42.42.42:1234/port-1234"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";
  std::string response;
  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus:1234/port-1234",
                                 default_body,
                                 response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", response);
}

// Test that sending to a non-standard port is handled correctly for IPv6
TEST_F(HttpClientTest, ParseHostPortIPv6)
{
  EXPECT_CALL(*_cm, inform_success(_));
  FakeHttpResolver _resolver("1::1");
  fakecurl_responses["http://[1::1]:1234/ipv6get"] = CURLE_OK;

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://[1::1]:1234/ipv6get",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
}

// Test that sending without a port goes to the default http port
TEST_F(HttpClientTest, ParseNoPort)
{
  fakecurl_responses["http://10.42.42.42:80/port-80"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";
  std::string response;
  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/port-80",
                                 default_body,
                                 response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", response);
}

// Test that sending without a port goes to the default http port for IPv6
TEST_F(HttpClientTest, ParseNoPortIPv6)
{
  EXPECT_CALL(*_cm, inform_success(_));
  FakeHttpResolver _resolver("1::1");
  fakecurl_responses["http://[1::1]:80/ipv6get/port-80"] = CURLE_OK;

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://[1::1]/ipv6get/port-80",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(200, ret);
}

// Test a non-resolvable URL gives us a BAD REQUEST error
TEST_F(HttpClientTest, BadURL)
{
  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "blah blah",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(HTTP_BAD_REQUEST, ret);
}

// Test we get an HTTP NOT FOUND back if there are no resolvable targets
TEST_F(HttpClientTest, NoTargets)
{
  _resolver._targets.clear();

  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/test",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 &default_resp_headers,
                                 default_host_state);

  EXPECT_EQ(HTTP_NOT_FOUND, ret);
}

// Send request takes in a pointer to a map to store response headers in. This
// can be passed in as NULL, and an internal map is created.
TEST_F(HttpClientTest, TestNullHeaderMap)
{
  long ret = _http->send_request(HttpClient::RequestType::GET,
                                 "http://cyrus/test",
                                 default_body,
                                 default_response,
                                 default_username,
                                 default_sas_trail,
                                 default_req_headers,
                                 NULL,
                                 default_host_state);

  EXPECT_EQ(HTTP_OK, ret);
}

// Check that the option to omit bodies from SAS logs works.
TEST_F(HttpClientTest, SasOmitBody)
{
  mock_sas_collect_messages(true);
  MockSASMessage* req_event;
  MockSASMessage* rsp_event;

  string test_body = "test body";

  _alt_http->send_request(HttpClient::RequestType::POST,
                          "http://cyrus/test",
                          test_body,
                          default_response,
                          default_username,
                          default_sas_trail,
                          default_req_headers,
                          &default_resp_headers,
                          default_host_state);

    // This client will be using the override timeout (1000)
  Request& req = fakecurl_requests["http://cyrus:80/test"];
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

  string empty_body = "";

  _alt_http->send_request(HttpClient::RequestType::POST,
                          "http://cyrus/test",
                          empty_body,
                          default_response,
                          default_username,
                          default_sas_trail,
                          default_req_headers,
                          &default_resp_headers,
                          default_host_state);

  MockSASMessage* req_event = mock_sas_find_event(SASEvent::TX_HTTP_REQ);
  EXPECT_TRUE(req_event != NULL);

  bool body_omitted = (req_event->var_params[2].find(BODY_OMITTED) != string::npos);
  EXPECT_FALSE(body_omitted);

  mock_sas_collect_messages(false);
}

// Check that we correctly apply the specified display name.
TEST_F(HttpClientTest, SasDisplayName)
{
  mock_sas_collect_messages(true);

  _alt_http->send_request(HttpClient::RequestType::POST,
                          "http://cyrus/test",
                          default_body,
                          default_response,
                          default_username,
                          default_sas_trail,
                          default_req_headers,
                          &default_resp_headers,
                          default_host_state);

  MockSASMessage* req_event = mock_sas_find_event(SASEvent::TX_HTTP_REQ);
  EXPECT_TRUE(req_event != NULL);

  // Ensure that display name added to SAS event matches the name we passed
  // to the HttpClient constructor
  EXPECT_EQ(req_event->var_params[0], _server_display_name);

  mock_sas_collect_messages(false);
}

// Test creation and destruction of the basic http resolver
TEST_F(HttpClientTest, BasicResolverTest)
{
  // Just check the resolver constructs/destroys correctly.
  HttpResolver resolver(NULL, AF_INET);
}

/// Fixture for blacklist tests
class HttpClientBlacklistTest : public BaseTest
{
  StrictMock<MockHttpResolver> _resolver;
  HttpClient* _http;
  LoadMonitor _lm;
  AlarmManager* _am = new AlarmManager();
  NiceMock<MockCommunicationMonitor>*_cm = new NiceMock<MockCommunicationMonitor>(_am);

  HttpClientBlacklistTest() :
    _lm(100000, 20, 10, 10, 0)
  {
    _http = new HttpClient(true,
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

  ~HttpClientBlacklistTest()
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
      Utils::parse_ip_target(os.str(), ai.address);
      targets.push_back(ai);
      os.str(std::string());
    }
    return targets;
  }

  // Some empty parameters for use in send_request when not being tested
  std::string default_body;
  std::string default_response;
  std::string default_username;
  uint64_t default_sas_trail = 0;
  std::vector<std::string> default_req_headers;
  std::map<std::string, std::string> default_resp_headers;
  int default_host_state = BaseResolver::ALL_LISTS;
};

TEST_F(HttpClientBlacklistTest, BlacklistTestHttpSuccess)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/http_success",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

TEST_F(HttpClientBlacklistTest, BlacklistTestTcpSuccess)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/tcp_success",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

TEST_F(HttpClientBlacklistTest, BlacklistTestOneFailure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/one_failure",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

TEST_F(HttpClientBlacklistTest, BlacklistTestOne503Failure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0], 30)).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/one_503_failure",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

// Note that the current implementation ignores the date in a Retry-After header
TEST_F(HttpClientBlacklistTest, BlacklistTestOneDate503Failure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/one_date_503_failure",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

// Note that the current impementation ignores the date in a Retry-After header
TEST_F(HttpClientBlacklistTest, BlacklistTestOne503FailureNoRetryAfter)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, success(targets[0])).Times(1);
  EXPECT_CALL(_resolver, success(targets[1])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/one_503_failure_no_retry_after",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);
}

TEST_F(HttpClientBlacklistTest, BlacklistTestAllFailure)
{
  std::vector<AddrInfo> targets = create_targets(2);

  EXPECT_CALL(_resolver, resolve_iter(_,_,_,_)).
    WillOnce(Return(new SimpleAddrIterator(targets)));
  EXPECT_CALL(_resolver, blacklist(targets[0])).Times(1);
  EXPECT_CALL(_resolver, blacklist(targets[1])).Times(1);

  _http->send_request(HttpClient::RequestType::GET,
                      "http://cyrus/all_failure",
                      default_body,
                      default_response,
                      default_username,
                      default_sas_trail,
                      default_req_headers,
                      &default_resp_headers,
                      default_host_state);

}
