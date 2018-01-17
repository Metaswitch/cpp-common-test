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
using ::testing::IsEmpty;
using ::testing::Pointee;

class HttpRequestTest : public ::testing::Test
{

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
// Verify that sensible defaults are set on all parameters
TEST_F(HttpRequestTest, send_basic_request)
{
  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST, "http://server/testpath", _, _, _, _, _, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.send();
}


// Chech that the HttpRequest defaults non-mandatory arguments to sensible values
//Potentiall we want to do this with a custom matcher "IsEmptyMapOfType..."
//Hit pain with this as the map of resp_headers is passed in as a ref, so the mock comparison
//compares the mem location, not 'are these the same sort of thing'

TEST_F(HttpRequestTest, test_default_params)
{
  //  Default values we expect when not specified on a request
  std::string req_body = "";
  std::string resp_body = "";
  std::string username = "";
  uint64_t empty_sas_trail = 0;
  std::vector<std::string> req_headers;
  std::map<std::string, std::string> resp_headers;

  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST, "http://server/testpath", req_body, resp_body, username, empty_sas_trail, req_headers, Pointee(IsEmpty()), _));
//                                     "http://server/testpath",       // Full req URL
//                                     req_body,                       // req body
//                                     resp_body,                      // resp body
//                                     username,                       // username
//                                     empty_sas_trail,                // SAS Trail ID 
//                                     req_headers,                    // req headers
//                                     &resp_headers,                   // resp headers
//                                     BaseResolver::ALL_LISTS));      // allowed host state

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.send();
}

///
// Test SET methods
///

// Test that a string body set on the request is sent through to the http client
TEST_F(HttpRequestTest, set_body)
{
  std::string request_body = "test body";
  EXPECT_CALL(*_client, send_request(_, _, request_body, _, _, _, _, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.set_req_body(request_body);
  req.send();
}

// Test that the username  can be set on a request
TEST_F(HttpRequestTest, set_username)
{
  std::string username = "test_user";
  EXPECT_CALL(*_client, send_request(_, _, _, _, username, _, _, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.set_username(username);
  req.send();
}

// Test the SAS trail can be set on a requeset
TEST_F(HttpRequestTest, set_sas_trail)
{
  // SAS Trail IDs are a typedefed uint64_t
  uint64_t test_trail_id = 12345;

  // Set up a variable to match what the send_request call should contain
  EXPECT_CALL(*_client, send_request(_, _, _, _, _, test_trail_id, _, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.set_sas_trail(test_trail_id);
  req.send();
}

// Test a header can be set on a request
TEST_F(HttpRequestTest, set_header)
{
  std::string request_header = "X-Test-Header: Test";

  // Set up a variable to match what the send_request call should contain
  std::vector<std::string> expected_header_param;
  expected_header_param.push_back(request_header);

  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, expected_header_param, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.add_req_header(request_header);
  req.send();
}

// Test multiple headers can be set on a request
TEST_F(HttpRequestTest, set_multiple_headers)
{
  std::string request_header_1 = "X-Test-Header: Test";
  std::string request_header_2 = "X-Other-Test-Header: Test";

  // Set up a variable to match what the send_request call should contain
  std::vector<std::string> expected_header_param;
  expected_header_param.push_back(request_header_1);
  expected_header_param.push_back(request_header_2);

  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, expected_header_param, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.add_req_header(request_header_1);
  req.add_req_header(request_header_2);
  req.send();
}

// Test allowed_host_state can be changed on a request
TEST_F(HttpRequestTest, set_allowed_host_state)
{
  // Set up a variable to match what the send_request call should contain
  EXPECT_CALL(*_client, send_request(_, _, _, _, _, _, _, _, BaseResolver::WHITELISTED));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  req.set_allowed_host_state(BaseResolver::WHITELISTED);
  req.send();
}

///
// Test GET methods on HttpResponse
///

// Test HttpResponses have the return code stored properly
TEST_F(HttpRequestTest, get_return_code)
{
  HTTPCode rc = HTTP_OK;

  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST, "http://server/testpath", _, _, _, _, _, _, _)).WillOnce(Return(rc));

  HttpRequest req = HttpRequest("server", "http", _client, HttpClient::RequestType::POST, "/testpath");
  HttpResponse resp = req.send();

  EXPECT_EQ(HTTP_OK, resp.get_return_code());
}
