#ifndef mozilla__ipdltest_TestSyncError_h
#define mozilla__ipdltest_TestSyncError_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncErrorParent.h"
#include "mozilla/_ipdltest/PTestSyncErrorChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSyncErrorParent :
    public PTestSyncErrorParent
{
public:
    TestSyncErrorParent();
    virtual ~TestSyncErrorParent();

    void Main();

protected:    
    NS_OVERRIDE
    virtual bool RecvError();

    NS_OVERRIDE
    virtual void ProcessingError(Result what)
    {
        // Ignore errors
    }

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestSyncErrorChild :
    public PTestSyncErrorChild
{
public:
    TestSyncErrorChild();
    virtual ~TestSyncErrorChild();

protected:
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual void ProcessingError(Result what)
    {
        // Ignore errors
    }

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSyncError_h
