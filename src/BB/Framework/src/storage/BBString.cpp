#include "Storage/BBString.h"
#include <stdarg.h>

using namespace BB;

size_t BB::_FormatString(char* a_dst, const size_t a_dst_size, const StringView a_fmt_string, va_list a_args)
{
    if (a_fmt_string.size() > a_dst_size)
        return size_t(-1);

    size_t dst_pos = 0;
    size_t fmt_pos = 0;

    auto add_to_string = [&dst_pos, &fmt_pos, &a_dst, &a_dst_size](const StringView a_src) -> bool
        {
            if (a_src.size() + fmt_pos >= a_dst_size)
                return false;
            memcpy(&a_dst[dst_pos], a_src.data(), a_src.size());
            dst_pos += a_src.size();
            return true;
        };

    auto add_to_string_wchar = [&dst_pos, &fmt_pos, &a_dst, &a_dst_size](const StringWView a_src) -> bool
        {
            if (a_src.size() + fmt_pos >= a_dst_size)
                return false;
            size_t conv_chars = 0;
            wcstombs_s(&conv_chars, &a_dst[dst_pos], a_src.size() + 1, a_src.c_str(), a_src.size());
            dst_pos += a_src.size();
            return true;
        };

    size_t percentage_spot = a_fmt_string.find_first_of('%');
    if (percentage_spot == size_t(-1))
    {
        add_to_string(a_fmt_string);
        return a_fmt_string.size();
    }

    while (percentage_spot != size_t(-1) && fmt_pos <= a_fmt_string.size())
    {
        const size_t string_size = percentage_spot - fmt_pos;
        if (!add_to_string(a_fmt_string.SubView(string_size, fmt_pos)))
            return size_t(-1);

        fmt_pos = percentage_spot + 2;

        switch (a_fmt_string[percentage_spot + 1])
        {
        case 'u':
        {
            uint32_t value = va_arg(a_args, uint32_t);
            char buf[32]{};
            int i = 30;
            for (; value && i; --i, value /= 10)
                buf[i] = "0123456789abcdef"[value % 10];

            add_to_string(&buf[i] + 1);
        }
            break;
        case 's':
            add_to_string(va_arg(a_args, char*));
            break;
        case 'S':
            add_to_string_wchar(va_arg(a_args, const wchar_t*));
            break;
        default:
            BB_ASSERT(false, "va arg format not yet supported");
            break;
        }
        percentage_spot = a_fmt_string.find_first_of('%', fmt_pos);
    } 

    return dst_pos;
}

size_t BB::FormatString(char* a_dst, const size_t a_dst_size, const StringView a_fmt_string, ...)
{
    va_list vl;
    va_start(vl, a_fmt_string);
    const size_t retval = _FormatString(a_dst, a_dst_size, a_fmt_string, vl);
    va_end(vl);
    return retval;
}
