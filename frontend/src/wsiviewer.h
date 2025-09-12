#pragma once
#include "WSIView.h"

// 兼容你的 .ui 中提升的类名 “WSIViewer”
class WSIViewer : public WSIView {
    Q_OBJECT
public:
    using WSIView::WSIView; // 直接复用基类构造函数
};
