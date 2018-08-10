#!venv/bin/python2

from flask import Flask, jsonify, send_from_directory
from flask import abort
from flask import make_response
from flask import request

import os
import json
import subprocess

UPLOAD_FOLDER = 'uploads'
ALLOWED_EXTENSIONS = set(['stcm', 'mp3'])

basedir = os.path.abspath(os.path.dirname(__file__))

progfile = basedir + "/apollod"
taskfile = basedir + "/" + UPLOAD_FOLDER + "/" + "tasks.json"
exectask = basedir + "/" + UPLOAD_FOLDER + "/" + "exec.json"
cmapfile = basedir + "/" + UPLOAD_FOLDER + "/" + "cmap.stcm"
homefile = basedir + "/" + UPLOAD_FOLDER + "/" + "homepose.json"

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def writeToJSONFile(filepath, data):
    with open(filepath, 'w') as fp:
        json.dump(data, fp)


app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 20 * 1024 * 1024
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

tasks = []

with open(taskfile) as f:
    tasks = json.load(f)


@app.errorhandler(404)
def not_found(error):
    return make_response(jsonify({'errno': 404, 'msg': 'Not found'}), 404)


@app.errorhandler(400)
def not_found(error):
    return make_response(jsonify({'errno': 400, 'msg': 'Invalid Arguments'}), 400)


@app.route('/tasks', methods=['GET'])
def get_tasks():
    return jsonify({'tasks': tasks})


@app.route('/tasks/<int:task_id>', methods=['GET'])
def get_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
        abort(404)
    return jsonify({'task': task[0]})


@app.route('/tasks', methods=['POST'])
def create_task():
    if not request.json or not 'name' in request.json or not 'milestones' in request.json:
        abort(400)

    task = filter(lambda t: t['name'] == request.json['name'], tasks)
    print(task)
    if len(task) != 0:
        abort(400)

    task = {
        #'id': tasks[-1]['id'] + 1,
        'id': len(tasks),
        'name': request.json['name'],
        'milestones': request.json['milestones'],
        'audio1': request.json.get('audio1', ""),
        'audio2': request.json.get('audio2', "")
    }
    tasks.append(task)
    writeToJSONFile(taskfile, tasks)
    return jsonify({'task': task}), 201


@app.route('/tasks/<int:task_id>', methods=['PUT'])
def update_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
        abort(404)
    task[0]['name'] = request.json.get('name', task[0]['name'])
    task[0]['milestones'] = request.json.get('milestones', task[0]['milestones'])
    task[0]['audio1'] = request.json.get('audio1', task[0]['audio1'])
    task[0]['audio2'] = request.json.get('audio2', task[0]['audio2'])
    writeToJSONFile(taskfile, tasks)
    return jsonify({'task': task[0]})


@app.route('/tasks/<int:task_id>', methods=['DELETE'])
def delete_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
        abort(404)
    tasks.remove(task[0])
    writeToJSONFile(taskfile, tasks)
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/tasks/<int:task_id>', methods=['POST'])
def exec_task(task_id):
    task = filter(lambda t: t['id'] == task_id, tasks)
    if len(task) == 0:
        abort(404)
    writeToJSONFile(exectask, task[0])
    subprocess.Popen([progfile, "--task", exectask])
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/exec/gohome', methods=['POST'])
def exec_gohome():
    subprocess.call([progfile, "--cancel"])
    subprocess.Popen([progfile, "--gohome"])
    return jsonify({'errno': 0, 'msg': 'success'})


@app.route('/exec/cancel', methods=['POST'])
def exec_cancel():
    subprocess.call([progfile, "--cancel"])
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
    writeToJSONFile(homefile, home)
    return jsonify({'errno': 0, 'msg': 'success'})


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


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
