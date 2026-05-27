#include "../sphere/threads.h"

class MainThread : public AbstractSphereThread
{
public:
    MainThread();
    ~MainThread() override { }

    MainThread(const MainThread& copy) = delete;
    MainThread& operator=(const MainThread& other) = delete;

    // we increase the access level from protected to public in order to allow manual execution when
    // configuration disables using threads
    // TODO: in the future, such simulated functionality should lie in AbstractThread inself instead of hacks
    void tick() override;

protected:
    void onStart() override;
    bool shouldExit() noexcept override;
};
