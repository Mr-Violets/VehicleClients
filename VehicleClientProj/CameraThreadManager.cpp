#include "CameraThreadManager.h"

CameraThreadManager::CameraThreadManager(const std::vector<std::string>& devicePaths)
    : devicePaths(devicePaths), frameQueues(devicePaths.size()) {
}

CameraThreadManager::~CameraThreadManager() {
    stop();
}

void CameraThreadManager::start() {
    keepRunning = true;

    // Ϊÿ���豸��������֡���߳�
    for (size_t i = 0; i < devicePaths.size(); ++i) {
        captureThreads.emplace_back(&CameraThreadManager::captureFrames, this, devicePaths[i], std::ref(frameQueues[i]));

        // ���沶���̵߳���Ϣ
        threadInfoList.push_back({ captureThreads.back().get_id(), devicePaths[i], "captureThread"});
    }

    // Ϊÿ���豸��������֡���߳�
    for (size_t i = 0; i < devicePaths.size(); ++i) {
        std::string cameraName = "camera" + std::to_string(i);
        saveThreads.emplace_back(&CameraThreadManager::saveFramesWorker, this, std::ref(frameQueues[i]), cameraName);

        // ���汣���̵߳���Ϣ
        threadInfoList.push_back({ saveThreads.back().get_id(), devicePaths[i],"saveThread"});
    }
}

void CameraThreadManager::stop() {
    keepRunning = false;
    dataCondition.notify_all();

    for (auto& t : captureThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    for (auto& t : saveThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    captureThreads.clear();
    saveThreads.clear();
}

const std::vector<CameraThreadManager::ThreadInfo>& CameraThreadManager::getThreadInfoList() const {
    return threadInfoList;
}

void CameraThreadManager::captureFrames(const std::string& devicePath, std::queue<std::pair<cv::Mat, int>>& frameQueue) {
 cvInit:
    cv::VideoCapture cap(devicePath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera at " << devicePath << std::endl;

        // �ڼ�⵽�޷�������ͷʱ���Ƴ��̲߳��˳��̺߳���
        removeDeviceAndThreads(devicePath);
        return;
    }

    int frameNumber = 0;
    int ErrCapCount = 0;
    while (keepRunning) {
        cv::Mat frame;
        cap >> frame;

        if (frame.empty()) {
            std::cerr << "Error: Captured empty frame from " << devicePath << std::endl;
            if (5 != ErrCapCount)
            {
                ErrCapCount++;
                continue;
            }
            else {
                ErrCapCount = 0;
                goto cvInit;
            }
           
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            frameQueue.push(std::make_pair(frame, frameNumber++));
            dataCondition.notify_one();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void CameraThreadManager::removeDeviceAndThreads(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(queueMutex);

    // �����豸·����Ӧ������
    auto it = std::find(this->devicePaths.begin(), this->devicePaths.end(), devicePath);
    if (it != devicePaths.end()) {
        size_t index = std::distance(devicePaths.begin(), it);

        // ֹͣ����������߳�
        // ��ǰ���쳣��������
        // �߳����Ƴ�֮ǰ�Ѿ������ˣ������ֳ��� join�����ܻ������쳣
        if (captureThreads[index].joinable()) { 
            stopThread(captureThreads[index]);
        }
        if (saveThreads[index].joinable()) {
            stopThread(saveThreads[index]);
        }

        // �Ƴ��豸���߳���Ϣ
        captureThreads.erase(captureThreads.begin() + index);
        saveThreads.erase(saveThreads.begin() + index);
        frameQueues.erase(frameQueues.begin() + index);
        threadInfoList.erase(threadInfoList.begin() + 2 * index, threadInfoList.begin() + 2 * index + 2);
        devicePaths.erase(it);

        std::cout << "Removed device and threads for: " << devicePath << std::endl;
    }
}


void CameraThreadManager::saveFramesWorker(std::queue<std::pair<cv::Mat, int>>& frameQueue, const std::string& cameraName) {
    while (keepRunning || !frameQueue.empty()) {
        std::string currentDateTime;    // ��ǰʱ��
        std::unique_lock<std::mutex> lock(queueMutex);
        dataCondition.wait(lock, [&frameQueue, this] { return !frameQueue.empty() || !keepRunning; });

        if (!frameQueue.empty()) {
            //std::cout << "Processing frames from " << cameraName << std::endl;
            currentDateTime = getCurrentDateTimeString();
            saveFrames(frameQueue, currentDateTime, cameraName);
            //frameQueue.pop(); // ʾ���н��Ƴ�֡�������ڴ˴���򱣴�֡����
        }
    }
}

std::string CameraThreadManager::getCurrentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&now_time_t, &tm);

    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d/%H-%M-%S");

    return ss.str();
}

void CameraThreadManager::saveFrames(std::queue<std::pair<cv::Mat, int>>& frameQueue, const std::string& currentDateTime, const std::string& cameraName) {
    std::string baseDir = "."; // �������ǰĿ¼Ϊ����Ŀ¼
    std::string carNumber = "0001";

    static std::string preDataTime = "";
    static int frameNum = 1; // ���
    if (preDataTime != currentDateTime) {
        preDataTime = currentDateTime;
        frameNum = 1;
    }
    else {
        frameNum++;
    }

    std::string folderPath = baseDir + "/dataCapture/Car" + carNumber + "/" + currentDateTime + "/" + cameraName;
    fs::create_directories(folderPath);

    while (!frameQueue.empty()) {
        auto [frame, frameNumber] = frameQueue.front();
        frameQueue.pop();

        std::string filename = folderPath + "/frame" + std::to_string(frameNum) + ".jpg";
        cv::imwrite(filename, frame);
        std::cout << "Saved " << filename << std::endl;
    }
}

void CameraThreadManager::onDeviceChange(const std::vector<std::string>& newDevicePaths) {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::cout << "Device change detected, updating threads..." << std::endl;

    // ���������豸
    for (const auto& device : newDevicePaths) {
        if (std::find(devicePaths.begin(), devicePaths.end(), device) == devicePaths.end()) {
            std::queue<std::pair<cv::Mat, int>> newQueue;
            frameQueues.push_back(std::move(newQueue));
            devicePaths.push_back(device);

            size_t index = devicePaths.size() - 1;
            captureThreads.emplace_back(&CameraThreadManager::captureFrames, this, devicePaths[index], std::ref(frameQueues[index]));
            saveThreads.emplace_back(&CameraThreadManager::saveFramesWorker, this, std::ref(frameQueues[index]), "camera" + std::to_string(index));

            // �����߳���Ϣ
            threadInfoList.push_back({ captureThreads.back().get_id(), devicePaths[index], "captureThread" });
            threadInfoList.push_back({ saveThreads.back().get_id(), devicePaths[index], "saveThread" });

            std::cout << "Added new device and threads for: " << device << std::endl;
        }
    }

    // �����Ƴ��豸
    for (auto it = devicePaths.begin(); it != devicePaths.end(); ) {
        if (std::find(newDevicePaths.begin(), newDevicePaths.end(), *it) == newDevicePaths.end()) {
            // ���豸�Ѿ��Ƴ���ֹͣ����߳�
            size_t index = std::distance(devicePaths.begin(), it);

            stopThread(captureThreads[index]);
            stopThread(saveThreads[index]);

            captureThreads.erase(captureThreads.begin() + index);
            saveThreads.erase(saveThreads.begin() + index);
            frameQueues.erase(frameQueues.begin() + index);
            threadInfoList.erase(threadInfoList.begin() + 2 * index, threadInfoList.begin() + 2 * index + 2);
            it = devicePaths.erase(it);

            std::cout << "Removed device and threads for: " << *it << std::endl;
        }
        else {
            ++it;
        }
    }
}
void CameraThreadManager::stopThread(std::thread& t) {
    try {
        if (t.joinable()) {
            t.join();
        }
    }
    catch (const std::system_error& e) {
        std::cerr << "Error joining thread: " << e.what() << std::endl;
        // �����̴߳��󣬱�������
    }
}

