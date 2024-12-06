
if (CONFIG_BUILD_TYPE_PNU)
	set(SPECIFIC_SRC "pump.c" "app.c")
endif()

if (CONFIG_BUILD_TYPE_GSR)
	set(SPECIFIC_SRC "")
endif()

message(SPECIFIC_SRC = "${SPECIFIC_SRC}")

idf_component_register(
				SRCS
				${SPECIFIC_SRC}
				"main.c"
				"cmd.c"
				"wifi.c"
				"time.c"
				"adc.c"
				"wss.c"
				"wss_keepalive.c"
				"led.c"
				"nvs.c"
				"config.c"
				"ota.c"

				INCLUDE_DIRS "."

                EMBED_TXTFILES "${project_dir}/certs/servercert.pem"
                               "${project_dir}/certs/prvtkey.pem"
			       "${project_dir}/ota_server_certs/ca_cert.pem"
				)

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format"  )
