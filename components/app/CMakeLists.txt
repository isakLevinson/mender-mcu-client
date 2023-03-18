

idf_component_register(SRCS 
        cli.c
        INCLUDE_DIRS "."
        REQUIRES dbgterm services
)


target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format" "-Wno-missing-braces" "-Wno-error-missing-braces" )
