from flask import Flask, request, jsonify, Response
import cv2
import numpy as np
import time

app = Flask(__name__)

# Haar Cascades
face_cascade = cv2.CascadeClassifier('haarcascade_frontalface_default.xml')
eye_cascade = cv2.CascadeClassifier('haarcascade_eye.xml')

# Estados
closed_eyes_frames = 0
THRESHOLD_FRAMES_FATIGUE = 3
latest_frame = None


def eye_EAR(eye_img):
    """ Calcula o EAR do olho (aprox. usando contornos) """
    gray = cv2.cvtColor(eye_img, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (5, 5), 0)
    _, thresh = cv2.threshold(gray, 70, 255, cv2.THRESH_BINARY_INV)

    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if len(contours) == 0:
        return 0  # Olho fechado

    cnt = max(contours, key=cv2.contourArea)
    x, y, w, h = cv2.boundingRect(cnt)

    EAR = h / float(w)

    return EAR


@app.route("/frame", methods=["POST"])
def handle_frame():
    global closed_eyes_frames, latest_frame

    img_bytes = request.data
    np_arr = np.frombuffer(img_bytes, np.uint8)
    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

    if frame is None:
        return jsonify({"error": "invalid_image"}), 400

    latest_frame = frame.copy()

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.equalizeHist(gray)

    faces = face_cascade.detectMultiScale(
        gray, scaleFactor=1.2, minNeighbors=5, minSize=(80, 80)
    )

    eyes_detected = False
    eyes_open = False

    for (x, y, w, h) in faces:

        roi_gray = gray[y:y + int(h * 0.6), x:x + w]
        roi_color = frame[y:y + int(h * 0.6), x:x + w]

        eyes = eye_cascade.detectMultiScale(
            roi_gray,
            scaleFactor=1.45,
            minNeighbors=4,
            minSize=(20, 20)
        )

        EAR_values = []

        for (ex, ey, ew, eh) in eyes:
            eyes_detected = True
            eye_img = roi_color[ey:ey + eh, ex:ex + ew]

            EAR = eye_EAR(eye_img)
            EAR_values.append(EAR)

            cv2.rectangle(roi_color, (ex, ey), (ex + ew, ey + eh), (255, 0, 0), 2)
            cv2.putText(roi_color, f"EAR: {EAR:.2f}", (ex, ey - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 0), 1)

        # Definição final se os olhos estão abertos ou fechados
        if len(EAR_values) > 0:
            EAR_mean = sum(EAR_values) / len(EAR_values)
            eyes_open = EAR_mean > 0.20   # limiar ajustado
        else:
            eyes_open = False

        # desenha o rosto
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

    # ----------- LÓGICA DE FADIGA + CONTAGEM -----------
    if not eyes_detected:
        closed_eyes_frames += 1
    elif not eyes_open:
        closed_eyes_frames += 1
    else:
        closed_eyes_frames = 0

    status = "fadiga" if closed_eyes_frames >= THRESHOLD_FRAMES_FATIGUE else "alerta"

    return jsonify({
        "status": status,
        "closed_eyes_frames": closed_eyes_frames,
        "faces_detected": int(len(faces))
    })


@app.route("/view")
def view_stream():
    def generate():
        global latest_frame

        while True:
            if latest_frame is not None:
                ret, jpeg = cv2.imencode('.jpg', latest_frame)
                frame_bytes = jpeg.tobytes()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
            time.sleep(0.03)

    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == "__main__":
    print("Servidor iniciado!")
    print("POST → http://0.0.0.0:5000/frame")
    print("STREAM → http://0.0.0.0:5000/view")
    app.run(host="0.0.0.0", port=5000, debug=False)
