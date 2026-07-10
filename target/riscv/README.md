

```sh
mkdir build-debug && cd build-debug

../configure --target-list=riscv64-softmmu \
             --enable-debug \
             --enable-debug-tcg \
             --enable-sanitizers \
             --enable-coroutine-pool

make -j$(nproc)
```
