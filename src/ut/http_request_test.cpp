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
using ::testing::AllOf;
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

    // If we don't override the default behaviour, return a nonsensical HTTP Code
    ON_CALL(*_client, send_request(_)).WillByDefault(Return(HttpResponse(-1, "", {})));
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
  EXPECT_CALL(*_client, send_request(AllOf(IsPost(),
                                           HasScheme("http"),
                                           HasServer("server"),
                                           HasPath("/testpath"),
                                           HasBody(""),
                                           Field(&HttpRequest::_headers, IsEmpty()),
                                           HasUsername(""),
                                           HasTrail(0L),
                                           HasHostState(BaseResolver::ALL_LISTS))));

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
  EXPECT_CALL(*_client, send_request(HasBody(request_body)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_body(request_body);
  req.send();
}

// Test that the username can be set on a request
TEST_F(HttpRequestTest, SetUsername)
{
  std::string username = "test_user";
  EXPECT_CALL(*_client, send_request(HasUsername(username)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.set_username(username);
  req.send();
}

// Test the SAS trail can be set on a requeset
TEST_F(HttpRequestTest, SetSasTrail)
{
  // SAS Trail IDs are a typedefed uint64_t
  uint64_t test_trail_id = 12345;

  EXPECT_CALL(*_client, send_request(HasTrail(test_trail_id)));

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

  EXPECT_CALL(*_client, send_request(HasHeader(request_header)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.add_header(request_header);
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

  EXPECT_CALL(*_client, send_request(AllOf(HasHeader(request_header_1),
                                           HasHeader(request_header_2))));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  req.add_header(request_header_1);
  req.add_header(request_header_2);
  req.send();
}

// Test allowed_host_state can be changed on a request
TEST_F(HttpRequestTest, SetAllowedHostState)
{
  EXPECT_CALL(*_client, send_request(HasHostState(BaseResolver::WHITELISTED)));

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
  EXPECT_CALL(*_client, send_request(_)).WillOnce(Return(HttpResponse(rc, "", {})));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ(HTTP_OK, resp.get_rc());
}

// Test HttpResponses have the returned body stored properly
TEST_F(HttpRequestTest, GetRespBody)
{
  HTTPCode rc = HTTP_OK;
  std::string test_body = "Test body";

  EXPECT_CALL(*_client, send_request(_))
    .WillOnce(Return(HttpResponse(rc, test_body, {})));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ("Test body", resp.get_body());
}

// Test HttpResponses have the returned headers stored properly
TEST_F(HttpRequestTest, GetRespHeaders)
{
  HTTPCode rc = HTTP_OK;
  std::map<std::string, std::string> test_headers;
  test_headers.insert(std::make_pair("Test-Header", "Test value"));

  EXPECT_CALL(*_client, send_request(_))
    .WillOnce(Return(HttpResponse(rc, "", test_headers)));

  HttpRequest req = HttpRequest(server, scheme, _client, HttpClient::RequestType::POST, path);
  HttpResponse resp = req.send();

  EXPECT_EQ("Test value", resp.get_headers()["Test-Header"]);
}
