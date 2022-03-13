# 操作系统概论实验系统
## 概述
本仓库为重庆大学操作系统概论课程实验所用系统
原svn仓库已无法访问，故建立此仓库（fork自https://github.com/hongmingjian/epos ，并修正部分makefile问题），兼保存之用
注意，课程使用的PPT已长期未更新，关于实验环境配置部分请参考本文
## 环境配置方法
### Windows用户
#### 使用教师提供的环境（推荐）
下载教师统一提供的压缩包，解压后打开解压路径下的expenv文件夹，点击setvar.bat自动完成环境配置，请无视出现的cmd界面，只需确保执行完成即可。
另，由于该bat对用户环境变量进行了修改，有需求或已安装部分软件的可自行删除对应语句。
又另，不推荐使用提供的Notepad++，请使用其他编辑器进行代码编写及修改。
#### 使用本仓库并自行配置相关环境
参照setvar.bat中的需求，自行安装MinGW，msys，QEMU，Git等（未测试）
#### 使用虚拟机
自行安装虚拟机或wsl，然后使用Linux安装方法
### Linux用户(Debian/Ubuntu)
打开终端，执行以下指令
```
sudo apt-get install gcc binutils gdb
sudo apt-get install git
sudo apt-get install qemu-system-x86
```
克隆本仓库
```
git clone git@github.com:tyf12399/epos.git
```
测试运行
### Mac OS用户
（可选）安装Xcode的命令行工具 ”Command Line Tools for Xcode“
安装[Homebrew](https://github.com/Homebrew/brew)
在命令行中执行以下指令
```
brew tap hongmingjian/i586-elf-toolchain
brew install i586-elf-gcc
brew install i586-elf-gdb
brew install qemu
```
克隆本仓库
```
git clone git@github.com:tyf12399/epos.git
```
## 测试及使用
在epos目录下打开命令行，输入以下指令
```
make run
```
可能会要求输入密码，输入即可
若出现qemu窗口，并输出内容结尾类似`task #1: I'm the first user task(pv=0x12345678)`即编译运行成功
清除临时文件
```
make clean
```
编译并使用gdb调试
```
make clean debug
```
该命令预期将打开两个新窗口，分别为QEMU和gdb。
