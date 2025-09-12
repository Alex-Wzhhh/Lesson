from pydantic import BaseModel, Field
from typing import List

class Box(BaseModel):
    x: float
    y: float
    w: float
    h: float
    label: str = "tumor"
    score: float = 0.0

class AnalyzeViewportReq(BaseModel):
    image_b64: str = Field(..., description="Base64 编码的 PNG 或 JPEG 图像（视口图像）")

class AnalyzeViewportResp(BaseModel):
    image_size: tuple[int, int]
    boxes: List[Box]
