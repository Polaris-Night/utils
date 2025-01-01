macro(build_breakpad BREAKPAD_ROOT_DIR)
    if (WIN32)
        add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
                        -DUNICODE
                        -D_UNICODE)
    elseif(UNIX)
        add_definitions(-DHAVE_A_OUT_H)
        enable_language(ASM)
    else()
        message(FATAL_ERROR "Not support system ${CMAKE_SYSTEM_NAME}")
    endif()

    # Set source dir
    set(BREAKPAD_SRC_DIR ${BREAKPAD_ROOT_DIR}/src)
    if (WIN32)
        set(BREAKPAD_COMMON_SRC_DIR ${BREAKPAD_SRC_DIR}/common/windows)
        set(BREAKPAD_CLIENT_SRC_DIR ${BREAKPAD_SRC_DIR}/client/windows)
    elseif(UNIX)
        set(BREAKPAD_COMMON_SRC_DIR ${BREAKPAD_SRC_DIR}/common/linux)
        set(BREAKPAD_CLIENT_SRC_DIR ${BREAKPAD_SRC_DIR}/client/linux)
    else()
        message(FATAL_ERROR "Not support system ${CMAKE_SYSTEM_NAME}")
    endif()

    # disasm
    file(GLOB DISASM_SRC ${BREAKPAD_SRC_DIR}/third_party/libdisasm/*.c ${BREAKPAD_SRC_DIR}/third_party/libdisasm/*.h)
    add_library(disasm STATIC ${DISASM_SRC})

    # common
    file(GLOB BREAKPAD_COMMON_SRC
        ${BREAKPAD_SRC_DIR}/common/*.cc
        ${BREAKPAD_SRC_DIR}/common/*.h
        ${BREAKPAD_COMMON_SRC_DIR}/*.cc
        ${BREAKPAD_COMMON_SRC_DIR}/*.h
        ${BREAKPAD_COMMON_SRC_DIR}/*.S
    )
    file(GLOB BREAKPAD_COMMON_TEST_SRC
        ${BREAKPAD_SRC_DIR}/common/*test.cc
        ${BREAKPAD_COMMON_SRC_DIR}/*test.cc
    )
    list(REMOVE_ITEM BREAKPAD_COMMON_SRC ${BREAKPAD_COMMON_TEST_SRC})
    if (UNIX)
        list(APPEND BREAKPAD_COMMON_SRC ${BREAKPAD_COMMON_SRC_DIR}/breakpad_getcontext.S)
    endif()
    add_library(breakpad_common STATIC ${BREAKPAD_COMMON_SRC})
    target_include_directories(breakpad_common PUBLIC ${BREAKPAD_SRC_DIR})
    if (UNIX)
        target_link_libraries(breakpad_common PRIVATE pthread)
    endif()

    # client
    file(GLOB_RECURSE BREAKPAD_CLIENT_SRC
        ${BREAKPAD_CLIENT_SRC_DIR}/*.cc
        ${BREAKPAD_CLIENT_SRC_DIR}/*.h
    )
    file(GLOB CLIENT_INDEPENDENCE_SRC
        ${BREAKPAD_SRC_DIR}/client/*.cc
        ${BREAKPAD_SRC_DIR}/client/*.h
    )
    list(APPEND BREAKPAD_CLIENT_SRC ${CLIENT_INDEPENDENCE_SRC})
    file(GLOB_RECURSE BREAKPAD_CLIENT_TEST_SRC ${BREAKPAD_SRC_DIR}/client/*test.cc)
    list(REMOVE_ITEM BREAKPAD_CLIENT_SRC ${BREAKPAD_CLIENT_TEST_SRC} ${BREAKPAD_CLIENT_SRC_DIR}/sender/google_crash_report_sender.cc)
    add_library(breakpad_client STATIC ${BREAKPAD_CLIENT_SRC})
    target_link_libraries(breakpad_client PUBLIC breakpad_common)
    target_include_directories(breakpad_client PUBLIC ${BREAKPAD_SRC_DIR})

    # dump_syms in linux
    set(BREAKPAD_DUMP_SYMS_SRC
        ${BREAKPAD_SRC_DIR}/tools/linux/dump_syms/dump_syms.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf_cfi_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf_cu_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf_line_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf_range_list_handler.cc
        ${BREAKPAD_SRC_DIR}/common/language.cc
        ${BREAKPAD_SRC_DIR}/common/module.cc
        ${BREAKPAD_SRC_DIR}/common/path_helper.cc
        ${BREAKPAD_SRC_DIR}/common/stabs_reader.cc
        ${BREAKPAD_SRC_DIR}/common/stabs_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf/bytereader.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf/dwarf2diehandler.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf/dwarf2reader.cc
        ${BREAKPAD_SRC_DIR}/common/dwarf/elf_reader.cc
        ${BREAKPAD_SRC_DIR}/common/linux/crc32.cc
        ${BREAKPAD_SRC_DIR}/common/linux/dump_symbols.cc
        ${BREAKPAD_SRC_DIR}/common/linux/elf_symbols_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/linux/elfutils.cc
        ${BREAKPAD_SRC_DIR}/common/linux/file_id.cc
        ${BREAKPAD_SRC_DIR}/common/linux/linux_libc_support.cc
        ${BREAKPAD_SRC_DIR}/common/linux/memory_mapped_file.cc
        ${BREAKPAD_SRC_DIR}/common/linux/safe_readlink.cc
        ${BREAKPAD_SRC_DIR}/common/linux/elf_symbols_to_module.cc
        ${BREAKPAD_SRC_DIR}/common/linux/elf_symbols_to_module.cc
    )
    add_executable(dump_syms ${BREAKPAD_DUMP_SYMS_SRC})
    target_link_libraries(dump_syms PRIVATE breakpad_common z)
    target_include_directories(dump_syms PUBLIC ${BREAKPAD_SRC_DIR})

    # minidump_stackwalk in linux
    set(MINUDUMP_STACKWALK_SRC
        ${BREAKPAD_SRC_DIR}/common/path_helper.cc
        ${BREAKPAD_SRC_DIR}/processor/minidump_stackwalk.cc
        ${BREAKPAD_SRC_DIR}/processor/basic_code_modules.cc
        ${BREAKPAD_SRC_DIR}/processor/basic_source_line_resolver.cc
        ${BREAKPAD_SRC_DIR}/processor/call_stack.cc
        ${BREAKPAD_SRC_DIR}/processor/cfi_frame_info.cc
        ${BREAKPAD_SRC_DIR}/processor/convert_old_arm64_context.cc
        ${BREAKPAD_SRC_DIR}/processor/disassembler_x86.cc
        ${BREAKPAD_SRC_DIR}/processor/dump_context.cc
        ${BREAKPAD_SRC_DIR}/processor/dump_object.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability_linux.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability_win.cc
        ${BREAKPAD_SRC_DIR}/processor/logging.cc
        ${BREAKPAD_SRC_DIR}/processor/minidump.cc
        ${BREAKPAD_SRC_DIR}/processor/minidump_processor.cc
        ${BREAKPAD_SRC_DIR}/processor/pathname_stripper.cc
        ${BREAKPAD_SRC_DIR}/processor/process_state.cc
        ${BREAKPAD_SRC_DIR}/processor/proc_maps_linux.cc
        ${BREAKPAD_SRC_DIR}/processor/simple_symbol_supplier.cc
        ${BREAKPAD_SRC_DIR}/processor/source_line_resolver_base.cc
        ${BREAKPAD_SRC_DIR}/processor/stack_frame_cpu.cc
        ${BREAKPAD_SRC_DIR}/processor/stack_frame_symbolizer.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalk_common.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_address_list.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_amd64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_arm.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_arm64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_mips.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_ppc.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_ppc64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_riscv.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_riscv64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_sparc.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_x86.cc
        ${BREAKPAD_SRC_DIR}/processor/symbolic_constants_win.cc
        ${BREAKPAD_SRC_DIR}/processor/tokenize.cc
        ${BREAKPAD_SRC_DIR}/processor/disassembler_objdump.cc
    )
    add_executable(minidump_stackwalk ${MINUDUMP_STACKWALK_SRC})
    target_link_libraries(minidump_stackwalk PRIVATE breakpad_common disasm)
    target_include_directories(minidump_stackwalk PUBLIC ${BREAKPAD_SRC_DIR})

    # minidump module reader
    set(MINIDUMP_MODULE_READER_SRC
        ${BREAKPAD_SRC_DIR}/processor/minidump_module_reader.cc
        ${BREAKPAD_SRC_DIR}/common/path_helper.cc
        ${BREAKPAD_SRC_DIR}/processor/basic_code_modules.cc
        ${BREAKPAD_SRC_DIR}/processor/basic_source_line_resolver.cc
        ${BREAKPAD_SRC_DIR}/processor/call_stack.cc
        ${BREAKPAD_SRC_DIR}/processor/cfi_frame_info.cc
        ${BREAKPAD_SRC_DIR}/processor/convert_old_arm64_context.cc
        ${BREAKPAD_SRC_DIR}/processor/disassembler_x86.cc
        ${BREAKPAD_SRC_DIR}/processor/dump_context.cc
        ${BREAKPAD_SRC_DIR}/processor/dump_object.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability_linux.cc
        ${BREAKPAD_SRC_DIR}/processor/exploitability_win.cc
        ${BREAKPAD_SRC_DIR}/processor/logging.cc
        ${BREAKPAD_SRC_DIR}/processor/minidump.cc
        ${BREAKPAD_SRC_DIR}/processor/minidump_processor.cc
        ${BREAKPAD_SRC_DIR}/processor/pathname_stripper.cc
        ${BREAKPAD_SRC_DIR}/processor/process_state.cc
        ${BREAKPAD_SRC_DIR}/processor/proc_maps_linux.cc
        ${BREAKPAD_SRC_DIR}/processor/simple_symbol_supplier.cc
        ${BREAKPAD_SRC_DIR}/processor/source_line_resolver_base.cc
        ${BREAKPAD_SRC_DIR}/processor/stack_frame_cpu.cc
        ${BREAKPAD_SRC_DIR}/processor/stack_frame_symbolizer.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalk_common.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_address_list.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_amd64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_arm.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_arm64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_mips.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_ppc.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_ppc64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_riscv.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_riscv64.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_sparc.cc
        ${BREAKPAD_SRC_DIR}/processor/stackwalker_x86.cc
        ${BREAKPAD_SRC_DIR}/processor/symbolic_constants_win.cc
        ${BREAKPAD_SRC_DIR}/processor/tokenize.cc
        ${BREAKPAD_SRC_DIR}/processor/disassembler_objdump.cc
    )
    add_executable(minidump_module_reader ${MINIDUMP_MODULE_READER_SRC})
    target_link_libraries(minidump_module_reader PRIVATE breakpad_common disasm)
    target_include_directories(minidump_module_reader PUBLIC ${BREAKPAD_SRC_DIR})

    install(TARGETS breakpad_client dump_syms minidump_stackwalk minidump_module_reader)

    install(
        DIRECTORY
        ${BREAKPAD_SRC_DIR}/client
        ${BREAKPAD_SRC_DIR}/common
        ${BREAKPAD_SRC_DIR}/google_breakpad
        ${BREAKPAD_SRC_DIR}/processor
        ${BREAKPAD_SRC_DIR}/third_party
        DESTINATION include/breakpad
        FILES_MATCHING PATTERN "*.h"
    )
endmacro()