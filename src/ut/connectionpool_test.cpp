/**
 * @file connectionpool_test.cpp UT for connection pooling classes in
 * connection_pool.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
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
#include "test_interposer.hpp"

#include "mock_connectionpool.h"

using ::testing::Return;

class ConnectionPoolTest : public ::testing::Test
{
public:
  static const int TEST_MAX_IDLE_TIME_S = 30;
  static const int TEST_TIME_DELTA_MS = 500;

  ConnectionPoolTest();
  ~ConnectionPoolTest();

  MockConnectionPool conn_pool;
  AddrInfo ai_1;
  AddrInfo ai_2;
};

ConnectionPoolTest::ConnectionPoolTest() : conn_pool(MockConnectionPool(TEST_MAX_IDLE_TIME_S))
{
  cwtest_completely_control_time();

  // Create a 'default' AddrInfo to use for indexing into the pool
  IP46Address address;
  address.af = AF_INET;
  inet_pton(AF_INET, "0.0.0.0", &address.addr.ipv4);
  ai_1.address = address;
  ai_1.port = 1;
  ai_1.transport = 0;
  ai_2 = ai_1;
  ai_2.port = 2;
}

ConnectionPoolTest::~ConnectionPoolTest()
{
  cwtest_reset_time();
}

// Test that the connection pool creates a new connection if one does not exist
// for the specified AddrInfo
TEST_F(ConnectionPoolTest, CreateNewConnection)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(1).WillOnce(Return(1));
  ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
  // Check that the connection has the correct parameters
  EXPECT_EQ(conn_handle.get_connection(), 1);
  EXPECT_EQ(conn_handle.get_target(), ai_1);
}

// Test that a connection is removed from the pool when selected by
// get_connection
TEST_F(ConnectionPoolTest, ConnectionRemovedFromPool)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(2).WillOnce(Return(1)).WillOnce(Return(2));

  // A connection is created and retrieved
  ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
  // Another connection is requested
  ConnectionHandle<int> conn_handle_2 = conn_pool.get_connection(ai_1);
  // The second call should return a different connection from the first
  EXPECT_EQ(conn_handle_2.get_connection(), 2);
}

// Test that a connection is retrieved from the pool if one exists for the
// specified AddrInfo, and that connections are returned to the pool on
// destruction of a handle
TEST_F(ConnectionPoolTest, RetrieveAndReturnConnection)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(1).WillOnce(Return(1));
  // Create then immediately destroy a ConnectionHandle, which should return the
  // connection to the pool
  conn_pool.get_connection(ai_1);

  // Another connection is requested
  ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
  // The second call should return the same connection as the first
  EXPECT_EQ(conn_handle.get_connection(), 1);
}

// Test that retrieving connections for two different targets works correctly
TEST_F(ConnectionPoolTest, RetrieveConnectionsToTwoTargets)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(conn_pool, create_connection(ai_2)).Times(1).WillOnce(Return(2));

  ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
  ConnectionHandle<int> conn_handle_2 = conn_pool.get_connection(ai_2);

  // Check that the retrieved connections correspond to the correct AddrInfo
  // objects
  EXPECT_EQ(conn_handle_1.get_connection(), 1);
  EXPECT_EQ(conn_handle_2.get_connection(), 2);
}

// Test that idle connections are removed from the pool after the specified
// time, but not before
TEST_F(ConnectionPoolTest, RemoveIdleConnections)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(2).WillOnce(Return(1)).WillOnce(Return(2));
  EXPECT_CALL(conn_pool, create_connection(ai_2)).Times(1).WillOnce(Return(3));

  // Create the connection by requesting a connection from the pool then
  // immediately returning it
  conn_pool.get_connection(ai_1);
  cwtest_advance_time_ms(1000 * TEST_MAX_IDLE_TIME_S - TEST_TIME_DELTA_MS);
  // The connection is still not considered idle
  {
    // The connection should still be in the pool
    ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
    EXPECT_EQ(conn_handle.get_connection(), 1);
  }

  cwtest_advance_time_ms(1000 * TEST_MAX_IDLE_TIME_S + TEST_TIME_DELTA_MS);
  // The connection is now considered idle - retrieve and return a connection
  // for a different AddrInfo to trigger removal
  EXPECT_CALL(conn_pool, destroy_connection(1)).Times(1);
  conn_pool.get_connection(ai_2);

  // The connection should have been removed
  ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
  EXPECT_EQ(conn_handle.get_connection(), 2);
}
