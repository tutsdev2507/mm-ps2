// Toolchain feasibility probe for the PS2 (EE) C++ cross-compiler.
//
// The Libultraship-based port needs C++ with the standard library (containers,
// strings, smart pointers, algorithms). This file only exists so CI can report
// which language standard and which parts of the STL actually build for the EE,
// which decides how much of LUS can be reused verbatim.

#include <cstdio>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>

namespace probe {

template <typename T>
struct Box {
    T value;
    explicit Box(T v) : value(static_cast<T&&>(v)) {}
    T get() const { return value; }
};

} // namespace probe

int main() {
    std::vector<int> v{3, 1, 2};
    std::sort(v.begin(), v.end());

    std::string s = "mm";
    s += "-ps2";

    std::unordered_map<std::string, int> m;
    m[s] = static_cast<int>(v.size());

    std::unique_ptr<probe::Box<int>> b(new probe::Box<int>(m[s]));

    std::printf("stl probe: %zu %s %d\n", v.size(), s.c_str(), b->get());
    return 0;
}
