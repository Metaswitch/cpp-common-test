TEST_TARGETS := cpp_common_test_test

cpp_common_test_test_SOURCES := \
                                dnscachedresolver.cpp \
                                static_dns_cache.cpp \
                                dnsparser.cpp \
                                log.cpp \
                                logger.cpp \
                                saslogger.cpp \
                                signalhandler.cpp \
                                static_dns_cache.cpp \
                                unique.cpp \
                                utils.cpp \
                                xml_utils.cpp \
                                fakelogger.cpp \
                                mock_sas.cpp \
                                pthread_cond_var_helper.cpp \
                                basetest.cpp \
                                test_main.cpp \
                                dnscachedresolver_test.cpp \
                                staticdns_test.cpp \
                                test_interposer.cpp

cpp_common_test_test_CPPFLAGS := -Wno-write-strings \
                                 -I../usr/include \
                                 -I../modules/rapidjson/include \
                                 -I../modules/cpp-common/include \
                                 -Iut

cpp_common_test_test_LDFLAGS := -L../usr/lib \
                                -lz \
                                -ldl \
                                -lpthread \
                                -lboost_regex \
                                -lcares

COVERAGE_ROOT := ../modules/cpp-common/src

cpp_common_test_test_VALGRIND_ARGS := --suppressions=ut/cpp_common_test.supp

VPATH += ../modules/cpp-common/src ../modules/cpp-common/test_utils ut
include ../build-infra/cpp.mk
