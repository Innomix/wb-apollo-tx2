# 智慧移动货架

我们的开发工作主要有两部分：

1. 运行于上位机（Nvidia Jetson TX2 开发套件）上的服务进程
2. 运行于 Android 手机上的 app 程序

Android 端的 app 的工作与 web 端相同，调用上位机提供的 http 接口。

本文档主要描述上位机服务进程的部署、实现过程。

## 上位机服务

上位机功能：

1. 任务的查、增、删
2. 执行任务（并播放指定 mp3）、取消
3. 上传地图
4. 回桩

上位机接口文档：

    https://documenter.getpostman.com/view/5030725/RWThU1TS

### 打包

```
./res/pack.sh 
```

会在当前目录下生成以当前日期为名字的压缩包，如 `apollo_20181008.tar.bz` 。

例：

```
➜  apollo ./res/pack.sh 
/Users/meishaoming/workspace/apollo
➜  apollo ls
README.md               bin                     src                     uploads
apollo_20181008.tar.bz2 res                     tests
```

### 部署

工作目录位于 `/opt/apollo` 。解压压缩包手，替换掉该目录即可（注意先把压缩包复制到上位机里再进行解压、替换操作）。

例：

```
rm -rf /opt/apollo
mv webank_apollod-master /opt/apollo
```

（例子中假设解压后的目录名为 `webank_apollod-master`）

### 实现

上位机中的服务分两个部分实现：

1. http 接口部分，使用 python flask 框架。代码详见 `bin/app.py ` 。
2. 控制思岚底盘部分 `bin/apollod`，使用 [思岚 SDK](https://www.slamtec.com/cn/Support#apollo)，由 C++ 实现，编译环境参见 [思岚 SDK API 文档](https://wiki.slamtec.com/pages/viewpage.action?pageId=1016252)。

## 上位机系统的修改

Nvidia Jetson TX2 中运行了一个由 Nvidia 定制过的 Ubuntu 14.04 系统。为了保证我们的服务运行，在系统中增加（或修改）了几个配置文件。

这些工作提前配置于系统中，再制作出用于批量烧写的 image。制作和烧写的方法参见 Nvidia 的官方文档。

最终批量的上位机中烧录的系统都已配置过，无需再关心。此处只为记录目的。

### http 服务

`bin/app.py ` 以系统服务运行，受系统 systemctl 监控。

```
# 守护进程服务
cp res/apollo-app.service /etc/systemd/system/apollo-app.service
sudo systemctl enable apollo-app
sudo systemctl start apollo-app
```

### 开机加载地图

```
sudo cp res/rc.local /etc/rc.local
```

### 播放 mp3

安装 mpg123

```
sudo apt install mpg123
```

设置音量

```
amixer -c 2 sset 'Speaker',0 100%
amixer -c 2 sget 'Speaker',0 | grep 'Right:' | awk -F'[][]' '{ print $2 }' | awk -F '%' '{ print $1}'
```

播放音频

```
mpg123 -a hw:2,0 1.mp3
```

### 设置 eth0 静态 ip

上位机的以太网口配置成静态 IP：`192.168.11.101` 。

```
cp res/interfaces /etc/network/interfaces
```

### 设置系统 locale 支持中文

```
sudo update-locale LANG=zh_CN.UTF-8
sudo update-locale LC_CTYPE=zh_CN.UTF-8
```

## python 环境搭建

使用 virtualenv 搭建 python 运行环境，以避免对系统的信赖

```
sudo apt-get install python-virtualenv

virtualenv venv

. venv/bin/activate

pip install Flask
```



