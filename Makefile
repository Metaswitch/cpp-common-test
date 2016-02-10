all: test

ROOT ?= ${PWD}
MK_DIR := ${ROOT}/mk
PREFIX ?= ${ROOT}/usr
INSTALL_DIR ?= ${PREFIX}
MODULE_DIR := ${ROOT}/modules
INCLUDE_DIR := ${INSTALL_DIR}/include
LIB_DIR := ${INSTALL_DIR}/lib

SUBMODULES := libevhtp libmemcached thrift cassandra freeDiameter sas-client

include $(patsubst %, ${MK_DIR}/%.mk, ${SUBMODULES})
include ${MK_DIR}/cpp-common-test.mk

build: ${SUBMODULES} cpp_common_test

test: ${SUBMODULES} cpp_common_test

full_test: ${SUBMODULES} cpp_common_test_full_test

testall: $(patsubst %, %_test, ${SUBMODULES}) full_test

clean: $(patsubst %, %_clean, ${SUBMODULES}) cpp_common_test_clean
	rm -rf ${ROOT}/usr
	rm -rf ${ROOT}/build

distclean: $(patsubst %, %_distclean, ${SUBMODULES}) cpp_common_test_distclean
	rm -rf ${ROOT}/usr
	rm -rf ${ROOT}/build

include build-infra/cw-deb.mk
