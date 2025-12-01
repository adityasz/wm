Unfortunately, neither
[gcc](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122625) nor
[clang](https://github.com/llvm/llvm-project/issues/170099) can handle modules
in their latest releases.

It took a long time to get this branch to build only for the compiler to crash
in the release build and the standard library not being able to handle PIC in
the debug build.
