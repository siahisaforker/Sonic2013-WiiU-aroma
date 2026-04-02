extern "C" {
    void __init();
}

extern "C" int main(int argc, char *argv[]);
extern "C" void __rpx_start()
{
    // Ensure C/C++ constructors run
    __init();

    // Call main with no arguments; the engine will use SDL/GetLaunchPath where appropriate
    int ret = main(0, (char**)0);

    // Terminate the process
    extern void _exit(int);
    _exit(ret);
}
