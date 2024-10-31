

idf_component_register(SRCS 
        cJSON.c
        INCLUDE_DIRS "."
)


target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format" )
