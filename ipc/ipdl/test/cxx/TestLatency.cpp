#include "TestLatency.h"

#include "IPDLUnitTests.h"      // fail etc.


namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestLatencyParent::TestLatencyParent() :
    mStart(),
    mPPTimeTotal(),
    mPP5TimeTotal(),
    mPPTrialsToGo(NR_TRIALS),
    mPP5TrialsToGo(NR_TRIALS),
    mPongsToGo(0)
{
    MOZ_COUNT_CTOR(TestLatencyParent);
}

TestLatencyParent::~TestLatencyParent()
{
    MOZ_COUNT_DTOR(TestLatencyParent);
}

void
TestLatencyParent::Main()
{
    if (mozilla::ipc::LoggingEnabled())
        NS_RUNTIMEABORT("you really don't want to log all IPC messages during this test, trust me");

    PingPongTrial();
}

void
TestLatencyParent::PingPongTrial()
{
    mStart = TimeStamp::Now();
    SendPing();
}

void
TestLatencyParent::Ping5Pong5Trial()
{
    mStart = TimeStamp::Now();
    // HACK
    mPongsToGo = 5;

    SendPing5();
    SendPing5();
    SendPing5();
    SendPing5();
    SendPing5();
}

void
TestLatencyParent::Exit()
{
    Close();
}

bool
TestLatencyParent::RecvPong()
{
    TimeDuration thisTrial = (TimeStamp::Now() - mStart);
    mPPTimeTotal += thisTrial;

    if (0 == ((mPPTrialsToGo % 1000)))
        printf("  PP trial %d: %g\n",
               mPPTrialsToGo, thisTrial.ToSeconds());

    if (--mPPTrialsToGo > 0)
        PingPongTrial();
    else
        Ping5Pong5Trial();
    return true;
}

bool
TestLatencyParent::RecvPong5()
{
    // HACK
    if (0 < --mPongsToGo)
        return true;

    TimeDuration thisTrial = (TimeStamp::Now() - mStart);
    mPP5TimeTotal += thisTrial;

    if (0 == ((mPP5TrialsToGo % 1000)))
        printf("  PP5 trial %d: %g\n",
               mPP5TrialsToGo, thisTrial.ToSeconds());

    if (0 < --mPP5TrialsToGo)
        Ping5Pong5Trial();
    else
        Exit();

    return true;
}


//-----------------------------------------------------------------------------
// child

TestLatencyChild::TestLatencyChild()
{
    MOZ_COUNT_CTOR(TestLatencyChild);
}

TestLatencyChild::~TestLatencyChild()
{
    MOZ_COUNT_DTOR(TestLatencyChild);
}

bool
TestLatencyChild::RecvPing()
{
    SendPong();
    return true;
}

bool
TestLatencyChild::RecvPing5()
{
    SendPong5();
    return true;
}


} // namespace _ipdltest
} // namespace mozilla
