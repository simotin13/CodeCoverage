#pragma once

class StringHelper
{
private:
    template<typename T, typename std::enable_if<std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value>::type* = nullptr>
    static auto convert(T value)
    {
        return std::forward<T>(value).c_str();
    }

    template<typename T, typename std::enable_if<!std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value>::type* = nullptr>
    static auto convert(T value)
    {
        return std::forward<T>(value);
    }

    template<typename ... Args>
    static std::string strformat(const std::string &fmt, Args ... args)
    {
        int len = std::snprintf(nullptr, 0, fmt.c_str(), std::forward<Args>(args) ...);
        if (len < 0)
        {
            // 異常系(想定外)
            return "";
        }
        size_t bufSize = len + sizeof(char);
        std::vector<char> buf(bufSize);
        std::snprintf(buf.data(), bufSize, fmt.c_str(), args ...);
        return std::string(buf.data(), buf.data() + len);
    }

public:
    template<typename ... Args>
    static std::string strprintf(const std::string &fmt, Args ... args)
    {
        return strformat(fmt, convert(std::forward<Args>(args)) ...);
    }
};
