extern "C" {
    void __init();
}

extern "C" int main(int argc, char *argv[]);
extern "C" void __rpx_start()
{
    __init();

    int ret = main(0, (char**)0);

    extern void _exit(int);
    _exit(ret);
}
