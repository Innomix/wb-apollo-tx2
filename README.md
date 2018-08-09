
## api

https://documenter.getpostman.com/view/5030725/RWThU1TS

## 环境搭建

```
sudo apt-get install python-virtualenv

virtualenv venv

. venv/bin/activate

pip install Flask
```

 
安装 mpg123

```
sudo apt install mpg123

mpg123 -a hw:2,0 1.mp3
```

设置 eth0 ip

/etc/network/interfaces

```
auto eth0
    iface eth0 inet static
    address 192.168.11.101
    netmask 255.255.255.0
    gateway 192.168.11.1
```



配置 alsa

~/.asoundrc

```
pcm.!default {
    type hw
    card 2
}

ctl.!default {
    type hw           
    card 2
}
```

设置音量

```
amixer -c 2 sset 'Speaker',0 100%
```

