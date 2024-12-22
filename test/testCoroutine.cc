#include <SpaE/coroutine.h>

using namespace SpaE;

#define LOG(fmt, ...)       printf("%.6f testCoroutine " fmt, uptime(), __VA_ARGS__)

static Coroutine *g_co1 = nullptr, *g_co2 = nullptr;

void testYield1()
{
    auto co1w1 = g_co1->work(
        []
        {
            LOG("%s, co1 work 1 begin \r\n", __FUNCTION__);

            g_co1->work(
                []
                {
                    LOG("%s, co1 work 2 \r\n", __FUNCTION__);
                }
            );

            Coroutine::yield();

            LOG("%s, co1 work 1 end \r\n", __FUNCTION__);
        }
    );

    g_co1->join(co1w1);

    LOG("%s %d \r\n\r\n", __FUNCTION__, __LINE__);
}

void testJoin1()
{
    auto co1w1 = g_co1->work(
        []
        {
            LOG("%s, co1 work 1 \r\n", __FUNCTION__);
        }
    );

    g_co1->join(co1w1);

    LOG("%s %d \r\n\r\n", __FUNCTION__, __LINE__);
}

void testJoin2()
{
    auto co2w1 = g_co2->work(
        []
        {
            Coroutine::yieldFor(1);

            LOG("%s, co2 work 1 \r\n", __FUNCTION__);
        }
    );

    auto co1w1 = g_co1->work(
        [=]
        {
            g_co2->join(co2w1);

            LOG("%s, co1 work 1 \r\n", __FUNCTION__);
        }
    );

    g_co1->join(co1w1);

    LOG("%s %d \r\n\r\n", __FUNCTION__, __LINE__);
}

void testJoin3()
{
    auto co1w1 = g_co1->work(
        []
        {
            LOG("%s, co1 work 1 begin \r\n", __FUNCTION__);

            Coroutine::yieldFor(1);

            LOG("%s, co1 work 1 end \r\n", __FUNCTION__);
        }
    );

    auto co1w2 = g_co1->work(
        [=]
        {
            LOG("%s, co1 work 2 begin \r\n", __FUNCTION__);

            g_co1->join(co1w1);

            LOG("%s, co1 work 2 end \r\n", __FUNCTION__);
        }
    );

    g_co1->join(co1w2);

    LOG("%s %d \r\n\r\n", __FUNCTION__, __LINE__);
}

void testPending1()
{
    auto co1w1 = g_co1->work(
        []
        {
            LOG("%s, co1 work 1 begin \r\n", __FUNCTION__);

            Coroutine::pending();

            LOG("%s, co1 work 1 end \r\n", __FUNCTION__);
        }
    );

    auto co1w2 = g_co1->work(
        [=]
        {
            LOG("%s, co1 work 2 begin \r\n", __FUNCTION__);

            Coroutine::yieldFor(1);

            g_co1->resume(co1w1);

            LOG("%s, co1 work 2 end \r\n", __FUNCTION__);
        }
    );

    g_co1->join(co1w2);

    LOG("%s %d \r\n\r\n", __FUNCTION__, __LINE__);
}

void testCoroutine()
{
    g_co1 = Coroutine::newInstance("co1");
    g_co2 = Coroutine::newInstance("co2");

    testYield1();

    testJoin1();
    testJoin2();
    testJoin3();

    testPending1();
}
