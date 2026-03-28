if(NOT DEFINED DEPLOY_DIR OR DEPLOY_DIR STREQUAL "")
    message(FATAL_ERROR "DEPLOY_DIR is required")
endif()

if(NOT EXISTS "${DEPLOY_DIR}")
    file(MAKE_DIRECTORY "${DEPLOY_DIR}")
    return()
endif()

set(PRESERVED_ENTRIES "logs")
if(DEFINED PRESERVE_ENTRIES AND NOT PRESERVE_ENTRIES STREQUAL "")
    list(APPEND PRESERVED_ENTRIES ${PRESERVE_ENTRIES})
endif()

file(GLOB children RELATIVE "${DEPLOY_DIR}" "${DEPLOY_DIR}/*")
foreach(child IN LISTS children)
    list(FIND PRESERVED_ENTRIES "${child}" preserved_index)
    if(NOT preserved_index EQUAL -1)
        continue()
    endif()

    file(REMOVE_RECURSE "${DEPLOY_DIR}/${child}")
endforeach()
