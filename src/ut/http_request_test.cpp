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

TEST_F(HttpRequestTest, send_post)
{
  EXPECT_CALL(*_client, send_request(HttpClient::RequestType::POST, "http://server/testpath", _, _, _, _, _, _, _));

  HttpRequest req = HttpRequest("server", "http", _client, "/testpath");
  req.send(HttpClient::RequestType::POST);
}
