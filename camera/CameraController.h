#pragma once

#include "core/AppTypes.h"
#include <QObject>

// CameraController — Basler Pylon 相机封装：开闭、连续采集、参数读写、最新帧副本
// 未定义 QT_PROJECT_USE_PYLON 时接口仍可用，但 open/startGrab 等会失败并 emit errorOccurred
class CameraController : public QObject
{
    Q_OBJECT

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    bool initPylon();              // 进程级 PylonInitialize
    void shutdownPylon();          // 停采关设备后 PylonTerminate
    bool isPylonInitialized() const { return m_pylonInitialized; }
    bool open();                   // 打开第一台可用 Basler 设备
    void close();                  // 关设备并清空 latestImage
    bool isOpen() const;
    bool startGrab();              // 启动 grabLoopWorker 连续取帧
    void stopGrab();
    bool setExposureUs(double us);
    bool setGainDb(double db);
    bool setFps(double fps);
    CamParamLimits paramLimits() const; // 当前缓存的节点 min/max
    bool copyLatestImage(QImage &out);  // 主线程取最近一帧深拷贝

signals:
    void errorOccurred(const QString &message);

private:
    void grabLoopWorker();         // 独立线程：RetrieveResult → RGB888 → latestImage
    void readParamLimits();        // open 后从 GenICam 刷新 m_limits
    bool applyResolution();        // 设置 Width/Height 为 kImageWidth×kImageHeight
    bool setFloatNode(const char *nodeName, double value);

    struct Impl;                   // Pimpl：隐藏 Pylon 类型，见 CameraController.cpp
    Impl *m_impl = nullptr;
    CamParamLimits m_limits;
    bool m_open = false;
    bool m_grabbing = false;
    bool m_pylonInitialized = false;
};
