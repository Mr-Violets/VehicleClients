#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <opencv2/opencv.hpp>
#include <condition_variable>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

class CameraThreadManager {
public:
    CameraThreadManager(const std::vector<std::string>& devicePaths);
    ~CameraThreadManager();

    void start();
    void stop();

    struct ThreadInfo {     // �洢�߳�id���豸id��һһ��Ӧ
        std::thread::id threadID;
        std::string deviceID;
        std::string threadName;
    };

    void onDeviceChange(const std::vector<std::string>& newDevicePaths);
    const std::vector<ThreadInfo>& getThreadInfoList() const;

private:
    void captureFrames(const std::string& devicePath, std::queue<std::pair<cv::Mat, int>>& frameQueue);      // ֡����
    void saveFramesWorker(std::queue<std::pair<cv::Mat, int>>& frameQueue, const std::string& cameraName);  // ֡����
    std::string getCurrentDateTimeString(void );
    void saveFrames(std::queue<std::pair<cv::Mat, int>>& frameQueue, const std::string& currentDateTime, const std::string& cameraName);
    void stopThread(std::thread& t);   // ֹͣ�̺߳���
    void removeDeviceAndThreads(const std::string& devicePath); // ֹͣ������һ������ͷ���߳�

    std::vector<std::string> devicePaths;   // ��������ͷ�豸·��
    std::vector<std::queue<std::pair<cv::Mat, int>>> frameQueues;   // �洢ÿ������ͷ���񵽵�֡����
    // ÿ�� std::queue ��Ӧһ������ͷ�豸
    // �����е�Ԫ����һ�� std::pair<cv::Mat, int>
    // �������񵽵�֡ (cv::Mat) �͸�֡�ı�� (int)
    std::vector<std::thread> captureThreads;    // ������Ƶ֡���̶߳���
    std::vector<std::thread> saveThreads;   // ����ͱ�����Ƶ֡���̶߳���
    std::mutex queueMutex;// ������
    std::condition_variable dataCondition;  // ��������
    bool keepRunning = true;    // �����̵߳�����״̬

    std::vector<ThreadInfo> threadInfoList; // ���ڱ����߳� ID ���豸 ID ���б�


};
