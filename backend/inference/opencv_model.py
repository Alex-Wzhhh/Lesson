# OpenCV 伪“模型”：针对H&E病理图像中紫/粉区域的简单阈值 + 形态学 + 轮廓检测，输出 bbox。
# 用于课程原型演示，非医学用途。
from __future__ import annotations
import cv2 as cv
import numpy as np
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class DetectConfig:
    # HSV 阈值（OpenCV H:[0,179], S:[0,255], V:[0,255]）
    # 针对H&E常见的紫/粉核染色取一个较宽的范围，用户可在前端设置后传入后端（这里写死默认值）。
    h_low: int = 110
    h_high: int = 170
    s_low: int = 30
    s_high: int = 255
    v_low: int = 20
    v_high: int = 255

    # 形态学与面积阈值
    open_kernel: int = 3
    close_kernel: int = 5
    min_area: int = 60
    max_area: int = 100000  # 允许较大连通域，防止过度过滤

def detect_bboxes(bgr_img: np.ndarray, cfg: DetectConfig = DetectConfig()) -> List[Tuple[int,int,int,int,float]]:
    """返回 (x, y, w, h, score) 列表，坐标基于输入图像像素。"""
    if bgr_img is None or bgr_img.size == 0:
        return []
    # 降噪
    blur = cv.medianBlur(bgr_img, 3)
    hsv = cv.cvtColor(blur, cv.COLOR_BGR2HSV)
    lower = np.array([cfg.h_low, cfg.s_low, cfg.v_low], dtype=np.uint8)
    upper = np.array([cfg.h_high, cfg.s_high, cfg.v_high], dtype=np.uint8)
    mask = cv.inRange(hsv, lower, upper)

    # 形态学
    if cfg.open_kernel > 0:
        kernel = cv.getStructuringElement(cv.MORPH_ELLIPSE, (cfg.open_kernel, cfg.open_kernel))
        mask = cv.morphologyEx(mask, cv.MORPH_OPEN, kernel, iterations=1)
    if cfg.close_kernel > 0:
        kernel2 = cv.getStructuringElement(cv.MORPH_ELLIPSE, (cfg.close_kernel, cfg.close_kernel))
        mask = cv.morphologyEx(mask, cv.MORPH_CLOSE, kernel2, iterations=1)

    # 轮廓 -> bbox
    contours, _ = cv.findContours(mask, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)
    h, w = mask.shape[:2]
    bboxes: List[Tuple[int,int,int,int,float]] = []
    for c in contours:
        area = cv.contourArea(c)
        if area < cfg.min_area or area > cfg.max_area:
            continue
        x, y, bw, bh = cv.boundingRect(c)
        # 简单评分：IoU 与 mask 的贴合度/长宽比惩罚
        rect_mask = np.zeros((h, w), dtype=np.uint8)
        cv.rectangle(rect_mask, (x, y), (x+bw, y+bh), 255, -1)
        inter = cv.bitwise_and(mask, rect_mask)
        iou_like = float(np.sum(inter > 0)) / float(bw*bh + 1e-6)
        aspect = bw / (bh + 1e-6)
        aspect_penalty = np.exp(-abs(aspect - 1.0))  # 偏正方形得分高一点点
        score = max(0.0, min(1.0, 0.6*iou_like + 0.4*aspect_penalty))
        bboxes.append((x, y, bw, bh, score))
    return bboxes
