#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <opencv2/opencv.hpp>
#include "PerceptionDeviceManager.h"
#include <sys/inotify.h>    // inotify 是一个强大的 Linux 内核子系统，它可以监听文件系统事件，如创建、删除、修改等。
#include <unistd.h>



// 构造函数，初始化设备列表和互斥锁
PerceptionDeviceManager::PerceptionDeviceManager() {
    // 启动检测线程
    detectionThread = std::thread(&PerceptionDeviceManager::detectDevices, this);
}

// 析构函数，确保线程退出
PerceptionDeviceManager::~PerceptionDeviceManager() {
    if (detectionThread.joinable()) {
        detectionThread.join();
    }
}

// 打印所有检测到的设备
void PerceptionDeviceManager::printDetectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex); // 加锁
    // std::lock_guard对象创建后，尝试获取它的mutex的所有权，当控制权不在作用域后，lock_guard对象被析构
    std::cout << "Detected video devices:" << std::endl;
    if (devices.empty()) {
        std::cout << "No video device detected." << std::endl;
        return;
    }
    for (const auto& device : devices) {    // 安全便利
        std::cout << device << std::endl;
    }
   // 离开作用域后，lock自动销毁并解锁
}

// 测试设备是否可用
void PerceptionDeviceManager::PrintAvailableDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    for (const auto& device : devices) {
        if (isDeviceAvailable(device)) {
            std::cout << device << " is available." << std::endl;
        }
        else {
            std::cout << device << " is not available." << std::endl;
        }
    }
}

// 设备检测函数，在独立线程中运行
/*
* 要实现 USB 设备的热插拔检测，并在没有外部设备插拔时让检测程序进入休眠
* 可以使用 Linux 的 inotify 机制来监视 /dev 目录下的设备文件变化。
* inotify 是一个强大的 Linux 内核子系统，它可以监听文件系统事件，如创建、删除、修改等。
* #include <sys/inotify.h>    // inotify 是一个强大的 Linux 内核子系统，它可以监听文件系统事件，如创建、删除、修改等。
* #include <unistd.h>
*/
void PerceptionDeviceManager::detectDevices() {
    int inotifyFd = inotify_init(); // 初始化 inotify
    if (inotifyFd < 0) {
        std::cerr << "Failed to initialize inotify" << std::endl;
        return;
    }

    int wd = inotify_add_watch(inotifyFd, "/dev", IN_CREATE | IN_DELETE); // 监视 /dev 目录的创建和删除事件
    if (wd < 0) {
        std::cerr << "Failed to add inotify watch" << std::endl;
        close(inotifyFd);
        return;
    }

    while (true) {
        // 检测当前所有设备
        std::vector<std::string> detectedDevices;
        for (int i = 0; i < 20; i += 2) { // 假设最多有20个设备
            std::string devicePath = "/dev/video" + std::to_string(i);
            if (isDeviceExists(devicePath)) {
                detectedDevices.push_back(devicePath);
            }
        }
        this->printDetectedDevices();
        this->PrintAvailableDevices();
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            devices = std::move(detectedDevices);
        }

        std::cout << "Devices currently detected:" << std::endl;
        for (const auto& device : devices) {
            std::cout << device << std::endl;
        }

        std::cout << "Waiting for device changes..." << std::endl;

        char buffer[1024];
        int length = read(inotifyFd, buffer, sizeof(buffer)); // 阻塞等待事件发生
        if (length < 0) {
            std::cerr << "inotify read error" << std::endl;
            break; // 错误发生，退出循环
        }

        // 处理事件
        struct inotify_event* event = (struct inotify_event*)&buffer[0];
        while (length > 0) {
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    std::cout << "New device added: " << event->name << std::endl;
                }
                else if (event->mask & IN_DELETE) {
                    std::cout << "Device removed: " << event->name << std::endl;
                }
            }

            length -= sizeof(struct inotify_event) + event->len;
            event = (struct inotify_event*)((char*)event + sizeof(struct inotify_event) + event->len);
        }

        // 稍作休眠后再次检测
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    inotify_rm_watch(inotifyFd, wd);
    close(inotifyFd);
}



// 检查设备文件是否存在
bool PerceptionDeviceManager::isDeviceExists(const std::string& path) const {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// 测试设备是否可用
bool PerceptionDeviceManager::isDeviceAvailable(const std::string& devicePath) const {
    cv::VideoCapture cap(devicePath);
    return cap.isOpened();
}

// 返回检测到的设备列表
const std::vector<std::string>& PerceptionDeviceManager::getDevices() const {
    return devices;
}

// 回调函数
void PerceptionDeviceManager::setDeviceChangeCallback(DeviceChangeCallback callback) {
    deviceChangeCallback = callback;
}