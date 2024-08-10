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
#include <sys/inotify.h>    // inotify ��һ��ǿ��� Linux �ں���ϵͳ�������Լ����ļ�ϵͳ�¼����紴����ɾ�����޸ĵȡ�
#include <unistd.h>



// ���캯������ʼ���豸�б�ͻ�����
PerceptionDeviceManager::PerceptionDeviceManager() {
    // ��������߳�
    detectionThread = std::thread(&PerceptionDeviceManager::detectDevices, this);
}

// ����������ȷ���߳��˳�
PerceptionDeviceManager::~PerceptionDeviceManager() {
    if (detectionThread.joinable()) {
        detectionThread.join();
    }
}

// ��ӡ���м�⵽���豸
void PerceptionDeviceManager::printDetectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex); // ����
    // std::lock_guard���󴴽��󣬳��Ի�ȡ����mutex������Ȩ��������Ȩ�����������lock_guard��������
    std::cout << "Detected video devices:" << std::endl;
    if (devices.empty()) {
        std::cout << "No video device detected." << std::endl;
        return;
    }
    for (const auto& device : devices) {    // ��ȫ����
        std::cout << device << std::endl;
    }
   // �뿪�������lock�Զ����ٲ�����
}

// �����豸�Ƿ����
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

// �豸��⺯�����ڶ����߳�������
/*
* Ҫʵ�� USB �豸���Ȳ�μ�⣬����û���ⲿ�豸���ʱ�ü������������
* ����ʹ�� Linux �� inotify ���������� /dev Ŀ¼�µ��豸�ļ��仯��
* inotify ��һ��ǿ��� Linux �ں���ϵͳ�������Լ����ļ�ϵͳ�¼����紴����ɾ�����޸ĵȡ�
* #include <sys/inotify.h>    // inotify ��һ��ǿ��� Linux �ں���ϵͳ�������Լ����ļ�ϵͳ�¼����紴����ɾ�����޸ĵȡ�
* #include <unistd.h>
*/
void PerceptionDeviceManager::detectDevices() {
    int inotifyFd = inotify_init(); // ��ʼ�� inotify
    if (inotifyFd < 0) {
        std::cerr << "Failed to initialize inotify" << std::endl;
        return;
    }

    int wd = inotify_add_watch(inotifyFd, "/dev", IN_CREATE | IN_DELETE); // ���� /dev Ŀ¼�Ĵ�����ɾ���¼�
    if (wd < 0) {
        std::cerr << "Failed to add inotify watch" << std::endl;
        close(inotifyFd);
        return;
    }

    while (true) {
        // ��⵱ǰ�����豸
        std::vector<std::string> detectedDevices;
        for (int i = 0; i < 20; i += 2) { // ���������20���豸
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
        int length = read(inotifyFd, buffer, sizeof(buffer)); // �����ȴ��¼�����
        if (length < 0) {
            std::cerr << "inotify read error" << std::endl;
            break; // ���������˳�ѭ��
        }

        // �����¼�
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

        // �������ߺ��ٴμ��
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    inotify_rm_watch(inotifyFd, wd);
    close(inotifyFd);
}



// ����豸�ļ��Ƿ����
bool PerceptionDeviceManager::isDeviceExists(const std::string& path) const {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// �����豸�Ƿ����
bool PerceptionDeviceManager::isDeviceAvailable(const std::string& devicePath) const {
    cv::VideoCapture cap(devicePath);
    return cap.isOpened();
}

// ���ؼ�⵽���豸�б�
const std::vector<std::string>& PerceptionDeviceManager::getDevices() const {
    return devices;
}

// �ص�����
void PerceptionDeviceManager::setDeviceChangeCallback(DeviceChangeCallback callback) {
    deviceChangeCallback = callback;
}