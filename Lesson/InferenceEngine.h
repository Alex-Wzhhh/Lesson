#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#endif // INFERENCEENGINE_H

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

struct IEOptions {
    // 模型输入规格（通常从模型导出文档得知；也可在运行时读取动态输入）
    int input_width  = 224;
    int input_height = 224;
    bool keep_ratio  = false;   // true: letterbox，false: 直接resize
    bool swapRB      = true;    // BGR->RGB
    bool toFloat     = true;    // 转 float32
    float scale      = 1.0f/255.0f; // 缩放到0~1
    std::vector<float> mean{0.485f, 0.456f, 0.406f};
    std::vector<float> std{ 0.229f, 0.224f, 0.225f};
    int intra_op_num_threads = 0;    // 0=由 ORT 自适应
    int inter_op_num_threads = 0;
    bool enable_mem_pattern  = true;
    bool enable_arena        = true;
    bool enable_profiling    = false;
    int graph_optimization_level = 3; // 0~3，对应 ORT_DISABLE_ALL ~ ORT_ENABLE_ALL
    int topK = 5;                     // 分类取前K
    bool try_directml = true;         // 尝试 DML（Windows）
};

struct ClassProb {
    int   class_id;
    float prob;
};

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();

    // 加载模型（env 可全局复用；这里内部自持）
    bool load(const std::wstring& onnx_path, const IEOptions& opt = IEOptions(), std::string* err = nullptr);

    // 单张图像分类推理
    std::vector<ClassProb> inferClassify(const cv::Mat& bgr) const;

    // 批量推理（演示用）
    std::vector<std::vector<ClassProb>> inferClassifyBatch(const std::vector<cv::Mat>& images) const;

    // 你也可以暴露通用 Run，返回 Ort::Value，外部自定义后处理
    std::vector<Ort::Value> runRaw(const std::vector<Ort::Value>& inputs,
                                   const std::vector<const char*>& input_names,
                                   const std::vector<const char*>& output_names) const;

    // 查询输入输出信息
    std::vector<std::string> getInputNames() const;
    std::vector<std::string> getOutputNames() const;

    // 是否可用
    bool good() const { return session_ != nullptr; }

private:
    IEOptions options_;
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> so_;
    mutable Ort::AllocatorWithDefaultOptions allocator_;

    // 缓存输入/输出名字（可选）
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    // 预处理
    cv::Mat preprocess(const cv::Mat& bgr) const;
    // 将图像打包成 NCHW FP32 Tensor
    Ort::Value matToTensor(const cv::Mat& chwFloat) const;

    // 后处理：softmax + topK（分类）
    static std::vector<ClassProb> softmaxTopK(const float* logits, size_t len, int k);

    // 工具
    static int toOrtOptLevel(int lv); // 0~3 -> ORT enum
};
