#include <thread>
#include "SingleTrigger.h"
#include "gtest/gtest.h"

using namespace utils;

TEST( SingleTriggerTest, Constructor ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( false );
    EXPECT_FALSE( trigger->Consume() );

    trigger = SingleTrigger::Create( true );
    EXPECT_TRUE( trigger->Consume() );
}

TEST( SingleTriggerTest, Trigger ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( false );
    trigger->Trigger();
    EXPECT_TRUE( trigger->Consume() );
}

TEST( SingleTriggerTest, Consume ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( true );
    EXPECT_TRUE( trigger->Consume() );
    EXPECT_FALSE( trigger->Consume() );
}

TEST( SingleTriggerTest, ExecuteIfTriggered ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( false );
    int                value   = 0;
    trigger->ExecuteIfTriggered( [&value]() { value = 1; } );
    EXPECT_EQ( value, 0 );

    trigger->Trigger();
    trigger->ExecuteIfTriggered( [&value]() { value = 2; } );
    EXPECT_EQ( value, 2 );
}

TEST( SingleTriggerTest, StaticTriggerPtr ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( false );
    SingleTrigger::Trigger( trigger );
    EXPECT_TRUE( trigger->Consume() );
}

TEST( SingleTriggerTest, StaticTriggerWeak ) {
    SingleTrigger::Ptr  trigger      = SingleTrigger::Create( false );
    SingleTrigger::Weak weak_trigger = trigger;
    SingleTrigger::Trigger( weak_trigger );
    EXPECT_TRUE( trigger->Consume() );
}

TEST( SingleTriggerTest, MultiThreadTrigger ) {
    SingleTrigger::Ptr trigger = SingleTrigger::Create( false );
    std::thread        t( [&trigger]() {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        trigger->Trigger();
    } );

    EXPECT_FALSE( trigger->Consume() );
    t.join();
    EXPECT_TRUE( trigger->Consume() );
}