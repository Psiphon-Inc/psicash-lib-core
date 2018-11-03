# PsiCash Client Library

This is the C++ core of the library. It can be used directly or wrapped with native-language glue. For example, for Android there is a Java (JNI) glue wrapper.

## TODO

* UserAgent string:
  - Add "Psiphon-PsiCash-Android" to server config

## Using the library

### Supplying the HTTP Requester

This library relies on the native environment to provide an HTTP request callback. Notes about its signature, inputs and outputs can be found in `psicash.hpp`.

There is an _example_ implementation in the Android wrapper project. But note that it _does not support proxied requests_, which may be necessary depending on the environment. (E.g., it probably doesn't matter on iOS, since our app only supported full-device VPN. But on Windows the app mostly uses a local proxy, so the HTTP Requester must support proxying.)

## Code Style

### C++

Mostly following [Google's Style Guide](https://google.github.io/styleguide/cppguide.html). Major alteration: 4-space indent instead of 2. THe `.clang-format` file should be crafted as necessary to match what we want.

## Review notes

* I made no effort to be memory efficient (such as explicitly using move semantics, references, etc.). I tried to be clear and safe above efficiency. If there's a leak, that's a bug. If there's gross memory inefficiency, it can probably be improved. But we shouldn't bother with small efficiencies.

## Troubleshooting

If Android Studio is saying that a new `.cpp` file is not part of the project, go to the "Build" menu and click "Refresh Linked C++ Projects". Then rebuild.

If you get a `SIGABRT` error in JNI code: You have probably triggered a JNI exception (that hasn't been cleared). It's possible that it's expected or acceptable and you just need to clear it, but it's more likely that it's a bug.

If you get a `SIGSEGV` error when hitting a breakpoint in JNI code: Yeah, beats me. I get it on MacOS but not Windows. Dunno.
