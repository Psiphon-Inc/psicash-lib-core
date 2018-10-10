## Code Style

### C++

Mostly following [Google's Style Guide](https://google.github.io/styleguide/cppguide.html).

## Review notes

* I made no effort to be memory efficient (such as using move semantics, references, etc.). I tried to be clear and safe above efficiency. If there's a leak, that's a bug. If there's gross memory inefficiency, it can probably be improved. But I don't think we should bother with small efficiencies.


## Troubleshooting

If Android Studio is saying that a new `.cpp` file is not part of the project, go to the "Build" menu and click "Refresh Linked C++ Projects". Then rebuild.

If you get a `SIGABRT` error in JNI code: You have probably triggered a JNI exception (that hasn't been cleared). It's possible that it's expected or acceptable and you just need to clear it, but it's more likely that it's a bug.
