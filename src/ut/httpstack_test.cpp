/**
 * @file httpstack_test.cpp UT for HttpStack module.
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

#include "test_utils.hpp"
#include "test_interposer.hpp"
#include "curl_interposer.hpp"

#include "httpstack.h"

#include "mockloadmonitor.hpp"
#include "fakesimplestatsmanager.hpp"
#include "mock_sas.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;
using ::testing::Gt;

const SAS::TrailId FAKE_TRAIL_ID = 0x12345678;

/// Fixture for HttpStackTest.
class HttpStackTest : public testing::Test
{
public:
  HttpStackTest()
  {
    cwtest_release_curl();
    _stack = new HttpStack(1, NULL);
    _host = "127.0.0.1";
    _port = 16384 + (getpid() % 16384);
    std::stringstream ss;
    ss << "http://" << _host << ":" << _port;
    _url_prefix = ss.str();
  }

  virtual ~HttpStackTest()
  {
    delete _stack; _stack = NULL;
    cwtest_control_curl();
  }

  void start_stack(std::string host = "")
  {
    _stack->initialize();
    _stack->bind_tcp_socket((host == "" ? _host.c_str() : host.c_str()), _port);
    _stack->start();
  }

  void stop_stack()
  {
    _stack->stop();
    _stack->wait_stopped();
  }

  int get(const std::string& path,
          int& status,
          std::string& response,
          std::list<std::string>* headers=NULL)
  {
    std::string url = _url_prefix + path;
    struct curl_slist *extra_headers = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();

    char errbuf[CURL_ERROR_SIZE];
    proxy_curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;
    proxy_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &string_store);
    proxy_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    proxy_curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    if (headers && headers->size() > 0)
    {
      for(std::list<std::string>::iterator hdr = headers->begin();
          hdr != headers->end();
          ++hdr)
      {
        extra_headers = curl_slist_append(extra_headers, hdr->c_str());
      }
      proxy_curl_easy_setopt(curl, CURLOPT_HTTPHEADER, extra_headers);
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
    {
      // Debug printf for test code. This call should never fail, so
      // we print to screen if it ever does to help debugging
      size_t len = strlen(errbuf);
      if (len)
      {
        printf("ERROR %s", errbuf);
      }
      else
      {
        printf("ERROR %s", curl_easy_strerror(rc));
      }
    }

    proxy_curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(extra_headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return (int)rc;
  }

  HttpStack* _stack;

private:
  static size_t string_store(void* ptr, size_t size, size_t nmemb, void* stream)
  {
    ((std::string*)stream)->append((char*)ptr, size * nmemb);
    return (size * nmemb);
  }

  std::string _host;
  int _port;
  std::string _url_prefix;
};

class HttpStackStatsTest : public HttpStackTest
{
public:
  HttpStackStatsTest() : HttpStackTest()
  {
    // Replace the stack with a version that has a stats manager.
    delete _stack; _stack = NULL;
    _stack = new HttpStack(1, NULL, NULL, &_load_monitor, &_stats_manager);

    cwtest_completely_control_time();
  }

  virtual ~HttpStackStatsTest()
  {
    cwtest_reset_time();
  }

private:
  // Strict mocks - we only allow method calls that the test explicitly expects.
  StrictMock<MockLoadMonitor> _load_monitor;
  FakeSimpleStatsManager _stats_manager;
};

// Basic handler.
class BasicHandler : public HttpStack::HandlerInterface
{
public:
  void process_request(HttpStack::Request &req, SAS::TrailId trail)
  {
    req.add_content("OK");
    req.send_reply(200, trail);
  }
};

// A handler that takes a long time to process requests (to test latency
// stats).
const int DELAY_MS = 2000;
const unsigned long DELAY_US = DELAY_MS * 1000;

class SlowHandler : public HttpStack::HandlerInterface
{
public:
  void process_request(HttpStack::Request &req, SAS::TrailId trail)
  {
    cwtest_advance_time_ms(DELAY_MS);
    req.send_reply(200, trail);
  }
};

// A handler that applies a load penalty.
class PenaltyHandler : public HttpStack::HandlerInterface
{
public:
  void process_request(HttpStack::Request &req, SAS::TrailId trail)
  {
    req.record_penalty();
    req.send_reply(200, trail);
  }
};

TEST_F(HttpStackTest, SimpleMainline)
{
  start_stack("127.0.0.10");
  sleep(1);
  stop_stack();
}

TEST_F(HttpStackTest, SimpleMainlinIPv6)
{
  start_stack("::1");
  sleep(1);
  stop_stack();
}

TEST_F(HttpStackTest, NoHandler)
{
  cwtest_release_curl();
  start_stack();

  int status;
  std::string response;
  int rc = get("/NoHandler", status, response);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(404, status);

  stop_stack();
  cwtest_control_curl();
}

TEST_F(HttpStackTest, SimpleHandler)
{
  start_stack();

  BasicHandler handler;
  _stack->register_handler("^/BasicHandler$", &handler);

  int status;
  std::string response;
  int rc = get("/BasicHandler", status, response);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(200, status);
  ASSERT_EQ("OK", response);

  // Check that NoHandler _doesn't_ match.
  rc = get("/NoHandler", status, response);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(404, status);

  stop_stack();
}

// Check that the stack copes with receiving a SAS correlation header.
TEST_F(HttpStackTest, SASCorrelationHeader)
{
  mock_sas_collect_messages(true);
  start_stack();

  BasicHandler handler;
  _stack->register_handler("^/BasicHandler$", &handler);

  int status;
  std::string response;
  std::list<std::string> hdrs;
  hdrs.push_back("X-SAS-HTTP-Branch-ID: 12345678-1234-1234-1234-123456789ABC");

  int rc = get("/BasicHandler", status, response, &hdrs);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(200, status);
  ASSERT_EQ("OK", response);

  MockSASMessage* marker = mock_sas_find_marker(MARKER_ID_VIA_BRANCH_PARAM);
  EXPECT_TRUE(marker != NULL);
  EXPECT_EQ(marker->var_params.size(), 1u);
  EXPECT_EQ(marker->var_params[0], "12345678-1234-1234-1234-123456789ABC");

  stop_stack();
  mock_sas_collect_messages(false);
}

TEST_F(HttpStackStatsTest, SuccessfulRequest)
{
  start_stack();

  SlowHandler handler;
  _stack->register_handler("^/BasicHandler$", &handler);

  EXPECT_CALL(_load_monitor, admit_request(_)).WillOnce(Return(true));
  EXPECT_CALL(_load_monitor, request_complete(DELAY_US)).Times(1);

  int status;
  std::string response;
  int rc = get("/BasicHandler", status, response);
  EXPECT_EQ(1, _stats_manager._incoming_requests->_count);
  EXPECT_EQ(1, _stats_manager._latency_us->_count);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(200, status);

  stop_stack();
}

TEST_F(HttpStackStatsTest, RejectOverload)
{
  start_stack();

  BasicHandler handler;
  _stack->register_handler("^/BasicHandler$", &handler);

  EXPECT_CALL(_load_monitor, admit_request(_)).WillOnce(Return(false));

  int status;
  std::string response;
  int rc = get("/BasicHandler", status, response);
  EXPECT_EQ(1, _stats_manager._incoming_requests->_count);
  EXPECT_EQ(1, _stats_manager._rejected_overload->_count);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(503, status);  // Request is rejected with a 503.

  stop_stack();
}

TEST_F(HttpStackStatsTest, LatencyPenalties)
{
  start_stack();

  PenaltyHandler handler;
  _stack->register_handler("^/BasicHandler$", &handler);

  EXPECT_CALL(_load_monitor, admit_request(_)).WillOnce(Return(true));
  EXPECT_CALL(_load_monitor, incr_penalties()).Times(1);
  EXPECT_CALL(_load_monitor, request_complete(_)).Times(1);

  int status;
  std::string response;
  int rc = get("/BasicHandler", status, response);
  EXPECT_EQ(1, _stats_manager._incoming_requests->_count);
  EXPECT_EQ(1, _stats_manager._latency_us->_count);
  ASSERT_EQ(CURLE_OK, rc);
  ASSERT_EQ(200, status);

  stop_stack();
}
