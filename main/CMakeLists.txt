idf_component_register(SRCS	"main.c" 
				"adc.c"
				"cmd.c"
				"cmd_ble.c"
				"gatt_svr.c"
				"wifi.c"
				"app.c"

				INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format"  )
