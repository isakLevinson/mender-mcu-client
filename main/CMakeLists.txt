
if (CONFIG_BUILD_TYPE_ESP)
	set(SPECIFIC_SRC "mmi.c")
endif()

if (CONFIG_BUILD_TYPE_PNU)
	set(SPECIFIC_SRC "ctrl.c" "pump.c" "max17049.c" "adc.c" "mmi.c")
endif()

if (CONFIG_BUILD_TYPE_GSR)
	set(SPECIFIC_SRC "max30001.c" "adc.c" "mmi.c")
endif()

if (CONFIG_BUILD_TYPE_EEG)
	set(SPECIFIC_SRC "")
endif()

message(SPECIFIC_SRC = "${SPECIFIC_SRC}")

idf_component_register(
				SRCS
				${SPECIFIC_SRC}
				"main.c"
				"cmd.c"
				"wifi.c"
				"ntp.c"
				"time.c"
				"tls.c"
				"httpd.c"
				"http_keepalive.c"
				"nvs.c"
				"config.c"
				"factory.c"
#				"ota.c"
				"mender_ota.c"

				INCLUDE_DIRS "."

                EMBED_TXTFILES	"${project_dir}/certs/server.crt"
								"${project_dir}/certs/server.key"
								"${project_dir}/ota_server_certs/ca_cert.pem"
				)

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format")

