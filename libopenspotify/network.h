#ifdef _WIN32
DWORD WINAPI network_thread(LPVOID data);
#else
void *network_thread(void *data);
#endif
