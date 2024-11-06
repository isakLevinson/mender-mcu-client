idf_component_register(SRCS	"main.c"
       				"app.c"
				"cmd.c"
				"wifi.c"
				"time.c"
				"pump.c"
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
