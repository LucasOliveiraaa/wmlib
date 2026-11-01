# wmlib

A C library for building X11 window managers.

`wmlib` handles the protocol-level details of window management, allowing developers to focus on **policy and behavior**: window layouts, focus management, key bindings, and other decisions that define how a window manager works.

## Building

Requirements:
- C11 compiler
- CMake 3.14+
- Xlib and Xrandr development headers

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

## Acknowledgments

The [`dwm`](https://dwm.suckless.org/) project served as an implementation reference during development, particularly for understanding X11 window manager conventions and default behaviors.

## License

MIT. See [LICENSE](LICENSE).