idf_component_register(SRCS	"main.c"
       				"app.c"
				"cmd.c"
				"cmd_ble.c"
				"gatt_svr.c"
				"wifi.c"
				"spi.c"
				"time.c"
				"pump.c"
				"adc.c"

				INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format"  )
