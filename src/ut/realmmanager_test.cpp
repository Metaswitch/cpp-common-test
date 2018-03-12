/**
 * @file realmmanager_test.cpp UT for Homestead Realm Manager module.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "realmmanager.h"
#include "mockdiameterstack.hpp"
#include "mockdiameterresolver.hpp"
#include "mockalarm.h"
#include "test_interposer.hpp"

using ::testing::SetArgReferee;
using ::testing::_;
using ::testing::Return;

/// Fixture for RealmmanagerTest.
class RealmmanagerTest : public testing::Test
{
public:
  static const std::string DIAMETER_REALM;
  static const std::string DIAMETER_HOSTNAME;

  // PDLog structures related to the alarm
  static PDLog _comm_restored_log;
  static PDLog1<const char*> _comm_error_log;

  static MockDiameterStack* _mock_stack;
  static MockDiameterResolver* _mock_resolver;
  static AlarmManager* _alarm_manager;
  static MockAlarm* _mock_alarm;

  static void SetUpTestCase()
  {
    _mock_stack = new MockDiameterStack();
    _mock_resolver = new MockDiameterResolver();
    _alarm_manager = new AlarmManager();
    _mock_alarm = new MockAlarm(_alarm_manager);
  }

  static void TearDownTestCase()
  {
    delete _mock_stack; _mock_stack = NULL;
    delete _mock_resolver; _mock_resolver = NULL;
    delete _mock_alarm; _mock_alarm = NULL;
    delete _alarm_manager; _alarm_manager = NULL;
  }

  RealmmanagerTest() {}
  ~RealmmanagerTest() {}

  void set_all_peers_connected(RealmManager* realm_manager)
  {
    for (std::map<std::string, Diameter::Peer*>::iterator ii = realm_manager->_peers.begin();
         ii != realm_manager->_peers.end();
         ii++)
    {
      realm_manager->peer_connection_cb(true,
                                        (ii->second)->host(),
                                        (ii->second)->realm());
      (ii->second)->_connected = true;
    }
  }

  AddrInfo create_peer(const char* ip_address)
  {
    AddrInfo peer;
    peer.transport = IPPROTO_TCP;
    peer.port = 3868;
    peer.address.af = AF_INET;
    inet_pton(AF_INET, ip_address, &peer.address.addr.ipv4);

    return peer;
  }
};

const std::string RealmmanagerTest::DIAMETER_REALM = "hss.example.com";
const std::string RealmmanagerTest::DIAMETER_HOSTNAME = "hss1.example.com";

PDLog RealmmanagerTest::_comm_restored_log = PDLog(1, LOG_INFO, "", "", "", "");
PDLog1<const char*> RealmmanagerTest::_comm_error_log = PDLog1<const char*>(2, LOG_ERR, "", "", "", "");

MockDiameterStack* RealmmanagerTest::_mock_stack = NULL;
MockDiameterResolver* RealmmanagerTest::_mock_resolver = NULL;
AlarmManager* RealmmanagerTest::_alarm_manager = NULL;
MockAlarm* RealmmanagerTest::_mock_alarm = NULL;

//
// ip_addr_to_arpa Tests
//

TEST_F(RealmmanagerTest, IPv4HostTest)
{
  IP46Address ip_addr;
  ip_addr.af = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &ip_addr.addr.ipv4);
  std::string expected_host = "127.0.0.1";
  std::string host = Utils::ip_addr_to_arpa(ip_addr);
  EXPECT_EQ(expected_host, host);
}

TEST_F(RealmmanagerTest, IPv6HostTest)
{
  IP46Address ip_addr;
  ip_addr.af = AF_INET6;
  inet_pton(AF_INET6, "2001:db8::1", &ip_addr.addr.ipv6);
  std::string expected_host = "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa";
  std::string host = Utils::ip_addr_to_arpa(ip_addr);
  EXPECT_EQ(expected_host, host);
}

TEST_F(RealmmanagerTest, IPv6HostTestLeading0s)
{
  IP46Address ip_addr;
  ip_addr.af = AF_INET6;
  inet_pton(AF_INET6, "::db6:1", &ip_addr.addr.ipv6);
  std::string expected_host = "1.0.0.0.6.b.d.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa";
  std::string host = Utils::ip_addr_to_arpa(ip_addr);
  EXPECT_EQ(expected_host, host);
}

TEST_F(RealmmanagerTest, IPv6HostTestTrailing0s)
{
  IP46Address ip_addr;
  ip_addr.af = AF_INET6;
  inet_pton(AF_INET6, "2001:db8::", &ip_addr.addr.ipv6);
  std::string expected_host = "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa";
  std::string host = Utils::ip_addr_to_arpa(ip_addr);
  EXPECT_EQ(expected_host, host);
}

// This tests that we can create and destroy a RealmManager object.
TEST_F(RealmmanagerTest, CreateDestroy)
{
  AddrInfo peer;
  peer.address.af = AF_INET;
  inet_pton(AF_INET, "1.1.1.1", &peer.address.addr.ipv4);
  std::vector<AddrInfo> targets;

  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  targets.push_back(peer);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, register_peer_hook_hdlr("realmmanager", _))
    .Times(1);
  EXPECT_CALL(*_mock_stack, register_rt_out_cb("realmmanager", _))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);
  realm_manager->start();

  // We have to sleep here to ensure that the main thread has been
  // created properly before we try and join to it during shutdown.
  sleep(1);

  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, unregister_peer_hook_hdlr("realmmanager"))
    .Times(1);
  EXPECT_CALL(*_mock_stack, unregister_rt_out_cb("realmmanager"))
    .Times(1);
  realm_manager->stop();

  delete realm_manager;
}

// This test alarm state change UNKNOWN->ALARMED as we fail to connect
// to our first peer right after startup.
TEST_F(RealmmanagerTest, TestAlarmStartup)
{
  AddrInfo peer = create_peer("1.1.1.1");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // The diameter resolver returns one peer. We expect to try and connect to it.
  targets.push_back(peer);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // We fail to connect to peer1. We expect the alarm to be raised since the
  // number of connected peers (1) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::UNKNOWN));
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false, "1.1.1.1", DIAMETER_REALM);

  delete realm_manager;
}

// This tests alarm state change CLEARED->ALARMED as we connect to a peer
// in unexpected realm resulting in the less than _max_peers connected peers.
TEST_F(RealmmanagerTest, TestAlarmRaisedBadRealm)
{
  /// SETUP
  // We get to the state of two connected peers with the alarm cleared.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  Diameter::Peer* peer2 = new Diameter::Peer("2.2.2.2", DIAMETER_REALM);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));
  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("2.2.2.2", peer2));

  peer1->_connected = true;
  peer2->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_clear_state);

  /// TEST
  // We fail to connect to peer1 by connecting to it in unexpected realm.
  // We expect the alarm to be raised since the number of connected peers (1)
  // is less than _max_peers. We remove peer1.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_CALL(*_mock_stack, remove(_)).Times(1);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(true, "1.1.1.1", "hss.badexample.com");

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}
/*
// This tests alarm state change CLEARED->ALARMED as we fail to connect
// to a peer resulting in less than _max_peers connected peers.
TEST_F(RealmmanagerTest, TestAlarmRaisedConnectionFailure)
{
  /// SETUP
  // We get to the state of two connected peers with the alarm cleared.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  Diameter::Peer* peer2 = new Diameter::Peer("2.2.2.2", DIAMETER_REALM);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));
  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("2.2.2.2", peer2));

  peer1->_connected = true;
  peer2->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_clear_state);

  /// TEST
  // We fail to connect to peer1. We expect the alarm to be raised since
  // the number of connected peers (1) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false, "1.1.1.1", DIAMETER_REALM);

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
  delete peer2;
}
*/
// This tests alarm state change ALARMED->CLEARED as we successfully connect
// to a peer resulting in at least _max_peers connected peers.
TEST_F(RealmmanagerTest, TestAlarmClearedAtLeastMaxPeers)
{
  /// SETUP
  // We get to the state of one connected peer with the alarm raised.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  AddrInfo addr_info1 = create_peer("1.1.1.1");
  AddrInfo addr_info2 = create_peer("2.2.2.2");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));

  peer1->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_set_state);

  /// TEST
  // The diameter resolver returns both peers. We expect to try to connect to peer2.
  targets.push_back(addr_info1);
  targets.push_back(addr_info2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We successfully connect to peer2. We expect the alarm to be cleared since
  // the number of connected peers (2) is at least _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);

  realm_manager->peer_connection_cb(true, "2.2.2.2", DIAMETER_REALM);

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
  delete peer1;
}

// This tests alarm state change ALARMED->CLEARED as we successfully connect
// to a peer resulting in no failed peers.
TEST_F(RealmmanagerTest, TestAlarmClearedNoFailedPeers)
{
  /// SETUP
  // We get to the state of one connected peer and one failed peer with
  // the alarm raised.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  AddrInfo addr_info1 = create_peer("1.1.1.1");
  AddrInfo addr_info2 = create_peer("2.2.2.2");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 3.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 3,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));
  realm_manager->_failed_peers.insert(std::pair<AddrInfo, unsigned long>(addr_info2, Utils::current_time_ms()));

  peer1->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_set_state);

  /// TEST
  // The diameter resolver returns both peers. We expect to try to connect to peer2.
  targets.push_back(addr_info1);
  targets.push_back(addr_info2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 3, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We now successfully connect to peer2. Although the number of connected peers (2)
  // is less than _max_peers, we expect the alarm to be cleared since there are no
  // failed peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);

  realm_manager->peer_connection_cb(true, "2.2.2.2", DIAMETER_REALM);

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 3, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
  delete peer1;
}

// This tests that the alarm stays raised as we fail to connect to a peer.
TEST_F(RealmmanagerTest, TestAlarmStayRaisedConnectionFailure)
{
  /// SETUP
  // We get to the state of no connected peers and one failed peer with the alarm raised.
  AddrInfo addr_info1 = create_peer("1.1.1.1");
  AddrInfo addr_info2 = create_peer("2.2.2.2");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, unsigned long>(addr_info1, 0));

  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_set_state);

  /// TEST
  // The diameter resolver returns peer2. We expect to try to connect to it.
  targets.push_back(addr_info2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We now fail connect to peer2. We expect the alarm to stay raised since the 
  // number of connected peers (0) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false, "2.2.2.2", DIAMETER_REALM);

  // We expect the alarm to be in the alarmed state.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_EQ(AlarmState::ALARMED, realm_manager->_peer_connection_alarm->get_alarm_state());

  delete realm_manager;
}

// This tests that the alarm stays raised as we successfully connect to a peer
// and there are less than _max_peers connected peers.
TEST_F(RealmmanagerTest, TestAlarmStayRaisedConnectionSuccess)
{
  /// SETUP
  // We get to the state of no connected peers and one failed peer with the alarm raised.
  AddrInfo addr_info1 = create_peer("1.1.1.1");
  AddrInfo addr_info2 = create_peer("2.2.2.2");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, unsigned long>(addr_info1, Utils::current_time_ms()));

  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_set_state);

  /// TEST
  // The diameter resolver returns peer2. We expect to try to connect to it.
  targets.push_back(addr_info2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We now successfully connect to peer2. We expect the alarm to stay raised since the 
  // number of connected peers (1) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(true, "2.2.2.2", DIAMETER_REALM);

  // We expect the alarm to be in the alarmed state.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_EQ(AlarmState::ALARMED, realm_manager->_peer_connection_alarm->get_alarm_state());

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}

// This test that the alarm stays cleared as we successfully
// connect to a new peer.
TEST_F(RealmmanagerTest, TestAlarmStayClearedConnectionSuccess)
{
  /// SETUP
  // We get to the state of one connected peer with the alarm cleared.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  AddrInfo addr_info2 = create_peer("2.2.2.2");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 1.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 1,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));

  peer1->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_clear_state);

  /// TEST
  // The diameter resolver returns peer2. We expect to try to connect to it.
  targets.push_back(addr_info2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 1, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We now successfully connect to peer2. We expect the alarm to stay cleared since the 
  // number of connected peers (2) is at least _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(true, "2.2.2.2", DIAMETER_REALM);

  // We expect the alarm to be in the cleared state.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_EQ(AlarmState::CLEARED, realm_manager->_peer_connection_alarm->get_alarm_state());

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 1, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
  delete peer1; 
}

// This test that the alarm stays cleared as we fail to connect to a peer
// and the number of connected peers is at least _max_peers.
TEST_F(RealmmanagerTest, TestAlarmStayClearedConnectionFailure)
{
  /// SETUP
  // We get to the state of two connected peers with the alarm cleared.
  Diameter::Peer* peer1 = new Diameter::Peer("1.1.1.1", DIAMETER_REALM);
  Diameter::Peer* peer2 = new Diameter::Peer("2.2.2.2", DIAMETER_REALM);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager with _max_peers set to 1.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 1,
                                                 _mock_resolver,
                                                 _mock_alarm,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("1.1.1.1", peer1));
  realm_manager->_peers.insert(std::pair<std::string, Diameter::Peer*>("2.2.2.2", peer2));

  peer1->_connected = true;
  peer2->_connected = true;
  realm_manager->_peer_connection_alarm->switch_to_state(&realm_manager->_peer_connection_alarm->_clear_state);

  /// TEST
  // We now fail to connect to peer2. We expect the alarm to stay cleared since the
  // number of connected peers (1) is at least _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false, "2.2.2.2", DIAMETER_REALM);

  // We expect the alarm to be in the cleared state.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_EQ(AlarmState::CLEARED, realm_manager->_peer_connection_alarm->get_alarm_state());

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 1, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
  delete peer1;
  delete peer2; 
}

// This tests the behaviour of the _failed_peers map as we attempt to connect
// to peers.
TEST_F(RealmmanagerTest, TestFailedPeersMap)
{
  AddrInfo peer1 = create_peer("1.1.1.1");
  AddrInfo peer2 = create_peer("2.2.2.2");
  AddrInfo peer3 = create_peer("3.3.3.3");
  AddrInfo peer4 = create_peer("4.4.4.4");
  std::vector<AddrInfo> targets;
  int ttl;

  // Add four peers to be returned by the diameter resolver.
  targets.push_back(peer1);
  targets.push_back(peer2);
  targets.push_back(peer3);
  targets.push_back(peer4);

  // Create RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // The diameter resolver returns the targets. 
  // We expect to try and connect to all of them.
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(4)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(4, 0))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  EXPECT_EQ(0, realm_manager->_failed_peers.size());

  // We fail to connect to peer1 and peer4 and successufully connect
  // to the other two peers.
  realm_manager->peer_connection_cb(false, "1.1.1.1", DIAMETER_REALM);
  realm_manager->peer_connection_cb(false, "4.4.4.4", DIAMETER_REALM);
  realm_manager->peer_connection_cb(true, "2.2.2.2", DIAMETER_REALM);
  realm_manager->peer_connection_cb(true, "3.3.3.3", DIAMETER_REALM);
 
  // We expect only peer1 and peer4 to be added to _failed_peers
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer1));
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer4));
  EXPECT_EQ(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer2));
  EXPECT_EQ(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer3));
  
  EXPECT_EQ(2, realm_manager->_failed_peers.size());

  // The diameter resolver returns the targets again. We expect to try and connect to peer1
  // and peer4.
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(4, 2))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // We now fail to connect to peer1, peer2 and peer3 and successfully
  // connect to peer4.
  realm_manager->peer_connection_cb(false, "1.1.1.1", DIAMETER_REALM);
  realm_manager->peer_connection_cb(false, "3.3.3.3", DIAMETER_REALM);
  realm_manager->peer_connection_cb(false, "2.2.2.2", DIAMETER_REALM);
  realm_manager->peer_connection_cb(true, "4.4.4.4", DIAMETER_REALM);

  // We expect peer1 to stay failed, peer2 and peer3 to be added to _failed_peers
  // and peer4 to be removed from _failed_peers.
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer1));
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer2));
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer3));
  EXPECT_EQ(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer4));

  EXPECT_EQ(3, realm_manager->_failed_peers.size());

  // We tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}

// This tests that we correctly remove old failed peers that are no longer
// being returned by the DNS.
TEST_F(RealmmanagerTest, TestRemoveOldFailedPeers)
{
  AddrInfo peer1 = create_peer("1.1.1.1");
  AddrInfo peer2 = create_peer("2.2.2.2");
  AddrInfo peer3 = create_peer("3.3.3.3");
  AddrInfo peer4 = create_peer("4.4.4.4");
  std::vector<AddrInfo> targets;
  int ttl;

  // Create RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // Add two failed peers
  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer1, Utils::current_time_ms()));
  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer3, Utils::current_time_ms()));

  // We simulate a FAILED_PEERS_TIMEOUT_MS amount of time passing
  // so that peers 1 and 3 timeout.
  cwtest_advance_time_ms(realm_manager->FAILED_PEERS_TIMEOUT_MS);

  // Add further two failed peers at a later time
  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer2, Utils::current_time_ms()));
  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer4, Utils::current_time_ms()));

  // We now call manage_connections expecting peers 1 and 3 to be removed from _failed_peers
  // as they were added more than FAILED_PEERS_TIMEOUT_MS ago.
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(0);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  EXPECT_EQ(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer1));
  EXPECT_EQ(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer3));
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer2));
  EXPECT_NE(realm_manager->_failed_peers.end(), realm_manager->_failed_peers.find(peer4));

  delete realm_manager;
}

// This tests that the _failed_peers map gets converted correctly to a CSV
// of IP addresses.
TEST_F(RealmmanagerTest, CreateFailedPeersString)
{
  AddrInfo peer1 = create_peer("1.1.1.1");
  AddrInfo peer2 = create_peer("2.2.2.2");

  // Create a RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer1, 1));
  EXPECT_EQ("1.1.1.1", realm_manager->create_failed_peers_string());

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer2, 2));
  EXPECT_EQ("1.1.1.1, 2.2.2.2", realm_manager->create_failed_peers_string());

  delete realm_manager;
}

// This tests that the RealmManager's manage_connections function
// behaves correctly when the DiameterResolver returns various
// combinations of peers.
TEST_F(RealmmanagerTest, ManageConnections)
{
  // Set up some AddrInfo structures for the diameter resolver
  // to return.
  AddrInfo peer1;
  peer1.transport = IPPROTO_TCP;
  peer1.port = 3868;
  peer1.address.af = AF_INET;
  inet_pton(AF_INET, "1.1.1.1", &peer1.address.addr.ipv4);
  AddrInfo peer2;
  peer2.transport = IPPROTO_TCP;
  peer2.port = 3868;
  peer2.address.af = AF_INET;
  peer2.priority = 1;
  inet_pton(AF_INET, "2.2.2.2", &peer2.address.addr.ipv4);
  AddrInfo peer3;
  peer3.transport = IPPROTO_TCP;
  peer3.port = 3868;
  peer3.address.af = AF_INET;
  inet_pton(AF_INET, "3.3.3.3", &peer3.address.addr.ipv4);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // First run through. The diameter resolver returns two peers. We
  // expect to try and connect to them.
  targets.push_back(peer1);
  targets.push_back(peer2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);
  EXPECT_EQ(15, ttl);

  // The connection to peer1 fails. Set the connected flag on the
  // remaining peers. This should just be peer2.
  realm_manager->peer_connection_cb(false,
                                    "1.1.1.1",
                                    DIAMETER_REALM);
  set_all_peers_connected(realm_manager);

  // The diameter resolver returns the peer we're already connected to
  // and a new peer. We expect to try and connect to the new peer.
  targets.clear();
  targets.push_back(peer2);
  targets.push_back(peer3);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(10)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);
  EXPECT_EQ(10, ttl);

  // Set the connected flag on the new peer.
  set_all_peers_connected(realm_manager);

  // The diameter resolver returns just one peer, and the priority of that peer
  // has changed. We expect to tear down one of the connections, and the new
  // priority to have been saved off correctly.
  targets.clear();
  peer2.priority = 2;
  targets.push_back(peer2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(1, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);
  EXPECT_EQ(realm_manager->_peers.find("2.2.2.2")->second->addr_info().priority, 2);

  // The diameter resolver returns two peers again. We expect to try and
  // reconnect to peer3. However, freeDiameter says we're already connected
  // to peer3, so it doesn't get added to the list of _peers.
  targets.clear();
  targets.push_back(peer2);
  targets.push_back(peer3);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // The RealmManager gets told that an unknown peer has connected. It ignores
  // this.
  realm_manager->peer_connection_cb(true,
                                    "9.9.9.9",
                                    DIAMETER_REALM);

  // The diameter resolver returns two peers again. We expect to try and
  // reconnect to peer3.
  targets.clear();
  targets.push_back(peer2);
  targets.push_back(peer3);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // However, this time peer3 reports that he's in an unexpected realm. We
  // remove it.
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  realm_manager->peer_connection_cb(true,
                                    "3.3.3.3",
                                    "hss.badexample.com");

  // The diameter resolver returns no peers. We expect to tear down the one
  // connection (to peer2) that we have up.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(1);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}

// This tests that the SRV priority callback works.
TEST_F(RealmmanagerTest, SRVPriority)
{
  // Set up some AddrInfo structures for the diameter resolver
  // to return.
  AddrInfo peer1;
  peer1.transport = IPPROTO_TCP;
  peer1.port = 3868;
  peer1.priority = 1;
  peer1.address.af = AF_INET;
  inet_pton(AF_INET, "1.1.1.1", &peer1.address.addr.ipv4);
  AddrInfo peer2;
  peer2.transport = IPPROTO_TCP;
  peer2.port = 3868;
  peer2.priority = 2;
  peer2.address.af = AF_INET;
  inet_pton(AF_INET, "2.2.2.2", &peer2.address.addr.ipv4);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // The diameter resolver returns two peers. We successfully connect to both of
  // them.
  targets.push_back(peer1);
  targets.push_back(peer2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);
  set_all_peers_connected(realm_manager);

  // Create a list of candidates and call the SRV priority callback. candidate1
  // and candidate2 are real peers - check that their scores are adjusted
  // correctly. candidate3 is not a real peer - check its score remains the
  // same.
  struct fd_list candidates;
  fd_list_init(&candidates, NULL);
  struct rtd_candidate candidate1;
  candidate1.cfg_diamid = const_cast<char*>("1.1.1.1");
  candidate1.score = 50;
  fd_list_init(&candidate1.chain, &candidate1);
  fd_list_insert_after(&candidates, &candidate1.chain);
  struct rtd_candidate candidate2;
  candidate2.cfg_diamid = const_cast<char*>("2.2.2.2");
  candidate2.score = 50;
  fd_list_init(&candidate2.chain, &candidate2);
  fd_list_insert_after(&candidates, &candidate2.chain);
  struct rtd_candidate candidate3;
  candidate3.cfg_diamid = const_cast<char*>("9.9.9.9");
  candidate3.score = 50;
  fd_list_init(&candidate3.chain, &candidate3);
  fd_list_insert_after(&candidates, &candidate3.chain);

  realm_manager->srv_priority_cb(&candidates);

  EXPECT_EQ(candidate1.score, 49);
  EXPECT_EQ(candidate2.score, 48);
  EXPECT_EQ(candidate3.score, 50);


  // Tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}


// This tests that the SRV priority callback works for negative priorities.
TEST_F(RealmmanagerTest, SRVPriorityNegative)
{
  // Set up some AddrInfo structures for the diameter resolver
  // to return.
  AddrInfo peer1;
  peer1.transport = IPPROTO_TCP;
  peer1.port = 3868;
  peer1.priority = 65535;
  peer1.address.af = AF_INET;
  inet_pton(AF_INET, "1.1.1.1", &peer1.address.addr.ipv4);
  AddrInfo peer2;
  peer2.transport = IPPROTO_TCP;
  peer2.port = 3868;
  peer2.priority = 2;
  peer2.address.af = AF_INET;
  inet_pton(AF_INET, "2.2.2.2", &peer2.address.addr.ipv4);
  std::vector<AddrInfo> targets;
  int ttl;

  // Create a RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 NULL,
                                                 _comm_restored_log,
                                                 _comm_error_log);

  // The diameter resolver returns two peers. We successfully connect to both of
  // them.
  targets.push_back(peer1);
  targets.push_back(peer2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);
  set_all_peers_connected(realm_manager);

  // Create a list of candidates and call the SRV priority callback.
  //
  // candidate1 is very low priority - but this shouldn't cause a negative score
  // candidate2 has a negative score - this should not be changed.
  struct fd_list candidates;
  fd_list_init(&candidates, NULL);
  struct rtd_candidate candidate1;
  candidate1.cfg_diamid = const_cast<char*>("1.1.1.1");
  candidate1.score = 50;
  fd_list_init(&candidate1.chain, &candidate1);
  fd_list_insert_after(&candidates, &candidate1.chain);
  struct rtd_candidate candidate2;
  candidate2.cfg_diamid = const_cast<char*>("2.2.2.2");
  candidate2.score = -1;
  fd_list_init(&candidate2.chain, &candidate2);
  fd_list_insert_after(&candidates, &candidate2.chain);

  realm_manager->srv_priority_cb(&candidates);

  EXPECT_EQ(candidate1.score, 1);
  EXPECT_EQ(candidate2.score, -1);


  // Tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  delete realm_manager;
}
