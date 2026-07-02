function(wm_add_library target_name)
    cmake_parse_arguments(ARG
        ""
        "MODULES_DIR"
        "MODULES;LINK_LIBS"
        ${ARGN}
    )

    set(srcs)
    set(private_modules)
    foreach(arg ${ARG_UNPARSED_ARGUMENTS})
        if(arg MATCHES "\.ixx$")
            list(APPEND private_modules ${arg})
        else()
            list(APPEND srcs ${arg})
        endif()
    endforeach()

    if(srcs)
        add_library(${target_name} OBJECT ${srcs})
    else()
        add_library(${target_name} OBJECT)
    endif()
    if(private_modules)
        target_sources(${target_name}
            PRIVATE
            FILE_SET private_modules_${target_name}
            TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES ${private_modules}
        )
    endif()

    if(ARG_MODULES)
        if(NOT DEFINED ARG_MODULES_DIR)
            set(ARG_MODULES_DIR "${CMAKE_SOURCE_DIR}/modules/wm/${target_name}/")
        endif()
        list(TRANSFORM ARG_MODULES
            PREPEND ${ARG_MODULES_DIR}/
            OUTPUT_VARIABLE module_files
        )
        target_sources(${target_name}
            PUBLIC
            FILE_SET public_modules_${target_name}
            TYPE CXX_MODULES
            BASE_DIRS ${ARG_MODULES_DIR}
            FILES ${module_files}
        )
    endif()

    if(ARG_LINK_LIBS)
        cmake_parse_arguments(LINK_LIBS_ARG
            ""
            ""
            "PUBLIC;PRIVATE"
            ${ARG_LINK_LIBS}
        )
        foreach(link_lib ${LINK_LIBS_ARG_PUBLIC})
            target_link_libraries(${target_name} PUBLIC ${link_lib})
        endforeach()
        foreach(link_lib ${LINK_LIBS_ARG_PRIVATE})
            target_link_libraries(${target_name} PRIVATE ${link_lib})
        endforeach()
    endif()
endfunction()
