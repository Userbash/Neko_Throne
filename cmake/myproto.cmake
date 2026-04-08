add_subdirectory(3rdparty/simple-protobuf)

# ─── Windows compatibility patch for spb-protoc ───────────────────────────────
# On Windows, the MSVC CRT headers mark strerror() as deprecated
# (_CRT_INSECURE_DEPRECATE). When building with Clang (rather than clang-cl),
# MSVC is FALSE so spb_compile_options.cmake does not add _CRT_SECURE_NO_WARNINGS.
# With -Werror active this becomes a build error. Suppress it here rather than
# patching the vendored 3rdparty source.
if(WIN32 AND TARGET spb-protoc)
    target_compile_definitions(spb-protoc PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(spb-protoc PRIVATE -Wno-deprecated-declarations)
endif()

spb_protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS core/server/gen/libcore.proto)

add_library(myproto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(myproto
        PUBLIC
        spb-proto
        )
