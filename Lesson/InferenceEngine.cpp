#ifndef INFERENCEENGINE_CPP
#define INFERENCEENGINE_CPP

#endif // INFERENCEENGINE_CPP

#include "InferenceEngine.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

// 可选：启用 DirectML Provider（不同 ORT 版本头文件位置可能不同）
#ifdef USE_DML_PROVIDER
#include <onnxruntime_c_api.h>
#include <onnxruntime/core/providers/dml/dml_provider_factory.h>
#endif

namespace {
inline std::wstring str2w(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}
}

InferenceEngine::InferenceEngine() : allocator_() {}

InferenceEngine::~InferenceEngine() = default;

int InferenceEngine::toOrtOptLevel(int lv) {
    switch (lv) {
    case 0: return ORT_DISABLE_ALL;
    case 1: return ORT_ENABLE_BASIC;
    case 2: return ORT_ENABLE_EXTENDED;
    default:return ORT_ENABLE_ALL;
    }
}

bool InferenceEngine::load(const std::wstring& onnx_path, const IEOptions& opt, std::string* err) {
    try {
        options_ = opt;

        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ie");
        so_  = std::make_unique<Ort::SessionOptions>();
        so_->SetIntraOpNumThreads(options_.intra_op_num_threads);
        so_->SetInterOpNumThreads(options_.inter_op_num_threads);
        so_->SetGraphOptimizationLevel(static_cast<GraphOptimizationLevel>(toOrtOptLevel(options_.graph_optimization_level)));

        if (!options_.enable_mem_pattern) {
            so_->DisableMemPattern();
        }
        if (!options_.enable_arena) {
            so_->DisableCpuMemArena();
        }
        if (options_.enable_profiling) {
            so_->EnableProfiling("ort_profile.json");
        }

        // Provider: 优先 DirectML（Windows），失败回退 CPU
        bool dml_ok = false;
#ifdef USE_DML_PROVIDER
        if (options_.try_directml) {
            auto* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
            OrtSessionOptions* raw_so = (*so_).release(); // 小心所有权，我们再包回去
            OrtStatus* st = OrtSessionOptionsAppendExecutionProvider_DML(raw_so, 0);
            (*so_) = Ort::SessionOptions(raw_so);
            if (st == nullptr) dml_ok = true;
        }
#endif
        // 无论是否启用 DML，CPU provider 总在；不需要显式追加 CPU

        session_ = std::make_unique<Ort::Session>(*env_, onnx_path.c_str(), *so_);

        // 缓存 I/O 名字
        size_t in_count  = session_->GetInputCount();
        size_t out_count = session_->GetOutputCount();
        input_names_.clear(); output_names_.clear();
        input_names_.reserve(in_count);
        output_names_.reserve(out_count);

        for (size_t i=0;i<in_count;++i) {
            auto name = session_->GetInputNameAllocated(i, allocator_);
            input_names_.push_back(name.get());
        }
        for (size_t i=0;i<out_count;++i) {
            auto name = session_->GetOutputNameAllocated(i, allocator_);
            output_names_.push_back(name.get());
        }

        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        session_.reset(); so_.reset(); env_.reset();
        return false;
    }
}

cv::Mat InferenceEngine::preprocess(const cv::Mat& bgr) const {
    CV_Assert(!bgr.empty());

    cv::Mat img = bgr;
    if (options_.keep_ratio) {
        // letterbox 到目标尺寸
        float r = std::min(
            options_.input_width  / static_cast<float>(img.cols),
            options_.input_height / static_cast<float>(img.rows)
            );
        int newW = static_cast<int>(std::round(img.cols * r));
        int newH = static_cast<int>(std::round(img.rows * r));

        cv::Mat resized;
        cv::resize(img, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

        cv::Mat canvas(options_.input_height, options_.input_width, img.type(), cv::Scalar(114,114,114));
        int x = (options_.input_width  - newW) / 2;
        int y = (options_.input_height - newH) / 2;
        resized.copyTo(canvas(cv::Rect(x,y,newW,newH)));
        img = canvas;
    } else {
        cv::resize(img, img, cv::Size(options_.input_width, options_.input_height), 0,0, cv::INTER_LINEAR);
    }

    if (options_.swapRB) {
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
    }

    cv::Mat img_float;
    if (options_.toFloat) {
        img.convertTo(img_float, CV_32F, options_.scale);
    } else {
        img_float = img;
    }

    // HWC -> CHW
    std::vector<cv::Mat> chw(3);
    for (int c=0;c<3;++c) chw[c].create(options_.input_height, options_.input_width, CV_32F);
    cv::split(img_float, chw);

    if (options_.toFloat && options_.mean.size()==3 && options_.std.size()==3) {
        for (int c=0; c<3; ++c) {
            chw[c] = (chw[c] - options_.mean[c]) / options_.std[c];
        }
    }

    cv::Mat blob(3, options_.input_height*options_.input_width, CV_32F);
    for (int c=0;c<3;++c) {
        std::memcpy(blob.ptr<float>(c),
                    chw[c].data,
                    options_.input_height * options_.input_width * sizeof(float));
    }
    // 现在 blob 形状是 (C, H*W) 的视图，稍后我们用 CreateTensor 时按 NCHW 解释
    return blob; // 实际数据是连续 C*(H*W)
}

Ort::Value InferenceEngine::matToTensor(const cv::Mat& chwFloat) const {
    // 构造 NCHW 维度
    std::array<int64_t,4> dims = {1, 3, options_.input_height, options_.input_width};
    size_t data_count = static_cast<size_t>(3 * options_.input_height * options_.input_width);
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    // 注意：chwFloat 的数据已经按 C*(H*W) 连续
    return Ort::Value::CreateTensor<float>(memory_info,
                                           const_cast<float*>(reinterpret_cast<const float*>(chwFloat.data)),
                                           data_count,
                                           dims.data(), dims.size());
}

std::vector<ClassProb> InferenceEngine::softmaxTopK(const float* logits, size_t len, int k) {
    // 数值稳定：先减 max
    float maxv = *std::max_element(logits, logits+len);
    std::vector<float> exps(len);
    float sum = 0.f;
    for (size_t i=0;i<len;++i) {
        exps[i] = std::exp(logits[i] - maxv);
        sum += exps[i];
    }
    for (auto& v : exps) v /= sum;

    // topK
    std::vector<int> idx(len);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin()+std::min<int>(k,len), idx.end(),
                      [&](int a, int b){ return exps[a] > exps[b]; });

    std::vector<ClassProb> out;
    for (int i=0; i<std::min<int>(k,len); ++i) {
        out.push_back({ idx[i], exps[idx[i]] });
    }
    return out;
}

std::vector<ClassProb> InferenceEngine::inferClassify(const cv::Mat& bgr) const {
    CV_Assert(session_!=nullptr);
    cv::Mat blob = preprocess(bgr);
    auto input_tensor = matToTensor(blob);

    // 默认取第0个输入输出；实际工程可根据名字匹配
    std::vector<const char*> in_names, out_names;
    in_names.push_back(input_names_.empty()? "input" : input_names_[0].c_str());
    out_names.push_back(output_names_.empty()? "output": output_names_[0].c_str());

    auto outputs = session_->Run(Ort::RunOptions{nullptr},
                                 in_names.data(), &input_tensor, 1,
                                 out_names.data(), 1);

    // 假设输出形状为 [1, num_classes]
    auto& out0 = outputs[0];
    float* data = out0.GetTensorMutableData<float>();

    auto type_info = out0.GetTensorTypeAndShapeInfo();
    auto shape     = type_info.GetShape();
    size_t num_classes = shape.back(); // [1, C]

    return softmaxTopK(data, num_classes, options_.topK);
}

std::vector<std::vector<ClassProb>> InferenceEngine::inferClassifyBatch(const std::vector<cv::Mat>& images) const {
    CV_Assert(session_!=nullptr);
    int N = static_cast<int>(images.size());
    if (N==0) return {};

    // 组装 NHWC->NCHW（这里按前面的单张逻辑批量拼）
    size_t chwSize = 3ull * options_.input_height * options_.input_width;
    std::vector<float> buffer(N * chwSize);

    for (int i=0;i<N;++i) {
        cv::Mat blob = preprocess(images[i]); // (C, H*W) 视图
        std::memcpy(buffer.data() + i*chwSize, blob.ptr<float>(0), chwSize*sizeof(float));
    }

    std::array<int64_t,4> dims = {N, 3, options_.input_height, options_.input_width};
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    auto input_tensor = Ort::Value::CreateTensor<float>(memory_info,
                                                        buffer.data(), buffer.size(),
                                                        dims.data(), dims.size());

    std::vector<const char*> in_names, out_names;
    in_names.push_back(input_names_.empty()? "input" : input_names_[0].c_str());
    out_names.push_back(output_names_.empty()? "output": output_names_[0].c_str());

    auto outputs = session_->Run(Ort::RunOptions{nullptr},
                                 in_names.data(), &input_tensor, 1,
                                 out_names.data(), 1);

    // 输出形状 [N, C]
    auto& out0 = outputs[0];
    float* data = out0.GetTensorMutableData<float>();
    auto type_info = out0.GetTensorTypeAndShapeInfo();
    auto shape = type_info.GetShape(); // [N, C]
    size_t C = static_cast<size_t>(shape[1]);

    std::vector<std::vector<ClassProb>> all;
    all.reserve(N);
    for (int i=0;i<N;++i) {
        all.push_back(softmaxTopK(data + i*C, C, options_.topK));
    }
    return all;
}

std::vector<Ort::Value> InferenceEngine::runRaw(const std::vector<Ort::Value>& inputs,
                                                const std::vector<const char*>& input_names,
                                                const std::vector<const char*>& output_names) const {
    CV_Assert(session_!=nullptr);
    return session_->Run(Ort::RunOptions{nullptr},
                         input_names.data(), inputs.data(), inputs.size(),
                         output_names.data(), output_names.size());
}

std::vector<std::string> InferenceEngine::getInputNames() const { return input_names_; }
std::vector<std::string> InferenceEngine::getOutputNames() const { return output_names_; }
