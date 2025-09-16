from pydantic import BaseModel, Field
from typing import List, Optional

class Box(BaseModel):
    x: float
    y: float
    w: float
    h: float
    label: str = "tumor"
    score: float = 0.0

class AnalyzeViewportReq(BaseModel):
    image_b64: str = Field(..., description="Base64 编码的 PNG 或 JPEG 图像（视口图像）")
    slide_id: Optional[int] = Field(
        None, ge=1, description="来自 /open_wsi 的 slide id；提供后端可换算到 level0 坐标"
    )
    level: int = Field(0, ge=0, description="本次视口图像所属的金字塔层级（默认0）")
    origin_x: float = Field(
        0.0, description="视口左上角在给定 level 下的 x 坐标（像素）"
    )
    origin_y: float = Field(
        0.0, description="视口左上角在给定 level 下的 y 坐标（像素）"
    )


class AnalyzeViewportResp(BaseModel):
    image_size: tuple[int, int]
    boxes: List[Box]
