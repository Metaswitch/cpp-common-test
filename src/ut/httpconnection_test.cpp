/**
 * @file httpconnection_test.cpp UT for HttpConnection.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "basetest.hpp"
#include "httpconnection.h"
#include "mock_httpclient.h"

class HttpConnectionTest : public BaseTest
{
  MockHttpClient* _test_client;
  std::string _test_server = "test_server";
  std::string _test_scheme = "test_http";
  std::string _test_path = "/test/path";

  HttpConnectionTest()
  {
    _test_client = new MockHttpClient();
  };

  ~HttpConnectionTest()
  {
    delete _test_client; _test_client = NULL;
  };
};

// Test the HttpRequest is set with the connection server
TEST_F(HttpConnectionTest, CreateRequestCheckServer)
{
  HttpConnection connection = HttpConnection(_test_server,
                                             _test_client,
                                             _test_scheme);
  HttpRequest req =
    connection.create_request(HttpClient::RequestType::GET, _test_path);

  // Expect that the HttpRequest has the correct details passed into it
  EXPECT_EQ(req._server, "test_server");
}

// Test the HttpRequest is set with the connection scheme
TEST_F(HttpConnectionTest, CreateRequestCheckScheme)
{
  HttpConnection connection = HttpConnection(_test_server,
                                             _test_client,
                                             _test_scheme);
  HttpRequest req =
    connection.create_request(HttpClient::RequestType::GET, _test_path);

  // Expect that the HttpRequest has the correct details passed into it
  EXPECT_EQ(req._scheme, "test_http");
}

// Test the HttpRequest is set with the requested path
TEST_F(HttpConnectionTest, CreateRequestCheckPath)
{
  HttpConnection connection = HttpConnection(_test_server,
                                             _test_client,
                                             _test_scheme);
  HttpRequest req =
    connection.create_request(HttpClient::RequestType::GET, _test_path);

  // Expect that the HttpRequest has the correct details passed into it
  EXPECT_EQ(req._path, "/test/path");
}

// Test the HttpRequest is set with the requested method
TEST_F(HttpConnectionTest, CreateRequestCheckMethod)
{
  HttpConnection connection = HttpConnection(_test_server,
                                             _test_client,
                                             _test_scheme);
  HttpRequest req =
    connection.create_request(HttpClient::RequestType::DELETE, _test_path);

  // Expect that the HttpRequest has the correct details passed into it
  EXPECT_EQ(req._method, HttpClient::RequestType::DELETE);
}

// Test the HttpRequest is set with the connection client
TEST_F(HttpConnectionTest, CreateRequestCheckClient)
{
  HttpConnection connection = HttpConnection(_test_server,
                                             _test_client,
                                             _test_scheme);
  HttpRequest req =
    connection.create_request(HttpClient::RequestType::GET, _test_path);

  // Expect that the HttpRequest has the correct details passed into it
  EXPECT_EQ(req._client, _test_client);
}
