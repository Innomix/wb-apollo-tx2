
## api

https://documenter.getpostman.com/view/5030725/RWThU1TS

## 系统部署

工作目录位于 `/opt/apollo`

http 服务

```
# 守护进程服务
cp res/apollo-app.service /etc/systemd/system/apollo-app.service
sudo systemctl enable apollo-app
sudo systemctl start apollo-app

# 开机加载地图
sudo cp res/rc.local /etc/rc.local

# 安装 mpg123
sudo apt install mpg123

# 设置 eth0 静态 ip

cp res/interfaces /etc/network/interfaces

# 设置 locale
sudo update-locale LANG=zh_CN.UTF-8
sudo update-locale LC_CTYPE=zh_CN.UTF-8
```


## 环境搭建

python 环境搭建

```
sudo apt-get install python-virtualenv

virtualenv venv

. venv/bin/activate

pip install Flask
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

http 调试接口

```
curl --request GET --url http://192.168.31.207:5000/tasks

curl --request GET --url http://192.168.31.207:5000/volume

curl --request PUT --url 'http://192.168.31.207:5000/volume?volume=10'

curl --request POST --url http://192.168.31.207:5000/tasks/6

curl --request POST --url http://192.168.31.207:5000/exec/poweroff

curl --request POST --url http://192.168.31.207:5000/exec/gohome

curl --request POST --url http://192.168.31.207:5000/exec/cancel

curl --request PUT --url http://127.0.0.1:5000/map


curl --request DELETE --url http://192.168.31.207:5000/tasks/0
```




