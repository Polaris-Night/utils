set(COMMON_SRC_DIR)
set(COMMON_LIB breakpad_common)
set(CLIENT_SRC_DIR)
set(CLIENT_LIB breakpad_client)

add_subdirectory(third_party)

if (WIN32)
    set(COMMON_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/common/windows)
    set(CLIENT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/client/windows)
elseif(UNIX)
    set(COMMON_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/common/linux)
    set(CLIENT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/client/linux)
else()
    message(FATAL_ERROR "Not support system ${CMAKE_SYSTEM_NAME}")
endif()

# common
file(GLOB COMMON_SRC
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/*.h
    ${COMMON_SRC_DIR}/*.cc
    ${COMMON_SRC_DIR}/*.h
    ${COMMON_SRC_DIR}/*.S
)
file(GLOB COMMON_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/common/*test.cc
    ${COMMON_SRC_DIR}/*test.cc
)
list(REMOVE_ITEM COMMON_SRC ${COMMON_TEST_SRC})
if (UNIX)
    enable_language(ASM)
    list(APPEND COMMON_SRC ${COMMON_SRC_DIR}/breakpad_getcontext.S)
endif()
add_library(${COMMON_LIB} STATIC ${COMMON_SRC})
target_include_directories(${COMMON_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# client
file(GLOB_RECURSE CLIENT_SRC
    ${CLIENT_SRC_DIR}/*.cc
    ${CLIENT_SRC_DIR}/*.h
)
file(GLOB CLIENT_INDEPENDENCE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/client/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/client/*.h
)
list(APPEND CLIENT_SRC ${CLIENT_INDEPENDENCE_SRC})
file(GLOB_RECURSE CLIENT_TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/client/*test.cc)
list(REMOVE_ITEM CLIENT_SRC ${CLIENT_TEST_SRC} ${CLIENT_SRC_DIR}/sender/google_crash_report_sender.cc)
add_library(${CLIENT_LIB} STATIC ${CLIENT_SRC})
target_link_libraries(${CLIENT_LIB} PUBLIC ${COMMON_LIB})
target_include_directories(${CLIENT_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# dump_syms in linux
set(DUMP_SYMS_SRC
    ${CMAKE_CURRENT_LIST_DIR}/tools/linux/dump_syms/dump_syms.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf_cfi_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf_cu_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf_line_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf_range_list_handler.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/language.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/path_helper.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/stabs_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/stabs_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf/bytereader.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf/dwarf2diehandler.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf/dwarf2reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/dwarf/elf_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/crc32.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/dump_symbols.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/elf_symbols_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/elfutils.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/file_id.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/linux_libc_support.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/memory_mapped_file.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/safe_readlink.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/elf_symbols_to_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/linux/elf_symbols_to_module.cc
)
add_executable(dump_syms ${DUMP_SYMS_SRC})
target_link_libraries(dump_syms PRIVATE ${COMMON_LIB} z)
target_include_directories(dump_syms PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# minidump_stackwalk in linux
set(MINUDUMP_STACKWALK_SRC
    ${CMAKE_CURRENT_LIST_DIR}/common/path_helper.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump_stackwalk.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/basic_code_modules.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/basic_source_line_resolver.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/call_stack.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/cfi_frame_info.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/convert_old_arm64_context.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/disassembler_x86.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/dump_context.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/dump_object.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability_linux.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability_win.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/logging.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump_processor.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/pathname_stripper.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/process_state.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/proc_maps_linux.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/simple_symbol_supplier.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/source_line_resolver_base.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stack_frame_cpu.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stack_frame_symbolizer.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalk_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_address_list.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_amd64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_arm.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_arm64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_mips.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_ppc.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_ppc64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_riscv.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_riscv64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_sparc.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_x86.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/symbolic_constants_win.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/tokenize.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/disassembler_objdump.cc
)
add_executable(minidump_stackwalk ${MINUDUMP_STACKWALK_SRC})
target_link_libraries(minidump_stackwalk PRIVATE ${COMMON_LIB} disasm)
target_include_directories(minidump_stackwalk PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# minidump module reader
set(MINIDUMP_MODULE_READER_SRC
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump_module_reader.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/path_helper.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/basic_code_modules.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/basic_source_line_resolver.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/call_stack.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/cfi_frame_info.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/convert_old_arm64_context.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/disassembler_x86.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/dump_context.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/dump_object.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability_linux.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/exploitability_win.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/logging.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/minidump_processor.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/pathname_stripper.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/process_state.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/proc_maps_linux.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/simple_symbol_supplier.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/source_line_resolver_base.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stack_frame_cpu.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stack_frame_symbolizer.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalk_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_address_list.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_amd64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_arm.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_arm64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_mips.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_ppc.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_ppc64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_riscv.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_riscv64.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_sparc.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/stackwalker_x86.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/symbolic_constants_win.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/tokenize.cc
    ${CMAKE_CURRENT_LIST_DIR}/processor/disassembler_objdump.cc
)
add_executable(minidump_module_reader ${MINIDUMP_MODULE_READER_SRC})
target_link_libraries(minidump_module_reader PRIVATE ${COMMON_LIB} disasm)
target_include_directories(minidump_module_reader PUBLIC ${CMAKE_CURRENT_LIST_DIR})

install(
    TARGETS ${CLIENT_LIB}
    ARCHIVE DESTINATION lib
)
