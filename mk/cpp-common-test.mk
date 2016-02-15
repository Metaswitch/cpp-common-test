CPP_COMMON_TEST_DIR := ${ROOT}/src

cpp_common_test:
	@echo "No build for cpp-common-test"

cpp_common_test_test:
	${MAKE} -C ${CPP_COMMON_TEST_DIR} test

cpp_common_test_full_test:
	${MAKE} -C ${CPP_COMMON_TEST_DIR} full_test

cpp_common_test_clean:
	${MAKE} -C ${CPP_COMMON_TEST_DIR} clean

cpp_common_test_distclean: cpp_common_test_clean

.PHONY: cpp_common_test cpp_common_test_test cpp_common_test_clean cpp_common_test_distclean
