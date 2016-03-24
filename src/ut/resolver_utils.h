/**
 * @file baseresolver_test.cpp UT for BaseResolver class.
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

#ifndef RESOLVER_UTILS_H__
#define RESOLVER_UTILS_H__

#include <functional>
#include <algorithm>
#include <string>

using namespace std;

namespace ResolverUtils
{
inline std::string addrinfo_to_string(const AddrInfo& ai)
{
  ostringstream oss;
  char buf[100];
  if (ai.address.af == AF_INET6)
  {
    oss << "[";
  }
  oss << inet_ntop(ai.address.af, &ai.address.addr, buf, sizeof(buf));
  if (ai.address.af == AF_INET6)
  {
    oss << "]";
  }
  oss << ":" << ai.port;
  oss << ";transport=";
  if (ai.transport == IPPROTO_SCTP)
  {
    oss << "SCTP";
  }
  else if (ai.transport == IPPROTO_TCP)
  {
    oss << "TCP";
  }
  else
  {
    oss << "Unknown (" << ai.transport << ")";
  }
  return oss.str();
}

inline DnsRRecord* a(const std::string& name,
              int ttl,
              const std::string& address)
{
  struct in_addr addr;
  inet_pton(AF_INET, address.c_str(), &addr);
  return (DnsRRecord*)new DnsARecord(name, ttl, addr);
}

inline DnsRRecord* aaaa(const std::string& name,
                 int ttl,
                 const std::string& address)
{
  struct in6_addr addr;
  inet_pton(AF_INET6, address.c_str(), &addr);
  return (DnsRRecord*)new DnsAAAARecord(name, ttl, addr);
}

inline DnsRRecord* srv(const std::string& name,
                int ttl,
                int priority,
                int weight,
                int port,
                const std::string& target)
{
  return (DnsRRecord*)new DnsSrvRecord(name, ttl, priority, weight, port, target);
}

inline DnsRRecord* naptr(const std::string& name,
                  int ttl,
                  int order,
                  int preference,
                  const std::string& flags,
                  const std::string& service,
                  const std::string& regex,
                  const std::string& replacement)
{
  return (DnsRRecord*)new DnsNaptrRecord(name, ttl, order, preference, flags,
                                         service, regex, replacement);
}
}

#endif
