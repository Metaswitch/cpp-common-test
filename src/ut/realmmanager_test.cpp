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

using ::testing::SetArgReferee;
using ::testing::_;
using ::testing::Return;

/// Fixture for RealmmanagerTest.
class RealmmanagerTest : public testing::Test
{
public:
  static const std::string DIAMETER_REALM;
  static const std::string DIAMETER_HOSTNAME;

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
};

const std::string RealmmanagerTest::DIAMETER_REALM = "hss.example.com";
const std::string RealmmanagerTest::DIAMETER_HOSTNAME = "hss1.example.com";

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

// Test that the _failed_peers map gets converted correctly to a csv of IP addresses
TEST_F(RealmmanagerTest, CreateFailedPeersString)
{
  // Set up some AddrInfo structures
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

  // Create a RealmManager.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver);

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer1, 1));
  EXPECT_EQ("1.1.1.1", realm_manager->create_failed_peers_string());

  realm_manager->_failed_peers.insert(std::pair<AddrInfo, const unsigned long>(peer2, 2));
  EXPECT_EQ("1.1.1.1, 2.2.2.2", realm_manager->create_failed_peers_string());

  delete realm_manager;
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
                                                 _mock_resolver);

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
                                                 _mock_resolver);

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
                                                 _mock_resolver);

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
                                                 _mock_resolver);

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

TEST_F(RealmmanagerTest, PeerConnectionAlarm)
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

  // Create a RealmManager with _max_peers set to 2.
  RealmManager* realm_manager = new RealmManager(_mock_stack,
                                                 DIAMETER_REALM,
                                                 DIAMETER_HOSTNAME,
                                                 2,
                                                 _mock_resolver,
                                                 _mock_alarm);

  // First run through. The diameter resolver returns one peer.
  // We expect to try and connect to it.
  targets.push_back(peer1);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);
  
  realm_manager->manage_connections(ttl);

  // We fail to connect to peer1. Alarm should get raised since the number of 
  // connected peers (0) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::UNKNOWN));
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false,
                                    "1.1.1.1",
                                    DIAMETER_REALM);

  // peer1 should have been added to _failed_peers
  EXPECT_EQ(1, realm_manager->_failed_peers.size());

  // The diameter resolver returns the same peer again. We
  // expect to try and connect to it.
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(1, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);
  
  // We now succeed in connecting to peer1. Although number of connected
  // peers is less than _max_peers, we expect the alarm to be cleared since
  // there are no failed peers anymore.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);

  realm_manager->peer_connection_cb(true,
                                    "1.1.1.1",
                                    DIAMETER_REALM);

  // Check that _failed_peers is empty
  EXPECT_EQ(0, realm_manager->_failed_peers.size());

  // The diameter resolver now returns the peer we are connected to and a 
  // new peer. We expect to try and connect to the new peer.
  targets.push_back(peer2);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(1)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(2, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // We now fail to connect to peer2 by connecting to it in unexpected realm.
  // We expect the alarm to be raised as the number of peers we are connected 
  // to (1) is less than _max_peers. We remove peer2.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::CLEARED));
  EXPECT_CALL(*_mock_stack, remove(_)).Times(1);
  EXPECT_CALL(*_mock_alarm, set()).Times(1);
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(true,
                                    "2.2.2.2",
                                    "hss.badexample.com");

  // We expect peer2 to be added to failed peers.
  EXPECT_EQ(1, realm_manager->_failed_peers.size());

  // The diameter resolver now returns both peers again and a new peer. We expect to 
  // try to connect only to peer2 and peer3 since we are already connected to peer1.
  targets.push_back(peer3);
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, add(_))
    .Times(2)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*_mock_stack, peer_count(3, 1))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // We fail to connect to peer3. Alarm should stay raised since the number of peers
  // we are currently connected to (1) is less than _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, clear()).Times(0);

  realm_manager->peer_connection_cb(false,
                                    "3.3.3.3",
                                    DIAMETER_REALM);

  // We expect peer3 to be added to _failed_peers
  EXPECT_EQ(2, realm_manager->_failed_peers.size());

  // We succeed in connecting to peer2. We expect the alarm to be cleared since 
  // the number of connected peers (2) is at least _max_peers.
  EXPECT_CALL(*_mock_alarm, get_alarm_state())
    .Times(1)
    .WillOnce(Return(AlarmState::ALARMED));
  EXPECT_CALL(*_mock_alarm, set()).Times(0);
  EXPECT_CALL(*_mock_alarm, clear()).Times(1);

  realm_manager->peer_connection_cb(true,
                                    "2.2.2.2",
                                    DIAMETER_REALM);

  // Check that peer2 was removed from _failed_peers.
  EXPECT_EQ(1, realm_manager->_failed_peers.size());

  // Finally, we set the _failed_peers_timeout_ms parameter to 0ms. We expect peer3
  // to be removed from the list of failed peers when we call manage_connections
  realm_manager->_failed_peers_timeout_ms = 0;

  // We also tidy up by having the resolver return no peers so that the RealmManager
  // tears down it's connections.
  targets.clear();
  EXPECT_CALL(*_mock_resolver, resolve(DIAMETER_REALM, DIAMETER_HOSTNAME, 2, _, _))
    .WillOnce(DoAll(SetArgReferee<3>(targets), SetArgReferee<4>(15)));
  EXPECT_CALL(*_mock_stack, remove(_))
    .Times(2);
  EXPECT_CALL(*_mock_stack, peer_count(0, 0))
    .Times(1);

  realm_manager->manage_connections(ttl);

  // Check that peer3 was removed
  EXPECT_EQ(0, realm_manager->_failed_peers.size());

  delete realm_manager;
}
