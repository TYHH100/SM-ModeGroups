#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#if defined _WIN32
#include <windows.h>
#endif

#if defined _WIN32
#define TIER0_LINKAGE __declspec(dllexport)
#else
#define TIER0_LINKAGE __attribute__((visibility("default")))
#endif

extern "C"
{
#if defined _WIN32
    TIER0_LINKAGE void Plat_localtime(struct tm* out, const time_t* timep)
    {
        if (out && timep)
        {
            localtime_s(out, timep);
        }
    }
#else
    TIER0_LINKAGE void Plat_localtime(struct tm* out, const time_t* timep)
    {
        if (out && timep)
        {
            localtime_r(timep, out);
        }
    }
#endif
}

class Color
{
private:
    unsigned char _color[4];
public:
    Color()
    {
        *((int *)_color) = 0;
    }
    Color(int r, int g, int b)
    {
        SetColor(r, g, b, 0);
    }
    Color(int r, int g, int b, int a)
    {
        SetColor(r, g, b, a);
    }
    void SetColor(int r, int g, int b, int a)
    {
        _color[0] = (unsigned char)r;
        _color[1] = (unsigned char)g;
        _color[2] = (unsigned char)b;
        _color[3] = (unsigned char)a;
    }
    void GetColor(int &r, int &g, int &b, int &a) const
    {
        r = _color[0];
        g = _color[1];
        b = _color[2];
        a = _color[3];
    }
    int r() const { return _color[0]; }
    int g() const { return _color[1]; }
    int b() const { return _color[2]; }
    int a() const { return _color[3]; }
    unsigned char operator[](int index) const
    {
        return _color[index];
    }
    bool operator==(const Color &rhs) const
    {
        return *((int *)_color) == *((int *)rhs._color);
    }
    bool operator!=(const Color &rhs) const
    {
        return !(operator==(rhs));
    }
    const unsigned char *Base() const { return _color; }
    static Color FromARGB(unsigned char a, unsigned char r, unsigned char g, unsigned char b)
    {
        return Color(r, g, b, a);
    }
};

TIER0_LINKAGE void ConColorMsg(const Color& color, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

TIER0_LINKAGE void ConPrint(const Color& color, const char* msg)
{
    printf("%s", msg);
}

TIER0_LINKAGE void ConMsg(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

TIER0_LINKAGE void ConWarning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

TIER0_LINKAGE void ConDMsg(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

TIER0_LINKAGE void ConDWarning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
