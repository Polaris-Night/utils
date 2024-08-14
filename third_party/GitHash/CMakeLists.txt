cmake_minimum_required(VERSION 3.14)

###################################
###### CUSTOMIZATION POINTS #######
###################################

# If you change the output file, remember to modify the GitHash.hpp header file
# in sync with it!

find_package(Git REQUIRED)

# Commands to read each needed variable
set(variablesToRead "GIT_BRANCH;GIT_SHA1;GIT_SHORTSHA1;GIT_COMMIT_DATE;GIT_DIRTY")
set(CMD_GIT_BRANCH      ${GIT_EXECUTABLE} branch --show-current)
set(CMD_GIT_SHA1        ${GIT_EXECUTABLE} log -1 --format=%H)
set(CMD_GIT_SHORTSHA1   ${GIT_EXECUTABLE} log -1 --format=%h)
set(CMD_GIT_COMMIT_DATE ${GIT_EXECUTABLE} log -1 --format=%ad --date=format:"%Y%m%d%H%M%S") # 20240813234000
set(CMD_GIT_DIRTY       ${GIT_EXECUTABLE} describe --always --dirty) # we post-process this one

# Generator of the .cpp of the githash library
function(genCppContents outputString)
    set(${outputString}
"namespace GitHash {
    extern const char * const branch;
    extern const char * const sha1;
    extern const char * const shortSha1;
    extern const char * const datetime;
    extern const bool dirty;
    const char * const branch = \"${GIT_BRANCH}\";
    const char * const sha1 = \"${GIT_SHA1}\";
    const char * const shortSha1 = \"${GIT_SHORTSHA1}\";
    const char * const datetime = ${GIT_COMMIT_DATE};
    const bool dirty = ${GIT_DIRTY};
}"
        PARENT_SCOPE
    )
endfunction()

# Cache format (which, if changed, triggers a regeneration of the .cpp)
function(genCache outputString)
    set(${outputString} "${GIT_SHA1}-${GIT_DIRTY}" PARENT_SCOPE)
endfunction()

###################################
###### CONFIGURATION POINTS #######
###################################

set(GitHash_OutputDir     "${PROJECT_BINARY_DIR}/GitHash" CACHE STRING "default directory for the output files")
set(GitHash_CppFilename   "GitHash.cpp"                   CACHE STRING "default name of the output cpp file")
set(GitHash_CacheFilename "GitHashCache.txt"              CACHE STRING "default name of the output cache file")

##########################################################
### You MUST call SetupGitHash in your CMakeLists.txt! ###
##########################################################

# Set utility names for full paths outputs
get_filename_component(GitHash_AbsoluteOutputDir ${GitHash_OutputDir} ABSOLUTE BASE_DIR "${PROJECT_BINARY_DIR}")
set(GitHash_CppFile "${GitHash_AbsoluteOutputDir}/${GitHash_CppFilename}")
set(GitHash_CacheFile "${GitHash_AbsoluteOutputDir}/${GitHash_CacheFilename}")

# Directory where to actually run the Git Commands.
if (NOT DEFINED GitHash_SourceDir)
    set(GitHash_SourceDir "${CMAKE_SOURCE_DIR}")
endif()

function(SetupGitHash)
    # Run this script when building. Note how we pass all variables we need, since we will not get them automatically
    # and even the CMake source dir might be wrong (if for example the build folder is outside the original path)
    add_custom_target(CheckGitHash COMMAND ${CMAKE_COMMAND}
        -DRUN_UPDATE_GIT_HASH=1
        -DGitHash_OutputDir="${GitHash_AbsoluteOutputDir}"
        -DGitHash_CppFilename="${GitHash_CppFilename}"
        -DGitHash_CacheFilename="${GitHash_CacheFilename}"
        -DGitHash_SourceDir="${GitHash_SourceDir}"
        -P ${_THIS_MODULE_FILE}
        BYPRODUCTS ${GitHash_CppFile}
    )

    # Create library for user
    add_library(githash ${GitHash_CppFile})
    target_include_directories(githash PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
    target_sources(githash PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/GitHash.hpp")
    add_dependencies(githash CheckGitHash)

    # Output library name to the other CMakeLists.txt
    set(GITHASH_LIBRARIES githash CACHE STRING "Name of githash library" FORCE)

    UpdateGitHash()
endfunction()

######################################
### Rest of implementation details ###
######################################

# Needed for setup for older CMake versions (reads this file's path).
set(_THIS_MODULE_FILE "${CMAKE_CURRENT_LIST_FILE}")

# Function called during `make`
function(UpdateGitHash)
    # Make sure our working folder exists.
    if (NOT EXISTS ${GitHash_AbsoluteOutputDir})
        file(MAKE_DIRECTORY ${GitHash_AbsoluteOutputDir})
    endif()

    # Automatically set all variables.
    foreach(c ${variablesToRead})
        execute_process(
            COMMAND ${CMD_${c}}
            WORKING_DIRECTORY "${GitHash_SourceDir}"
            OUTPUT_VARIABLE ${c}
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endforeach(c)

    # GIT_DIRTY post-processing
    if(${GIT_DIRTY} MATCHES ".*dirty")
        set(GIT_DIRTY "true")
    else()
        set(GIT_DIRTY "false")
    endif()

    # Generate new contents for the cache.
    genCache(newSha1Cache)

    # Always update the GitHash.cpp file
    # Set the cache so we can skip rebuilding if nothing changed.
    file(WRITE ${GitHash_CacheFile} ${newSha1Cache})

    # Get the CPP file contents with all variables correctly embedded.
    genCppContents(outputString)

    # Finally output our new library cpp file.
    file(WRITE ${GitHash_CppFile} "${outputString}")
    message(STATUS "Compiling branch ${GIT_BRANCH}, commit ${GIT_SHA1}, datetime ${GIT_COMMIT_DATE}, dirty is ${GIT_DIRTY}")
endfunction()

# This is used to run this function from an external cmake process.
if (RUN_UPDATE_GIT_HASH)
    UpdateGitHash()
else()
    SetupGitHash()
endif()