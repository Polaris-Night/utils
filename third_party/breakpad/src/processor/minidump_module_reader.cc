// Copyright 2010 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// minidump_module_reader.cc: Process a minidump, printing out the module name and identifier.
//
// Author: Polaris-Night

#ifdef HAVE_CONFIG_H
    #include <config.h>  // Must come first
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <limits>
#include <string>
#include <vector>

#include "common/path_helper.h"
#include "google_breakpad/processor/minidump.h"
#include "processor/logging.h"
#include "processor/pathname_stripper.h"


namespace {

struct Options {
    string minidump_file;
};

using google_breakpad::CodeModule;
using google_breakpad::Minidump;
using google_breakpad::MinidumpModuleList;
using google_breakpad::PathnameStripper;

bool PrintModuleIdentifier( const Options &options ) {
    Minidump dump( options.minidump_file );
    if ( !dump.Read() ) {
        BPLOG( ERROR ) << "Minidump " << dump.path() << " could not be read";
        return false;
    }
    MinidumpModuleList *module_list = dump.GetModuleList();
    if ( module_list == nullptr ) {
        BPLOG( ERROR ) << "Minidump " << dump.path() << " module list is null";
        return false;
    }
    unsigned int module_count = module_list->module_count();
    if ( module_count == 0 ) {
        BPLOG( ERROR ) << "Minidump " << dump.path() << " module list is empty";
        return false;
    }
    printf( "{\n" );
    for ( unsigned int module_sequence = 0; module_sequence < module_count; ++module_sequence ) {
        const CodeModule *module = module_list->GetModuleAtSequence( module_sequence );
        printf( "    \"%s\": \"%s\"%s\n", PathnameStripper::File( module->debug_file() ).c_str(),
                module->debug_identifier().c_str(), module_sequence < module_count - 1 ? "," : "" );
    }
    printf( "}" );
    std::fflush( stdout );
    return true;
}

}  // namespace

static void Usage( int argc, const char *argv[], bool error ) {
    fprintf( error ? stderr : stdout,
             "Usage: %s [options] <minidump-file>\n"
             "\n"
             "Output a module identifier for the provided minidump\n"
             "\n"
             "Options:\n"
             "\n"
             "  -r         Read dump file module identifier\n",
             google_breakpad::BaseName( argv[0] ).c_str() );
}

static void SetupOptions( int argc, const char *argv[], Options *options ) {
    int ch;
    while ( ( ch = getopt( argc, (char *const *)argv, "bchms" ) ) != -1 ) {
        switch ( ch ) {
            case 'h':
                Usage( argc, argv, false );
                exit( 0 );
                break;

            case 'r':
                break;

            case '?':
                Usage( argc, argv, true );
                exit( 1 );
                break;
        }
    }

    if ( ( argc - optind ) == 0 ) {
        fprintf( stderr, "%s: Missing minidump file\n", argv[0] );
        Usage( argc, argv, true );
        exit( 1 );
    }

    options->minidump_file = argv[optind];
}

int main( int argc, const char *argv[] ) {
    Options options;
    SetupOptions( argc, argv, &options );

    return PrintModuleIdentifier( options ) ? 0 : 1;
}
