

idf_component_register(SRCS 
        cli.c
        INCLUDE_DIRS "."
        REQUIRES main dbgterm services
)


target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format" )
