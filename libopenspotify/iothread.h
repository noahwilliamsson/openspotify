#ifdef _WIN32
DWORD WINAPI iothread(LPVOID data);
#else
void *iothread(void *data);
#endif
