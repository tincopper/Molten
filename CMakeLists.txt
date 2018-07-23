cmake_minimum_required(VERSION 3.10)
project(Molten C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(common)

add_executable(Molten
        common/molten_common.h
        common/molten_lock.c
        common/molten_lock.h
        common/molten_shm.c
        common/molten_shm.h
        common/molten_slog.c
        common/molten_slog.h
        common/molten_stack.c
        common/molten_stack.h
        common/cJSON.c
        common/cJSON.h
        molten_http_util.c
        molten_http_util.h
        molten.c
        molten_chain.c
        molten_chain.h
        molten_ctrl.c
        molten_ctrl.h
        molten_intercept.c
        molten_intercept.h
        molten_log.c
        molten_log.h
        molten_report.c
        molten_report.h
        molten_span.c
        molten_span.h
        molten_status.c
        molten_status.h
        molten_struct.h
        molten_util.c
        molten_util.h
        php7_wrapper.h
#        php_molten.h test_c/main.c
#        molten_http_util.c molten_http_util.h test_c/http_util_test.c
        )

#定义php源码路径，这里根据自己的真实路径来更改
set(PHP_SOURCE /opt/php-5.6.9)

#引入php需要的扩展源码，这里也是根据自己需要的来更改
include_directories(${PHP_SOURCE}/main)
include_directories(${PHP_SOURCE}/Zend)
include_directories(${PHP_SOURCE}/sapi)
include_directories(${PHP_SOURCE}/pear)
include_directories(${PHP_SOURCE}/TSRM)
include_directories(${PHP_SOURCE}/win32)
include_directories(${PHP_SOURCE}/netware)
include_directories(${PHP_SOURCE})

#include_directories(${PROJECT_SOURCE_DIR})

#add_custom_target(makefile COMMAND sudo ./usr/local/php7_debug/bin/phpize && ./configure --with-php-config=/usr/local/php7_debug/bin/php-config && make
#        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(makefile COMMAND sudo sh /usr/local/php_debug/bin/phpize
        && sudo ./configure --with-php-config=/usr/local/php_debug/bin/php-config
        && sudo make
        && sudo make install WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})