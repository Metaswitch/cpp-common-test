/**
 * @file connectionpool_test.cpp UT for connection pooling classes in
 * connectionpool.h
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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_interposer.hpp"

#include "testable_connection_pool.h"

using ::testing::Return;
using ::testing::_;

class ConnectionPoolTest : public ::testing::Test
{
public:
  static const int TEST_MAX_IDLE_TIME_S = 60;
  static const int TEST_TIME_DELTA_MS = 500;

  ConnectionPoolTest();
  ~ConnectionPoolTest();

  TestableConnectionPool conn_pool;
  AddrInfo ai_1;
  AddrInfo ai_2;
};

ConnectionPoolTest::ConnectionPoolTest() : conn_pool(TestableConnectionPool(TEST_MAX_IDLE_TIME_S))
{
  cwtest_completely_control_time();

  // Create two AddrInfo objects to use for indexing into the pool
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
  // Check that the create connection  method is called correctly
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(1).WillOnce(Return(1));
  ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);

  // Check that the connection has the correct parameters
  EXPECT_EQ(conn_handle.get_connection(), 1);
  EXPECT_EQ(conn_handle.get_target(), ai_1);

  // Check that the connection is destroyed when the conn_pool destructor is
  // called as conn_pool drops out of scope
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
}

// Test that a connection is removed from the pool when selected by
// get_connection
TEST_F(ConnectionPoolTest, ConnectionRemovedFromPool)
{
  // Check that the create connection method is called correctly
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(2).WillOnce(Return(1)).WillOnce(Return(2));

  // A connection is created and retrieved
  ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
  // Another connection is requested
  ConnectionHandle<int> conn_handle_2 = conn_pool.get_connection(ai_1);
  // The second call should return a different connection from the first
  EXPECT_EQ(conn_handle_2.get_connection(), 2);

  // Check that the connections are correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 2)).Times(1);
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

  // Check that the connection is correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
}

// Test that connections are destroyed on destruction of a handle when the
// 'return to pool' flag is false
TEST_F(ConnectionPoolTest, ConnectionDestroyOnRelease)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);

  // Create a connection handle, set the 'return to pool' flag to false, and let
  // the handle fall out of scope
  {
    ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
    conn_handle.set_return_to_pool(false);
  }
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

  // Check that the connections are correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
  EXPECT_CALL(conn_pool, destroy_connection(ai_2, 2)).Times(1);
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
  // for a different AddrInfo to trigger removal, and check that the connection
  // is correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
  conn_pool.get_connection(ai_2);

  // The connection should have been removed
  ConnectionHandle<int> conn_handle = conn_pool.get_connection(ai_1);
  EXPECT_EQ(conn_handle.get_connection(), 2);

  // Check that the connections are correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 2)).Times(1);
  EXPECT_CALL(conn_pool, destroy_connection(ai_2, 3)).Times(1);
}

TEST_F(ConnectionPoolTest, MoveConnectionHandle)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(2).WillOnce(Return(1)).WillOnce(Return(2));

  // Create a connection handle, move it, and let both drop out of scope. The
  // connection should only be returned to the pool once.
  {
    ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
    ConnectionHandle<int> conn_handle_2 = std::move(conn_handle_1);
  }

  // The first call should retrieve the existing connection
  ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
  EXPECT_EQ(conn_handle_1.get_connection(), 1);

  // The second call should create a new connection
  EXPECT_EQ(conn_pool.get_connection(ai_1).get_connection(), 2);

  // Check that the connections are correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 2)).Times(1);
}

// Tests that the _free_on_error property correctly destroys other connections
// to the same target when one is not returned to the pool
TEST_F(ConnectionPoolTest, FreeOnError)
{
  EXPECT_CALL(conn_pool, create_connection(ai_1)).Times(3)
    .WillOnce(Return(1)).WillOnce(Return(2)).WillOnce(Return(3));

  EXPECT_CALL(conn_pool, create_connection(ai_2)).Times(1).WillOnce(Return(11));

  // Set the pool to free all connections to the same target on error
  conn_pool.set_free_on_error(true);

  {
    // Create a connection that won't be returned on release, but don't release
    // it yet
    ConnectionHandle<int> conn_handle_1 = conn_pool.get_connection(ai_1);
    conn_handle_1.set_return_to_pool(false);

    // Create a connection, let it drop out of scope so it's returned to the pool
    {
      ConnectionHandle<int> conn_handle_2 = conn_pool.get_connection(ai_1);
    }

    // Confirm the connection is still in the pool by getting a connection; it
    // should be the one that was just returned to the pool.
    {
      ConnectionHandle<int> conn_handle_3 = conn_pool.get_connection(ai_1);
      EXPECT_EQ(conn_handle_3.get_connection(), 2);
    }

    // Create a connection to a different target, should trigger creation of
    // connection "11"
    ConnectionHandle<int> conn_handle_target2_1 = conn_pool.get_connection(ai_2);
    EXPECT_EQ(conn_handle_target2_1.get_connection(), 11);

    // Now connection "1" is released and not returned to the pool, which should
    // trigger "2" to be destroyed as well
    EXPECT_CALL(conn_pool, destroy_connection(ai_1, 1)).Times(1);
    EXPECT_CALL(conn_pool, destroy_connection(ai_1, 2)).Times(1);
  }

  // The pool should be empty, so this call triggers creation of a new one
  ConnectionHandle<int> conn_handle_4 = conn_pool.get_connection(ai_1);
  EXPECT_EQ(conn_handle_4.get_connection(), 3);

  // Connection "11" should still be in the pool
  ConnectionHandle<int> conn_handle_target2_2 = conn_pool.get_connection(ai_2);
  EXPECT_EQ(conn_handle_target2_2.get_connection(), 11);

  // Check that the connections are correctly destroyed
  EXPECT_CALL(conn_pool, destroy_connection(ai_1, 3)).Times(1);
  EXPECT_CALL(conn_pool, destroy_connection(ai_2, 11)).Times(1);
}
