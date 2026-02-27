#include <time.h>
#include <string.h>

#if defined _WIN32
#include <windows.h>
#endif

extern "C"
{
#if defined _WIN32
    void Plat_localtime(struct tm* out, const time_t* timep)
    {
        if (out && timep)
        {
            localtime_s(out, timep);
        }
    }
#else
    void Plat_localtime(struct tm* out, const time_t* timep)
    {
        if (out && timep)
        {
            localtime_r(timep, out);
        }
    }
#endif
}
