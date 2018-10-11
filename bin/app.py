#!venv/bin/python2
# -*- coding: UTF-8 -*-


from flask import Flask, jsonify, send_from_directory
from flask import abort
from flask import make_response
from flask import request

import os
import json
import subprocess
import codecs
import time

from flask_cors import CORS

basedir = os.path.abspath(os.path.dirname(__file__))
rootdir = basedir + "/.."
upload_dir = rootdir + "/uploads"

progfile = rootdir + "/bin/apollod"

taskfile = upload_dir + "/tasks.json"
exectask = upload_dir + "/exec.json"
cmapfile = upload_dir + "/cmap.stcm"
homefile = upload_dir + "/homepose.json"

UPLOAD_FOLDER = upload_dir
ALLOWED_EXTENSIONS = set(['stcm', 'mp3'])

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def save_jsonfile(path, data):
    with codecs.open(path, 'w', 'utf-8') as fp:
        json.dump(data, fp, ensure_ascii=False, indent=4)



app = Flask(__name__)
CORS(app)

app.config['MAX_CONTENT_LENGTH'] = 20 * 1024 * 1024
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['JSON_AS_ASCII'] = False

tasks = []

try:
    with open(taskfile) as fp:
        tasks = json.load(fp)
except Exception as error:
    save_jsonfile(taskfile, tasks)

#print(json.dumps(tasks, ensure_ascii=False, indent=4))


def json_response(response):
    return make_response(jsonify(response), 200) 

@app.errorhandler(404)
def not_found(error):
    return make_response(jsonify({'errno': 404, 'msg': 'Not found'}), 404)


@app.errorhandler(400)
def not_found(error):
    return make_response(jsonify({'errno': 400, 'msg': 'Invalid Arguments'}), 400)


@app.route('/tasks', methods=['GET'])
def get_tasks():
    return make_response(jsonify({'tasks': tasks}), 200)


@app.route('/tasks', methods=['POST'])
def create_task():
    if not request.json or not 'name' in request.json or not 'milestones' in request.json:
        return json_response({ 'errno': 400, 'msg': u'任务格式错误'})

    audio1 = request.json.get('audio1')

    if audio1 and not os.path.exists(upload_dir + '/' + audio1):
    	return json_response({ 'errno': 400, 'msg': u'{} 文件不存在'.format(audio1)})

    if len(tasks) > 0:
        task_id = tasks[-1]['id']
    else: 
	    task_id = 0

    task = {
        'id': task_id + 1,
        'name': request.json['name'],
        'milestones': request.json['milestones'],
        'audio1': request.json.get('audio1', ""),
        'audio2': request.json.get('audio2', "")
    }
    tasks.append(task)
    save_jsonfile(taskfile, tasks)
    return json_response({ 'errno': 0, 'task': task})


@app.route('/tasks/<int:task_id>', methods=['GET'])
def get_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
    	return json_response({ 'errno': 400, 'msg': u'task {} not found'.format(task_id)})
    return json_response({ 'errno': 0, 'task': task[0]})


@app.route('/tasks/<int:task_id>', methods=['PUT'])
def update_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
    	return json_response({ 'errno': 400, 'msg': u'task {} not found'.format(task_id)})
    task[0]['name'] = request.json.get('name', task[0]['name'])
    task[0]['milestones'] = request.json.get('milestones', task[0]['milestones'])
    task[0]['audio1'] = request.json.get('audio1', task[0]['audio1'])
    task[0]['audio2'] = request.json.get('audio2', task[0]['audio2'])
    save_jsonfile(taskfile, tasks)
    return json_response({'errno': 0, 'msg': u'task {} update success'.format(task_id)})


@app.route('/tasks/<int:task_id>', methods=['DELETE'])
def delete_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
    	return json_response({ 'errno': 400, 'msg': u'task {} not found'.format(task_id)})
    tasks.remove(task[0])
    save_jsonfile(taskfile, tasks)
    return json_response({'errno': 0, 'msg': u'task {} delete success'.format(task_id)})


@app.route('/tasks/<int:task_id>', methods=['POST'])
def exec_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
    	return json_response({ 'errno': 400, 'msg': u'task {} not found'.format(task_id)})
    save_jsonfile(exectask, task[0])
    subprocess.Popen([progfile, "--task", exectask])
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/exec/gohome', methods=['POST'])
def exec_gohome():
    subprocess.call([progfile, "--cancel"])
    subprocess.call(["sleep", "1"])
    subprocess.Popen([progfile, "--gohome"])
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/exec/cancel', methods=['POST'])
def exec_cancel():
    subprocess.call([progfile, "--cancel"])
    subprocess.call(["sleep", "1"])
    subprocess.call("killall -9 mpg123", shell=True)
    subprocess.call("killall -9 apollod", shell=True)
    return jsonify({'errno': 0, 'msg': 'success'})

@app.route('/exec/poweroff', methods=['POST'])
def exec_poweroff():
    subprocess.call([progfile, "--cancel"])
    subprocess.Popen(["sleep 2 && poweroff"], shell=True)
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/home', methods=['GET'])
def get_home():
    home = []
    with open(homefile) as f:
        home = json.load(f)
    return jsonify(home)


@app.route('/home', methods=['PUT'])
def set_home():
    x = request.args.get('x')
    y = request.args.get('y')
    home = { 'x': x, 'y': y }
    save_jsonfile(homefile, home)
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/status', methods=['GET'])
def get_status():

    cmd = ["ping", "-c", "1", "-W", "1", "192.168.11.1"]
    try:
        subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        output = "network success. "
    except subprocess.CalledProcessError as e:
        output = "network failed. "

    cmd = ["cat", "/tmp/apollo.log"]
    try:
        output += subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        output += "booting ..."
    return jsonify({'errno': 0, 'msg': output})

@app.route('/volume', methods=['GET'])
def get_volume():
    cmd = "amixer -c 2 sget 'Speaker',0 | grep 'Right:' | awk -F'[][]' '{ print $2 }' | awk -F '%' '{ printf $1}'"
    vol = subprocess.check_output(cmd, shell=True)
    return jsonify({'errno': 0, 'volume': vol})


@app.route('/volume', methods=['PUT'])
def set_volume():
    vol = request.args.get('volume', 80)
    subprocess.call(["amixer", "-c", "2", "sset", "\'Speaker\',0", str(vol)+"%"])
    return jsonify({'errno': 0, 'msg': 'success'})

@app.route('/battery', methods=['GET'])
def get_battery():
    cmd = [progfile, "--battery"]
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        return jsonify({'errno': 0, 'battery': output})
    except subprocess.CalledProcessError as e:
        return json_response({ 'errno': 400, 'msg': 'get battery failed'})

@app.route('/map', methods=['GET'])
def get_map():
    abort(404)


@app.route('/map', methods=['POST'])
def post_map():
    file_dir = os.path.join(basedir, app.config['UPLOAD_FOLDER'])
    if not os.path.exists(file_dir):
        os.makedirs(file_dir)
    try:
        f = request.files['file']
    except:
        f = None
    if f and allowed_file(f.filename):
        f.save(os.path.join(file_dir, 'cmap.stcm'))

        home = []
        with open(homefile) as f:
            home = json.load(f)
        x = home['x']
        y = home['y']
        subprocess.Popen([progfile, "--loadmap", cmapfile, "-x", x, "-y", y])
        return jsonify({"errno":0, "msg": "success"})
    else:
        return jsonify({"errno":1001, "msg":u"failed"})


@app.route('/map', methods=['PUT'])
def update_map():
    home = []
    with open(homefile) as f:
        home = json.load(f)
    x = home['x']
    y = home['y']
    subprocess.call([progfile, "--loadmap", cmapfile, "-x", x, "-y", y])
    return jsonify({"errno":0, "msg": "success"})


@app.route('/upload', methods=['POST'])
def upload_file():
    file_dir = os.path.join(basedir, app.config['UPLOAD_FOLDER'])
    if not os.path.exists(file_dir):
        os.makedirs(file_dir)
    try:
        f = request.files['file']
    except:
        f = None
    if f and allowed_file(f.filename):
        f.save(os.path.join(file_dir, f.filename))
        return jsonify({"errno":0, "msg": "success"})
    else:
        return jsonify({"errno":1001, "msg":u"failed"})


@app.route('/uploads/<filename>')
def uploaded_file(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

@app.route('/time', methods=['GET'])
def get_time():
    timestamp = int(time.time())
    timestr = time.strftime("%Y-%m-%d %H:%M:%S")
    return jsonify({'errno': 0, 'timestamp': timestamp, 'timestr': timestr})

@app.route('/time', methods=['PUT'])
def put_time():
    timestamp = int(request.args.get('timestamp', 0))
    tmp_date = time.strftime("%Y-%m-%d", time.localtime(timestamp))
    tmp_time = time.strftime("%H:%M:%S", time.localtime(timestamp))
    tmp_cmd = 'date -s "{0} {1}"'.format(tmp_date, tmp_time)

    try:
        subprocess.check_output(tmp_cmd, stderr=subprocess.STDOUT, shell=True)
        os.system("hwclock -w")
        msg = 'success'
        err = 0
    except subprocess.CalledProcessError as e:
        msg = e.output.strip()
        err = 100

    timestr = time.strftime("%Y-%m-%d %H:%M:%S")
    return jsonify({ 'errno': err, 'msg': msg, 'timestr': timestr })


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
