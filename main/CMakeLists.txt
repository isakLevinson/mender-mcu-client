idf_component_register(SRCS "main.c" "cmd_pwm.c"
                            "cmd_ble.c"
                            "gatt_svr.c"
                            "cmd_wifi.c"
                            
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
