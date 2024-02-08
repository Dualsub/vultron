#include "Vultron/Vultron.h"

class Application
{
private:
public:
    Application() = default;
    virtual ~Application() = default;

    void Init()
    {
    }

    void Shutdown()
    {
    }

    void Update()
    {
    }

        void Run()
    {
        Vultron::PrintHelloWorld();
    }
};

int main()
{
    Application app;
    app.Run();
    return 0;
}