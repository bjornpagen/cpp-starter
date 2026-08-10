# Existing Bugzilla comments

Add these comments after you file the three new reports.

## PR124197: `template for` and `-Wshadow`

Open [PR124197](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124197).

Paste this comment:

```text
I confirmed this behavior with GCC 16.1.0 and the C++26 reflection implementation.

The reflection form also reports -Wshadow for the expansion variable:

#include <meta>
#include <cstddef>
#include <string_view>

enum class Compass { North, East, South, West };

consteval auto count() -> std::size_t {
  auto result = std::size_t{0};
  template for (constexpr auto enumerator
                : std::define_static_array(
                      std::meta::enumerators_of(^^Compass))) {
    if (!std::meta::identifier_of(enumerator).empty())
      ++result;
  }
  return result;
}

static_assert(count() == 4);

Command:

g++-16 -std=c++26 -freflection -Wshadow -Werror -c repro.cc

GCC reports four -Wshadow errors at the declaration of enumerator. It reports one error for each expanded element.

The source contains one variable declaration and no nested declaration with the same name. The warnings appear to come from the scopes that GCC creates for expansion elements.

This behavior forces projects that use -Werror to disable -Wshadow for each translation unit that contains an expansion statement.
```

## PR71962: UBSan changes constant evaluation

Open [PR71962](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=71962).

Paste this comment:

```text
This bug also affects C++26 reflection and consteval code in GCC 16.1.0.

This standalone testcase reproduces the std::string(ptr, size) failure over vague-linkage storage:

#include <string>

inline constexpr char text[] = "x";

consteval auto check() -> bool {
  auto value = std::string{text, 1};
  return value == "x";
}

static_assert(check());

Command:

g++-16 -std=c++26 -fsanitize=undefined -c repro.cc

GCC rejects the static assertion. The final diagnostic is:

bits/basic_string.h:713:17: error: '(((const char*)(& text)) == 0)' is not a constant expression

The same command succeeds without -fsanitize=undefined.

Reflection APIs can expose text through compiler-generated storage with the same linkage property. This bug therefore blocks required constant evaluation in reflection code.

Attachment 65251, added on 2026-08-05, addresses the same separation between constant evaluation and null-check optimization. This testcase can extend its regression coverage.
```

Do not state that attachment 65251 fixes the reflection case. We did not test that patch against this case.
