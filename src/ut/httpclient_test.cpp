/**
 * @file httpclient_test.cpp UT for Sprout HttpClient.
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
  HttpClientTest() :
    _resolver("10.42.42.42")
  {
    _http = new HttpClient(true,
                           &_resolver,
                           SASEvent::HttpLogLevel::PROTOCOL,
                           _cm);

    fakecurl_responses.clear();
    fakecurl_responses["http://10.42.42.42:80/blah/blah/blah"] = "<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>";
  }

  virtual ~HttpClientTest()
  {
    fakecurl_responses.clear();
    fakecurl_requests.clear();
    delete _http; _http = NULL;
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

  Request& req = fakecurl_requests["http://10.42.42.42:80/blah/blah/blah"];

  EXPECT_EQ("GET", req._method);
  EXPECT_FALSE(req._httpauth & CURLAUTH_DIGEST) << req._httpauth;
  EXPECT_EQ("", req._username);
  EXPECT_EQ("", req._password);
}

TEST_F(HttpClientTest, BadURL)
{
  string output;
  long ret = _http->send_get("blah blah", output, "gandalf", 0);
  EXPECT_EQ(HTTP_BAD_REQUEST, ret);
}

TEST_F(HttpClientTest, GetWithHeaders)
{
  std::vector<std::string> headers;
  headers.push_back("HttpClientTest: true");

  std::string output;
  long ret = _http->send_get("http://cyrus/blah/blah/blah", output, headers, 0);

  EXPECT_EQ(200, ret);
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"><boring>Document</boring>", output);

  Request& req = fakecurl_requests["http://10.42.42.42:80/blah/blah/blah"];

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
