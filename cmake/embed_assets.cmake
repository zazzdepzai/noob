function(embed_file src out_var out_size_var)
    if (NOT EXISTS ${src})
        set(${out_var} "0,0,0,0" PARENT_SCOPE)
        set(${out_size_var} 4 PARENT_SCOPE)
        return()
    endif()
    file(READ ${src} hex HEX)
    string(LENGTH "${hex}" hex_len)
    math(EXPR num_bytes "${hex_len} / 2")
    set(arr "")
    set(i 0)
    while(i LESS num_bytes)
        math(EXPR pos "${i} * 2")
        string(SUBSTRING "${hex}" ${pos} 2 byte)
        string(APPEND arr "0x${byte},")
        math(EXPR i "${i} + 1")
        math(EXPR mod32 "${i} % 32")
        if (mod32 EQUAL 0)
            string(APPEND arr "\n")
        endif()
    endwhile()
    set(${out_var} "${arr}" PARENT_SCOPE)
    set(${out_size_var} ${num_bytes} PARENT_SCOPE)
endfunction()

embed_file(${LOGO}   LOGO_ARR   LOGO_SIZE)
embed_file(${BG}     BG_ARR     BG_SIZE)
embed_file(${BANNER} BANNER_ARR BANNER_SIZE)

file(WRITE ${OUT}
"#pragma once\n"
"static const unsigned char RAVENXD_LOGO_PNG[]   = {\n${LOGO_ARR}\n};\n"
"static const unsigned int  RAVENXD_LOGO_PNG_SIZE   = ${LOGO_SIZE};\n"
"static const unsigned char RAVENXD_BG_PNG[]     = {\n${BG_ARR}\n};\n"
"static const unsigned int  RAVENXD_BG_PNG_SIZE     = ${BG_SIZE};\n"
"static const unsigned char RAVENXD_BANNER_PNG[] = {\n${BANNER_ARR}\n};\n"
"static const unsigned int  RAVENXD_BANNER_PNG_SIZE = ${BANNER_SIZE};\n")
