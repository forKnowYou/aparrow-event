#include <stdio.h>

#include <unistd.h>

#include <SpaE/connector.h>
#include <SpaE/timer.h>
#include <SpaE/coroutine.h>

#include "../src/context.h"

void testIRRef(int &&i)
{

}

template<typename T>
void testRRef(T &&t)
{

}

class Parent : public SpaE::Object
{
signals:
    SpaE::Signal<>          signal1;
    SpaE::Signal<int>       signal2;
    SpaE::Signal<int>       signal3;

slots:
    void slot1()
    {

    }

    void slot2(int i)
    {
        printf("%s %s %d: %d \r\n", __FILE__, __FUNCTION__, __LINE__, i);
    }
};

void g_slot1()
{

}

void g_slot2(int i)
{

}

void testTemplate()
{
    SpaE::Loop::getInstance()->workSync(
        []
        {
            Parent p;

            SpaE::connect(&p, &p.signal1, [] () {});
            SpaE::connect(&p, &p.signal1, g_slot1);
            SpaE::connect(&p, &p.signal2, g_slot2);
            SpaE::connect(&p, &p.signal2, [] (int) {});

            SpaE::connect(&p, &p.signal1, &p, &Parent::slot1);
            SpaE::connect(&p, &p.signal2, &p, &Parent::slot2);
            SpaE::connect(&p, &p.signal2, &p, [] (int) {});
            SpaE::connect(&p, &p.signal2, &p, &p.signal3);  // connect to signal must use pointer
            // SpaE::connect(&p, &p.signal2, &p, p.signal3);

            SpaE::connect(&p, &p.signal1, nullptr, &g_slot1);
            SpaE::connect(&p, &p.signal2, nullptr, &g_slot2);
            SpaE::connect(&p, &p.signal2, nullptr, [] (int) {});

            p.signal1.dispatch();
            p.signal2.dispatch(1);

            auto p2 = p;
        }
    );
}

void testTimer()
{
    auto l = SpaE::Loop::getInstance();

    l->work(
        []
        {
            SpaE::Timer *t2;

            t2 = SpaE::setTimeout(2,
                [=]
                {
                    printf("%s %s %d \r\n", __FILE__, __FUNCTION__, __LINE__);
                }
            );

            SpaE::setTimeout(1,
                [=]
                {
                    // SpaE::deleteTimer(t2);

                    t2->start(3);

                    printf("%s %s %d \r\n", __FILE__, __FUNCTION__, __LINE__);
                }
            );
        }
    );
}

void f1(tb_context_from_t from)
{
    int a = 1;

    printf("context f1 %d \r\n", __LINE__);

    from = tb_context_jump(from.context, nullptr);

    printf("context f1 %d, a = %d \r\n", __LINE__, a);

    from = tb_context_jump(from.context, nullptr);

    a = 3;

    printf("context f1 %d, a = %d \r\n", __LINE__, a);

    from = tb_context_jump(from.context, nullptr);

    // app will exit because no return [address_register]
}

char context1[8192 * 1024];

void testContext()
{
    auto c1 = tb_context_make(context1, sizeof(context1), f1);

    auto from = tb_context_jump(c1, nullptr);

    printf("context return %d \r\n", __LINE__);

    from = tb_context_jump(from.context, nullptr);

    printf("context return %d \r\n", __LINE__);

    from = tb_context_jump(from.context, nullptr);

    printf("context return %d \r\n", __LINE__);
}

extern void testCoroutine();

void testFRef(const std::function<void ()> &f)
{
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
}

void testFRef(std::function<void ()> &&f)
{
    auto ff = std::move(f);

    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    ff();
}

int main(int argc, char **argv)
{
    testTemplate();

    testTimer();

    // testContext();

    testCoroutine();

    // std::function<void ()> f = [] {
    //     printf("f \r\n");
    // };

    // testFRef(f);
    // testFRef([] {});

    // f();

    sleep(-1);

    return 0;
}
