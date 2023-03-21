# enclaveSilo

# About

Working silo protocol within enclave using Intel SGX SDK.<br>
Comparison program is [here](https://github.com/Noxy3301/silo_minimum).

# How to use
```
$ wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.19.100.3.bin
$ chmod 777 sgx_linux_x64_sdk_2.17.100.3.bin
$ ./sgx_linux_x64_sdk_2.17.100.3.bin
[yes]
$ source {ENVIRONMENT-PATH}   // indicated by green text. 
$ cd enclaveSilo
$ make
$ ./silo
```

If you try this code in datadock, please follow 2 steps.<br>
1. open "consts.h" in Include folder and modify `THREAD_NUM` to 224 and `CLOCKS_PER_US` to 2100.<br>
2. open "Enclave.config.xml" in Enclave folder and modify `TCSNum` to 225(THREAD_NUM + 1).<br>

If `TCSNum` == `THREAD_NUM`, ecall_sendQuit() is not work normally and never finishes the worker thread, so +1 is added.
