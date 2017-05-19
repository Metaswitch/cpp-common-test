/**
 * @file fakesimplestatsmanager.cpp
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef FAKESIMPLESTATSMANAGER_HPP__
#define FAKESIMPLESTATSMANAGER_HPP__

#include "gmock/gmock.h"
#include "fakesnmp.hpp"
#include "httpstack.h"

class FakeSimpleStatsManager : public HttpStack::StatsInterface
{
public:
  FakeSimpleStatsManager()
  {
    _latency_us = new SNMP::FakeEventAccumulatorTable();
    _incoming_requests = new SNMP::FakeCounterTable();
    _rejected_overload = new SNMP::FakeCounterTable();
  }
  
  virtual ~FakeSimpleStatsManager()
  {
    delete _latency_us;
    delete _incoming_requests;
    delete _rejected_overload;
  }

private:
  virtual void update_http_latency_us(unsigned long latency_us)
  {
    _latency_us->accumulate(latency_us);
  }
  virtual void incr_http_incoming_requests()
  {
    _incoming_requests->increment();
  }
  virtual void incr_http_rejected_overload()
  {
    _rejected_overload->increment();
  }

  SNMP::FakeEventAccumulatorTable* _latency_us;
  SNMP::FakeCounterTable* _incoming_requests;
  SNMP::FakeCounterTable* _rejected_overload;
};

#endif
