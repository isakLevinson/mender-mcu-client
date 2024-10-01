idf_component_register(SRCS	"main.c"
       				"app.c"
				"cmd.c"
				"wifi.c"
				"time.c"
				"pump.c"
				"adc.c"
				"wss.c"
				"wss_keepalive.c"

				INCLUDE_DIRS "."

                EMBED_TXTFILES "certs/servercert.pem"
                               "certs/prvtkey.pem"
								"root.html"
								"upload_script.html"
				)

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format"  )
