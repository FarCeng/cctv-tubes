#Kelompok Tugas Besar- Farrel (21) & Abizar (51)
import os
from flask import Flask, render_template, request, jsonify, Response
from datetime import datetime

app = Flask(__name__)

# Konfigurasi folder untuk menyimpan gambar hasil capture
UPLOAD_FOLDER = os.path.join('static', 'uploads')
if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

# Data JSON untuk kontrol esp32
esp32_state = {
    "servo_pan": 90,
    "servo_tilt": 90,
    "led_intensity": 0,
    "stream": False,
    "capture": False 
}

# Variabel global untuk menampung frame video dari ESP32 untuk di-stream ke web
latest_frame = None

# Route Dashboard
@app.route('/')
def index():
    return render_template('index.html', current_state=esp32_state)

@app.route('/images')
def image_gallery():
    try:
        image_files = sorted(
            [f for f in os.listdir(UPLOAD_FOLDER) if f.endswith(('.jpg', '.jpeg'))],
            key=lambda x: os.path.getmtime(os.path.join(UPLOAD_FOLDER, x)),
            reverse=True
        )
    except OSError:
        image_files = []
    return render_template('gallery.html', images=image_files)

# Route API Komunikasi HTML dengan Flask
@app.route('/update_controls', methods=['POST'])
def update_controls():
    global esp32_state
    data = request.json
    # Menerjemahkan data dari web ke format yang dimengerti ESP
    esp32_state['servo_pan'] = int(data['pan'])
    esp32_state['servo_tilt'] = int(data['tilt'])
    esp32_state['led_intensity'] = int(data['cahaya'])
    
    print(f"Dashboard Update: {esp32_state}")
    return jsonify({"status": "success"})

@app.route('/toggle_stream', methods=['POST'])
def toggle_stream():
    global esp32_state
    esp32_state['stream'] = not esp32_state['stream']
    status = "ON" if esp32_state['stream'] else "OFF"
    print(f"Stream toggled to: {status}")
    return jsonify({"streaming": esp32_state['stream']})

@app.route('/trigger_capture', methods=['POST'])
def trigger_capture():
    global esp32_state
    esp32_state['capture'] = True
    print("Capture command set to True")
    return jsonify({"status": "capture signal sent"})

# Fungsi untuk Stream
def video_stream_generator():
    global latest_frame
    while True:
        if latest_frame:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + latest_frame + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(video_stream_generator(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

# Route API Komunikasi Server dengan ESP32 Cam
@app.route('/get-commands', methods=['GET'])
def get_commands():
    global esp32_state
    commands_to_send = esp32_state.copy()
    # Setelah ESP32 mengambil perintah capture, reset flag-nya menjadi False
    # agar tidak mengambil gambar terus-menerus.
    if esp32_state['capture']:
        esp32_state['capture'] = False
    # Mengirim data JSON dengan format yang sama persis seperti yang diharapkan C++.
    return jsonify(commands_to_send)

@app.route('/upload-image', methods=['POST'])
def upload_image():
    global latest_frame # Tambahkan ini untuk memodifikasi variabel global
    if request.content_type != 'image/jpeg':
        return "Content-Type not supported!", 400
    
    # Simpan data gambar untuk stream dan untuk file
    image_data = request.data
    latest_frame = image_data # <-- BARIS KUNCI: Update frame terbaru

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"capture_{timestamp}.jpg"
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    
    with open(filepath, 'wb') as f:
        f.write(image_data) # Gunakan data yang sudah disimpan di variabel
  
    print(f"Image received and saved as {filename}")
    return "Image uploaded successfully", 200

@app.route('/upload-frame', methods=['POST'])
def upload_frame():
    global latest_frame
    if request.content_type == 'image/jpeg':
        latest_frame = request.data
        return "Frame received", 200
    return "Invalid frame data", 400


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)