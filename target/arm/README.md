```sh
mkdir build && cd build

../configure --target-list=x86_64-softmmu,aarch64-softmmu \
             --enable-kvm \
             --enable-debug \
             --enable-virtfs



../configure --target-list=riscv64-softmmu,riscv32-softmmu \
             --enable-debug \
             --enable-debug-tcg \
             --disable-coroutine-pool \
             --extra-cflags="-O0 -g"


```
