from __future__ import annotations
import base64, io, threading, itertools
from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import StreamingResponse
from PIL import Image
import numpy as np
from typing import List, Dict, Tuple

# 现有导入（保留）
from inference.opencv_model import detect_bboxes, DetectConfig
from schemas import AnalyzeViewportReq, AnalyzeViewportResp, Box

# === 新增：OpenSlide-only-in-Python ===
try:
    import openslide
except Exception as e:
    openslide = None

app = FastAPI(title="Qt-Python Backend for WSI Tumor Recognition")

@app.get("/ping")
def ping():
    return {"status": "ok"}

# ---- 你现有的识别接口（保留） ----
@app.post("/analyze_viewport", response_model=AnalyzeViewportResp)
def analyze_viewport(req: AnalyzeViewportReq):
    data = base64.b64decode(req.image_b64)
    pil_img = Image.open(io.BytesIO(data)).convert("RGB")
    rgb = np.array(pil_img)
    bgr = rgb[:, :, ::-1]
    boxes = detect_bboxes(bgr, DetectConfig())
    out_boxes: List[Box] = [
        Box(x=float(x), y=float(y), w=float(w), h=float(h), label="tumor", score=float(score))
        for (x, y, w, h, score) in boxes
    ]
    h, w = rgb.shape[:2]
    return AnalyzeViewportResp(image_size=(int(w), int(h)), boxes=out_boxes)

# ----------------- 新增：WSI 服务 -----------------

_SLIDES: Dict[int, openslide.OpenSlide] = {}
_META: Dict[int, dict] = {}
_LOCK = threading.Lock()
_GEN = itertools.count(1)

def _ensure_openslide():
    if openslide is None:
        raise HTTPException(status_code=500, detail="openslide 未安装：请在后端执行 conda install -c conda-forge openslide openslide-python")

@app.post("/open_wsi")
def open_wsi(path: str):
    """
    传入本机的 WSI 文件路径（如 .svs / 金字塔 .tif）。
    返回 slide_id 与层级元数据。前端只保留这个 id，以后按 id 拉取区域图像。
    """
    _ensure_openslide()
    try:
        slide = openslide.OpenSlide(path)
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"OpenSlide 打开失败: {e}")

    level_count = slide.level_count
    level_dims = [slide.level_dimensions[i] for i in range(level_count)]
    downsamples = [slide.level_downsamples[i] for i in range(level_count)]
    props = {k: slide.properties.get(k) for k in [
        openslide.PROPERTY_NAME_MPP_X,
        openslide.PROPERTY_NAME_MPP_Y,
        openslide.PROPERTY_NAME_VENDOR,
        "aperio.AppMag",
    ] if hasattr(openslide, 'PROPERTY_NAME_MPP_X')}

    with _LOCK:
        sid = next(_GEN)
        _SLIDES[sid] = slide
        _META[sid] = {
            "path": path,
            "level_count": level_count,
            "level_dimensions": level_dims,
            "level_downsamples": downsamples,
            "properties": props,
        }
    return {"id": sid, **_META[sid]}

@app.get("/region")
def read_region(id: int = Query(...),
                level: int = Query(0, ge=0),
                x: int = Query(0, ge=0),
                y: int = Query(0, ge=0),
                w: int = Query(..., gt=0),
                h: int = Query(..., gt=0)):
    """
    读取指定 slide 的 level 层，从 (x,y) 处取 w*h 区域，返回 PNG。
    坐标是该 level 的坐标（不是 level0 坐标），这样前端换层时不用换算。
    """
    _ensure_openslide()
    with _LOCK:
        slide = _SLIDES.get(id)
    if slide is None:
        raise HTTPException(status_code=404, detail="无此 slide id，请先 /open_wsi")

    if level < 0 or level >= slide.level_count:
        raise HTTPException(status_code=400, detail="level 越界")

    # 将 level 坐标换成 level0 坐标读取
    down = slide.level_downsamples[level]
    lx = int(round(x * down))
    ly = int(round(y * down))
    try:
        region = slide.read_region((lx, ly), level, (w, h))  # 返回 PIL Image RGBA
        img = region.convert("RGB")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"read_region 失败: {e}")

    buf = io.BytesIO()
    img.save(buf, format="PNG")
    buf.seek(0)
    return StreamingResponse(buf, media_type="image/png")

@app.get("/tile")
def read_tile(id: int = Query(...),
              level: int = Query(0, ge=0),
              tx: int = Query(..., ge=0),
              ty: int = Query(..., ge=0),
              tile: int = Query(512, gt=0)):
    """
    DeepZoom 风格：返回 (level, tx, ty) 的 tile（大小 tile*tile）。
    """
    return read_region(id=id, level=level, x=tx*tile, y=ty*tile, w=tile, h=tile)

