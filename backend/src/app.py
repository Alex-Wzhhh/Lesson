from fastapi import FastAPI, UploadFile, File, Form
from fastapi.responses import JSONResponse
from pydantic import BaseModel
import uvicorn
import cv2
import numpy as np
from pathlib import Path
from datetime import datetime

app = FastAPI(title="Qt-Python Backend")

BASE_DIR = Path(__file__).resolve().parent
OUT_DIR = BASE_DIR / "outputs"
OUT_DIR.mkdir(exist_ok=True)

class PathReq(BaseModel):
    image_path: str

@app.get("/ping")
def ping():
    return {"ok": True, "msg": "backend alive"}

def process_image_np(img_np: np.ndarray):
    """一个最简单的图像处理示例：转灰度 + Canny 边缘"""
    gray = cv2.cvtColor(img_np, cv2.COLOR_BGR2GRAY)
    edges = cv2.Canny(gray, 100, 200)
    # 把边缘叠加回三通道用于展示
    edges_rgb = cv2.cvtColor(edges, cv2.COLOR_GRAY2BGR)
    overlay = cv2.addWeighted(img_np, 0.8, edges_rgb, 0.2, 0)
    # 统计一个简单指标（边缘像素个数）
    edge_count = int(np.count_nonzero(edges))
    return overlay, edge_count

@app.post("/process/by-path")
def process_by_path(req: PathReq):
    p = Path(req.image_path)
    if not p.exists():
        return JSONResponse({"ok": False, "error": f"File not found: {p.as_posix()}"}, status_code=400)
    img = cv2.imread(p.as_posix())
    if img is None:
        return JSONResponse({"ok": False, "error": "cv2.imread failed"}, status_code=400)

    out_img, edge_count = process_image_np(img)
    out_name = f"out_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}.png"
    out_path = (OUT_DIR / out_name).as_posix()
    cv2.imwrite(out_path, out_img)

    return {
        "ok": True,
        "input_path": p.as_posix(),
        "output_path": out_path,
        "metrics": {"edge_count": edge_count}
    }

@app.post("/process/upload")
async def process_upload(file: UploadFile = File(...)):
    data = await file.read()
    arr = np.frombuffer(data, dtype=np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if img is None:
        return JSONResponse({"ok": False, "error": "imdecode failed"}, status_code=400)

    out_img, edge_count = process_image_np(img)
    out_name = f"out_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}.png"
    out_path = (OUT_DIR / out_name).as_posix()
    cv2.imwrite(out_path, out_img)

    return {
        "ok": True,
        "input_filename": file.filename,
        "output_path": out_path,
        "metrics": {"edge_count": edge_count}
    }

if __name__ == "__main__":
    uvicorn.run("app:app", host="127.0.0.1", port=5001, reload=False)
