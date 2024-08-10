#include "PerceptionDeviceManager.h"
#include "CameraThreadManager.h"
#include <iostream>

int main() {
    PerceptionDeviceManager PDmanager;    // �豸����̡߳� ʵ���������������д�����һ���߳�����
    std::this_thread::sleep_for(std::chrono::seconds(1)); // ���߳���ͣ1s��ʹ�豸��ʼ��
    const std::vector<std::string>& devices = PDmanager.getDevices();

    CameraThreadManager threadManager(devices); // ����̹߳�����
    threadManager.start();
    // ��ӡ�߳���Ϣ
    const auto& threadInfoList = threadManager.getThreadInfoList();
    for (const auto& info : threadInfoList) {
        std::cout << "ThreadName: " << info.threadName << "\tThread ID: " << info.threadID << " \tcontrols device: " << info.deviceID << std::endl;
    }

    // �����豸�仯�ص�
    PDmanager.setDeviceChangeCallback([&threadManager](const std::vector<std::string>& devices) {
        threadManager.onDeviceChange(devices);
        });

    while (true) {
        std::cout << "main() running 10s" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(10)); // ���߳�ÿ10���ӡһ�μ����
        /*
        * std::this_thread::sleep_for:ʹ��ǰ�߳���ִͣ��ָ����ʱ���
        * std::chrono::seconds(10)������<chrono>ͷ�ļ��е�һ��ʱ�䳤�ȱ�ʾ����ʾ10���ʱ������
        * std::chrono�����ռ��ṩ�˱�ʾʱ������ʱ�ӵ�ʱ�䵥λ��
        */
    }

    return 0;
}
