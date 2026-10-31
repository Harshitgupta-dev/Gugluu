#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#define PRINT(a_Message) \
    do { std::cout << a_Message << '\n'; } while (false)

#define GUGLUU_ASSERT(a_Condition, a_Message) \
    do \
    { \
        if (!(a_Condition)) \
        { \
            std::ostringstream gugluu_assert_message; \
            gugluu_assert_message << a_Message; \
            throw std::runtime_error( \
                std::string("Assertion failed: ") + #a_Condition + ": " + \
                gugluu_assert_message.str()); \
        } \
    } while (false)

template <typename T>
inline std::ostream& operator<<(std::ostream& a_Os, const std::vector<T>& a_Values)
{
    a_Os << '[';
    for (std::size_t index = 0; index < a_Values.size(); ++index)
    {
        if constexpr (std::is_same_v<T, std::string>)
            a_Os << '\"' << a_Values[index] << '\"';
        else
            a_Os << a_Values[index];

        if (index + 1 < a_Values.size())
            a_Os << ", ";
    }
    a_Os << ']';
    return a_Os;
}
