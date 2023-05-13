idf_component_register(SRCS	"main.c" 
				"cmd.c"
				"cmd_ble.c"
				"gatt_svr.c"
				"wifi.c"
				"adc_spi.c"

				INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format"  )
