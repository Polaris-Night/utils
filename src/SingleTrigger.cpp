#include "SingleTrigger.h"

namespace utils {

SingleTrigger::SingleTrigger( bool trigger ) : trigger_( trigger ) {}
void SingleTrigger::Trigger() {
    trigger_ = true;
}

bool SingleTrigger::Consume() {
    return trigger_.exchange( false );
}

SingleTrigger::Ptr SingleTrigger::Create( bool trigger ) {
    return std::make_shared<SingleTrigger>( trigger );
}

void SingleTrigger::Trigger( Ptr &ptr ) {
    if ( ptr ) {
        ptr->Trigger();
    }
}

void SingleTrigger::Trigger( Weak &weak ) {
    if ( auto ptr = weak.lock() ) {
        ptr->Trigger();
    }
}

}  // namespace utils