from __future__ import annotations
import base64, io
from fastapi import FastAPI
from PIL import Image
import numpy as np
from typing import List
from inference.opencv_model import detect_bboxes, DetectConfig
from schemas import AnalyzeViewportReq, AnalyzeViewportResp, Box

app = FastAPI(title="Qt-Python Backend for WSI Tumor Recognition")

@app.get("/ping")
def ping():
    return {"status": "ok"}

@app.post("/analyze_viewport", response_model=AnalyzeViewportResp)
def analyze_viewport(req: AnalyzeViewportReq):
    # 解码 base64 -> PIL -> numpy (BGR)
    data = base64.b64decode(req.image_b64)
    pil_img = Image.open(io.BytesIO(data)).convert("RGB")
    rgb = np.array(pil_img)
    # OpenCV 使用 BGR
    bgr = rgb[:, :, ::-1]

    boxes = detect_bboxes(bgr, DetectConfig())
    out_boxes: List[Box] = [
        Box(x=float(x), y=float(y), w=float(w), h=float(h), label="tumor", score=float(score))
        for (x, y, w, h, score) in boxes
    ]
    h, w = rgb.shape[:2]
    return AnalyzeViewportResp(image_size=(int(w), int(h)), boxes=out_boxes)
