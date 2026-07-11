# Vendored IJG libjpeg 9f — decompression subset only (see README.md).
# jmem_esp.c replaces jmemnobs.c to route all library memory to PSRAM.
idf_component_register(
    SRCS
        "jmem_esp.c"
        "jpeg-9f/jaricom.c"
        "jpeg-9f/jcomapi.c"
        "jpeg-9f/jdapimin.c"
        "jpeg-9f/jdapistd.c"
        "jpeg-9f/jdarith.c"
        "jpeg-9f/jdatasrc.c"
        "jpeg-9f/jdcoefct.c"
        "jpeg-9f/jdcolor.c"
        "jpeg-9f/jddctmgr.c"
        "jpeg-9f/jdhuff.c"
        "jpeg-9f/jdinput.c"
        "jpeg-9f/jdmainct.c"
        "jpeg-9f/jdmarker.c"
        "jpeg-9f/jdmaster.c"
        "jpeg-9f/jdmerge.c"
        "jpeg-9f/jdpostct.c"
        "jpeg-9f/jdsample.c"
        "jpeg-9f/jerror.c"
        "jpeg-9f/jidctflt.c"
        "jpeg-9f/jidctfst.c"
        "jpeg-9f/jidctint.c"
        "jpeg-9f/jmemmgr.c"
        "jpeg-9f/jquant1.c"
        "jpeg-9f/jquant2.c"
        "jpeg-9f/jutils.c"
    INCLUDE_DIRS
        "jpeg-9f"
)

# 1994-era K&R-flavoured C — keep the vendored sources pristine and silence
# the warnings the project's -Werror CI build would otherwise trip on.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-shift-negative-value
    -Wno-implicit-fallthrough
)
