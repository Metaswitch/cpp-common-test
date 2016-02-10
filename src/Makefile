TEST_TARGETS := cpp_common_test_test

cpp_common_test_test_SOURCES := accesslogger.cpp \
                                accumulator.cpp \
                                alarm.cpp \
                                alarmdefinition.cpp \
                                base64.cpp \
                                base_communication_monitor.cpp \
                                baseresolver.cpp \
                                cassandra_store.cpp \
                                chronosconnection.cpp \
                                communicationmonitor.cpp \
                                counter.cpp \
                                diameterresolver.cpp \
                                diameterstack.cpp \
                                dnscachedresolver.cpp \
                                dnsparser.cpp \
                                exception_handler.cpp \
                                health_checker.cpp \
                                httpconnection.cpp \
                                httpresolver.cpp \
                                httpstack.cpp \
                                httpstack_utils.cpp \
                                ipv6utils.cpp \
                                json_alarms.cpp \
                                load_monitor.cpp \
                                localstore.cpp \
                                log.cpp \
                                logger.cpp \
                                memcached_config.cpp \
                                memcachedstore.cpp \
                                memcachedstoreview.cpp \
                                namespace_hop.cpp \
                                realmmanager.cpp \
                                saslogger.cpp \
                                signalhandler.cpp \
                                sip_string_to_request_type.cpp \
                                snmp_agent.cpp \
                                snmp_continuous_accumulator_table.cpp \
                                snmp_cx_counter_table.cpp \
                                snmp_event_accumulator_table.cpp \
                                snmp_infinite_base_table.cpp \
                                snmp_infinite_scalar_table.cpp \
                                snmp_infinite_timer_count_table.cpp \
                                snmp_row.cpp \
                                snmp_scalar.cpp \
                                snmp_single_count_by_node_type_table.cpp \
                                snmp_success_fail_count_table.cpp \
                                statistic.cpp \
                                timer_counter.cpp \
                                unique.cpp \
                                utils.cpp \
                                zmq_lvc.cpp \
                                fakecurl.cpp \
                                fakehttpconnection.cpp \
                                fakelogger.cpp \
                                fakezmq.cpp \
                                fakesnmp.cpp \
                                mock_sas.cpp \
                                mockfreediameter.cpp \
                                basetest.cpp \
                                test_main.cpp \
                                test_interposer.cpp \
                                alarm_test.cpp \
                                chronosconnection_test.cpp \
                                communicationmonitor_test.cpp \
                                diameterresolver_test.cpp \
                                diameterstack_test.cpp \
                                httpconnection_test.cpp \
                                httpstack_test.cpp \
                                httpstack_utils_test.cpp \
                                json_alarms_test.cpp \
                                logger_test.cpp \
                                load_monitor_test.cpp \
                                memcached_config_test.cpp \
                                memcachedstorereview_test.cpp \
                                realmmanager_test.cpp

cpp_common_test_test_CPPFLAGS := -Wno-write-strings \
                                 -I../usr/include \
                                 -I../modules/rapidjson/include \
                                 -I../modules/cpp-common/include \
                                 -Iut

cpp_common_test_test_LDFLAGS := -L../usr/lib \
                                -lthrift \
                                -lcassandra \
                                -lfdcore \
                                -lfdproto \
                                -lz \
                                -ldl \
                                -lpthread \
                                -lboost_regex \
                                -lzmq \
                                -lcares \
                                -levhtp \
                                -levent \
                                -levent_pthreads \
                                -lmemcached \
                                `net-snmp-config --netsnmp-agent-libs`

COMMON_COVERAGE_ROOT := ../modules/cpp-common/src

VPATH += ../modules/cpp-common/src ../modules/cpp-common/test_utils ut
include ../build-infra/cpp.mk

# Special extra objects for cpp_common_test
${BUILD_DIR}/bin/cpp_common_test_test : ${cpp_common_test_test_OBJECT_DIR}/test_interposer.so

# Build rule for our interposer.
${cpp_common_test_test_OBJECT_DIR}/test_interposer.so : ../modules/cpp-common/test_utils/test_interposer.cpp ../modules/cpp-common/test_utils/test_interposer.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(cpp_common_test_test_CPPFLAGS) -shared -fPIC -ldl $< -o $@
CLEANS += ${cpp_common_test_test_OBJECT_DIR}/test_interposer.so
