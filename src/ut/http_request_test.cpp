/**
 * @file http_request_test.cpp UT for the HttpRequest interface.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_httpclient.h"
#include "http_request.h"

using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::SetArgReferee;
using ::testing::SetArgPointee;


class HttpRequestTest : public ::testing::Test
{

  std::string server = "server";
  std::string scheme = "http";
  std::string path = "/testpath";

  HttpRequestTest()
  {
    _client = new MockHttpClient();
  }

  ~HttpRequestTest()
  {
    delete _client; _client = NULL;
  }

  MockHttpClient* _client;
};

// Build and send a basic http request, with the default parameters
// The send method is passed through, so we shouldn't need individual tests to
// cover all of POST, PUT, GET, DELETE.
// Checks the HttpRequest defaults non-mandatory arguments to empty/sensible values
TEST_F(HttpRequestTest, SendBasicRequestDefaultParams)
{
  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST,  // Method
                                     "http://server/testpath",       // Full req URL
                                     IsEmpty(),                      // req body      empty string
                                     IsEmpty(),                      // resp body     empty string&
                                     IsEmpty(),                      // username      empty string
                                     0L,                             // SAS Trail ID  0-ed uint64_t
                                     IsEmpty(),                      // req headers   empty vector of strings
                                     Pointee(IsEmpty()),             // resp headers  empty map of string-string
                                     BaseResolver::ALL_LISTS));      // host state    default to All Lists

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.send();
}

///
// Test SET methods
///

// Test that a string body set on the request is sent through to the http client
TEST_F(HttpRequestTest, SetBody)
{
  std::string request_body = "test body";
  EXPECT_CALL(*_client, send_request(_, _, request_body, _, _, _, _, _, _));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_req_body(request_body);
  req.send();
}

// Test that the username can be set on a request
TEST_F(HttpRequestTest, SetUsername)
{
  std::string username = "test_user";
  EXPECT_CALL(*_client, send_request(_, _, _, _, username, _, _, _, _));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_username(username);
  req.send();
}

// Test the SAS trail can be set on a requeset
TEST_F(HttpRequestTest, SetSasTrail)
{
  // SAS Trail IDs are a typedefed uint64_t
  uint64_t test_trail_id = 12345;

  EXPECT_CALL(*_client, send_request(_, _, _, _, _, test_trail_id, _, _, _));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_sas_trail(test_trail_id);
  req.send();
}

// Test a header can be set on a request
TEST_F(HttpRequestTest, SetHeader)
{
  std::string request_header = "X-Test-Header: Test";

  // Set up a variable to match what the send_request call should contain
  std::vector<std::string> expected_header_param;
  expected_header_param.push_back(request_header);

  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, expected_header_param, _, _));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.add_req_header(request_header);
  req.send();
}

// Test multiple headers can be set on a request
TEST_F(HttpRequestTest, SetMultipleHeaders)
{
  std::string request_header_1 = "X-Test-Header: Test";
  std::string request_header_2 = "X-Other-Test-Header: Test";

  // Set up a variable to match what the send_request call should contain
  std::vector<std::string> expected_header_param;
  expected_header_param.push_back(request_header_1);
  expected_header_param.push_back(request_header_2);

  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, expected_header_param, _, _));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.add_req_header(request_header_1);
  req.add_req_header(request_header_2);
  req.send();
}

// Test allowed_host_state can be changed on a request
TEST_F(HttpRequestTest, SetAllowedHostState)
{
  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, _, _, BaseResolver::WHITELISTED));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_allowed_host_state(BaseResolver::WHITELISTED);
  req.send();
}

///
// Test GET methods on HttpResponse
///

// Test HttpResponses have the return code stored properly
TEST_F(HttpRequestTest, GetReturnCode)
{
  HTTPCode rc = HTTP_OK;

  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST,
                                     "http://server/testpath",
                                     _, _, _, _, _, _, _)).WillOnce(Return(rc));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ(HTTP_OK, resp.get_return_code());
}

// Test HttpResponses have the returned body stored properly
TEST_F(HttpRequestTest, GetRespBody)
{
  HTTPCode rc = HTTP_OK;
  std::string test_resp_body = "Test body";

  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST,
                                     "http://server/testpath",
                                     _, _, _, _, _, _, _)).WillOnce(DoAll(
                                            SetArgReferee<3>(test_resp_body),
                                            Return(rc)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ("Test body", resp.get_resp_body());
}

// Test HttpResponses have the returned headers stored properly
TEST_F(HttpRequestTest, GetRespHeaders)
{
  HTTPCode rc = HTTP_OK;
  std::map<std::string, std::string> test_resp_headers;
  test_resp_headers.insert(std::make_pair("Test-Header", "Test value"));
  

  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST,
                                     "http://server/testpath",
                                     _, _, _, _, _, _, _)).WillOnce(DoAll(
                                            SetArgPointee<7>(test_resp_headers),
                                            Return(rc)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ("Test value", resp.get_resp_headers()["Test-Header"]);
}
