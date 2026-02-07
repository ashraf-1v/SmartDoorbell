from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# --- CONFIG ---
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_STATUS = "alabs/doorbell/status"
TOPIC_CONTROL = "alabs/doorbell/control"

status = {"door": "Locked", "alert": "None"}

def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected to MQTT!")
    client.subscribe(TOPIC_STATUS)

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"ESP Says: {payload}")
    
    if payload == "DOOR_LOCKED": status["door"] = "Locked"
    elif payload == "DOOR_UNLOCKED": status["door"] = "Unlocked"
    elif payload == "RINGING": status["alert"] = "Ding Dong!"
    elif payload == "BURGLAR_ALERT": status["alert"] = "Intruder!"
    
    socketio.emit('mqtt_message', {'data': payload})

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_start()

@app.route('/')
def index(): return render_template('index.html', status=status)

@app.route('/command', methods=['POST'])
def command():
    cmd = request.json.get('command')
    print(f"Sending Command: {cmd}")
    client.publish(TOPIC_CONTROL, cmd)
    return jsonify({"status": "sent"})

if __name__ == '__main__':
    socketio.run(app, debug=True, port=5000, allow_unsafe_werkzeug=True)