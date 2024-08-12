#include "BreakpadHandler.h"
#include "crash_lib.h"

int main() {
    auto &handler = utils::BreakpadHandler::Instance();
    handler.SetDumpPath( "." );
    handler.Init();
    testCrash();
    return 0;
}