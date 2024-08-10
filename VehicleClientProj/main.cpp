#include "PerceptionDeviceManager.h"
#include "CameraThreadManager.h"
#include <iostream>

int main() {
    PerceptionDeviceManager PDmanager;    // 设备检测线程。 实例化后，析构函数中创建了一个线程运行
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 主线程暂停1s，使设备初始话
    const std::vector<std::string>& devices = PDmanager.getDevices();

    CameraThreadManager threadManager(devices); // 相机线程管理类
    threadManager.start();
    // 打印线程信息
    const auto& threadInfoList = threadManager.getThreadInfoList();
    for (const auto& info : threadInfoList) {
        std::cout << "ThreadName: " << info.threadName << "\tThread ID: " << info.threadID << " \tcontrols device: " << info.deviceID << std::endl;
    }

    // 设置设备变化回调
    PDmanager.setDeviceChangeCallback([&threadManager](const std::vector<std::string>& devices) {
        threadManager.onDeviceChange(devices);
        });

    while (true) {
        std::cout << "main() running 10s" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(10)); // 主线程每10秒打印一次检测结果
        /*
        * std::this_thread::sleep_for:使当前线程暂停执行指定的时间段
        * std::chrono::seconds(10)：这是<chrono>头文件中的一个时间长度表示，表示10秒的时间间隔。
        * std::chrono命名空间提供了表示时间间隔和时钟的时间单位。
        */
    }

    return 0;
}
