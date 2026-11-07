execute_process(COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=C
    "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}" --config "${CONFIG}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(MAKE_DIRECTORY "${BUILD_DIR}/test_logs")
file(WRITE "${BUILD_DIR}/test_logs/${TARGET}.log" "${output}\n${error}")
if(result EQUAL 0)
    message(FATAL_ERROR "${TARGET} unexpectedly compiled")
endif()
string(FIND "${output}\n${error}" "${MARKER}" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Expected '${MARKER}' in diagnostic:\n${output}\n${error}")
endif()
