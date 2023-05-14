# xpwd

`xpwd` prints the working directory of the active window.

**Note**: this program is a fork of [xcwd](https://github.com/schischi/xcwd) by Adrien Schildknecht
adapted for my own needs.

The main goal is to launch applications directly into the same directory as the focused applications.
This is especially useful to open a new terminal or a file explorer.

This program is basically a hack, but it works well with my setup and I hope it will work for you as well :)

## Configuration

Just edit `xpwd.c` and change `process_blacklist` and `path_blacklist` to adapt the blacklist filter to your needs.
