### trezor-firmware legacy本地编译调试过程

以下操作在ubuntu-18.04.3-desktop-amd64系统下。

以下操作基于源码中ci/Dockerfile和legac/script/cibuild整理。

环境安装：

-   apt-get update

\#安装git、make、wget

-   sudo apt-get -y install git make wget

\#安装protoc

-   sudo apt-get -y install protobuf-compiler

\#安装python

-   sudo apt-get -y install python3-dev python3-pip

-   ln -s /usr/bin/python3 /usr/bin/python  //创建链接

-   pip3 install pipenv

\#安装其他依赖库

-   sudo apt-get install libsdl2-dev libsdl2-image-dev //emulator 依赖

\#安装交叉编译工具arm-none-eabi-gcc

[官网地址：]<https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads>

-   wget <https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/9-2019q4/RC2.1/gcc-arm-none-eabi-9-2019-q4-major-x86_64-linux.tar.bz2>

-   tar xjf gcc-arm-none-eabi-9-2019-q4-major-x86\_64-linux.tar.bz2

\#添加工具链路径，按照自己的路径修改

-   export PATH=\"\$PATH:/<font color=#FF0000 >your-dir</font>/gcc-arm-none-eabi-9-2019-q4-major/bin/\" \>\>\~/.bashrc

-   source \~/.bashrc

\#下载编译源码

-   git clone <https://github.com/trezor/trezor-firmware.git>

-   cd trezor-firmware/

-   pipenv install

-   cd legacy/

-   make vendor \#下载其他依赖库

\#编译设备bootloader和firmware

-   make -C vendor/libopencm3 lib/stm32/f2 //编译stm32驱动库

-   make

-   make -C bootloader/ align

-   make -C vendor/nanopb/generator/proto

-   pipenv run make -C firmware/protob

-   pipenv run make -C firmware

\#编译EMULATOR

emulator是一个可以直接在Linux下运行的设备模拟器，如果编译过设备的bootLoader和firmware,需要将原来编译的中间文件重新删除后再编译。以下操作在legacy/目录下。

-   make clean

-   make -C firmware/ clean

-   export EMULATOR=1

-   make

-   make -C emulator

-   make -C vendor/nanopb/generator/proto

-   pipenv run make -C firmware/protob

-   pipenv run make -C firmware

在firmware/目录下生成trezor.elf可执行文件，运行模拟器：

- firmware/trezor.elf

### trezor core emulator编译过程

trezor core使用了scons来管理工程，需要安装scons

-   sudo apt-get install scons

-   sudo apt-get install libsdl2-dev libsdl2-image-dev

-   git clone <https://github.com/trezor/trezor-firmware.git>

-   cd trezor-firmware

-   pipenv sync //需要已安装好python、pip、pipenv，参考legacy部分

-   make vendor// leagcy下载过无需再下载

-   pipenv run make build\_unix

运行模拟器：

- ./emu.sh
