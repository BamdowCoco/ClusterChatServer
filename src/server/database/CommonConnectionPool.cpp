#include "CommonConnectionPool.hpp"
#include "Connection.hpp"
#include "logger.hpp"

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <cstdlib>
#include <thread>
#include <functional>
using namespace std;


ConnectionPool::ConnectionPool() {
    // 加载配置项
    if(!loadConfigFile()) {
        return;
    }

    // 运行标志位
    _isStop = false;

    // 创建初始连接
    for(int i=0;i<_initSize;i++) {
        Connection* connPtr = new Connection();
        connPtr->connect(_ip, _port, _username, _password, _dbname);
        // 刷新连接空闲时间
        connPtr->refreshIdleStart();
        _connectionQue.push(connPtr);
        _connectionCount++;
    }

    // 创建子线程: 连接的生产者
    thread producer(std::bind(&ConnectionPool::produceConnectionTask, this));
    // !!! 分离子线程
    producer.detach();

    // 创建子线程: 定时扫描超过_maxIdleTime的空闲连接 并回收相应资源
    thread scaner(std::bind(&ConnectionPool::scanConnectionTask, this));
    // 分离子线程
    scaner.detach();
}

// 获取连接池单例
ConnectionPool& ConnectionPool::getInstance() {
    static ConnectionPool pool;
    return pool;
}

// 从配置文件中加载配置项
bool ConnectionPool::loadConfigFile() {
    // 打开文件
    ifstream inFile("../config/database.cnf");
    if(!inFile) {
        LOG("file: ../config/database.cnf do not exist!");
        return false;
    }
    // 读取文件
    string line;
    while(getline(inFile,line)) {
        // 注意: getline 不会保留\n
        // 跳过注释 和 空行
        if(line.empty() || line[0] == '#' || line[0] == '[') {
            continue;
        }
        int equalIdx = line.find('=');
        if(equalIdx==string::npos) {
            LOG("invalid config line!");
            continue;
        }
        string key = line.substr(0, equalIdx);
        string val = line.substr(equalIdx+1);
        
        // 读取环境变量
        if(val[0] == '$') {
            // 提取环境变量名
            string envName;
            if(val.size()>2 && val[1]=='{') { // ${xxx}
                envName = val.substr(2, val.size()-3);
            } else { // $xxx
                envName = val.substr(1);
            }

            // 读取环境变量
            const char* envVal = getenv(envName.c_str());
            if(envVal != nullptr) {
                val = envVal;
            } else {
                LOG("failed to extract env value: " + envName);
            }
        }

        // LOG("key:" + key + " " + "val:" +  val);
        if(key  == "ip") {
            _ip = val;
        } else if(key == "port") {
            // try {
            //     unsigned long temp = std::stoul(val);
                
            //     // 检查范围是否在 unsigned short 内
            //     if (temp > std::numeric_limits<unsigned short>::max()) {
            //         throw std::out_of_range("值超出 unsigned short 范围");
            //     }
                
            //     _port = static_cast<unsigned short>(temp);
            // } catch (const std::invalid_argument& e) {
            //     cerr << "无效的数字格式: " << e.what() << endl;
            // } catch (const std::out_of_range& e) {
            //     cerr << "数值超出范围: " << e.what() << endl;
            // }
            _port = stoi(val);
        } else if(key == "username") {
            _username  = val;
        } else if(key == "password") {
            _password = val;
        } else if(key == "dbname") {
            _dbname = val;
        } else if(key == "init_size") {
            _initSize = stoi(val);
        } else if(key == "max_size") {
            _maxSize = stoi(val);
        } else if(key == "max_idle_time") {
            _maxIdleTime = stoi(val);
        } else if(key == "connection_timeout") {
            _connectionTimeout = stoi(val);
        } else {
            LOG("invalid key: " + key + " !");
        }
    }
    // 关闭文件
    inFile.close();
    return true;
}

// 运行在子线程 生产新连接
void ConnectionPool::produceConnectionTask() {
    // 停止条件!!!
    while (!_isStop) {
        {
            unique_lock<mutex> lock(_queueMutex);
            // LOG("produceConnectionTask() 阻塞");
            // LOG("_connectionCount:" + to_string(_connectionCount));
            // 当连接队列为空 且 未达连接数量上限时才创建新连接
            _cvProducer.wait(lock, [this] {
                return (_connectionQue.empty() &&
                        _connectionCount < _maxSize) ||
                       _isStop;
            });
            // LOG("produceConnectionTask() 解除阻塞");
            if(_isStop) break;
            Connection* connPtr = new Connection();
            connPtr->connect(_ip, _port, _username, _password, _dbname);
            connPtr->refreshIdleStart();
            _connectionQue.push(connPtr);
            _connectionCount++;
            // 互斥锁 解锁并释放
        }
        // 唤醒其中一个阻塞的消费者 可以获取空闲连接了
        _cvConsumer.notify_one();
    }
    LOG("生产者线程已退出");
}

// 运行在子线程 定时扫描超过_maxIdleTime的空闲连接 并回收相应资源
void ConnectionPool::scanConnectionTask() {
    while (!_isStop) {
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));
        if(_isStop) break;
        // LOG("scanConnectionTask() 阻塞");
        unique_lock<mutex> lock(_queueMutex);
        // LOG("scanConnectionTask() 解除阻塞");
        // 当连接数>初始连接数时 才进行回收
        // 队列中 连接按空闲时间递增顺序排列 即 队头空闲时间最长
        while(_connectionCount>_initSize) {
            Connection* connPtr = _connectionQue.front();
            if(connPtr->getIdleAliveDuration()>=_maxIdleTime) {
                _connectionQue.pop();
                _connectionCount--;
                delete connPtr; // 释放连接资源
            } else {
                break; // 当前队头空闲时间最长 没超时 即后续连接都没超时
            }
        }
        // size_t queSize = _connectionQue.size();
        // for(int i=0;i<queSize&&_connectionCount>_initSize;i++) {
        //     Connection* connPtr = _connectionQue.front();
        //     _connectionQue.pop();
        //     if(connPtr->getIdleAliveDuration()>_maxIdleTime) {
        //         // 超过最大空闲时间 释放资源
        //         delete connPtr;
        //         _connectionCount--;
        //     } else {
        //         // 没超过最大空闲时间 重新入队 但不刷新空闲时间点
        //         _connectionQue.push(connPtr);
        //     }
        // }
    }
    LOG("定时扫描线程已退出");
}

// 外部接口 获取可用空闲连接
std::shared_ptr<Connection> ConnectionPool::getConnection() {
    shared_ptr<Connection> sharedPtr;

    {
        unique_lock<mutex> lock(_queueMutex);
        // LOG("getConnection() 阻塞");
        // 等待连接队列有空闲连接 设置超时时间
        bool isSuccess = _cvConsumer.wait_for(lock, chrono::milliseconds(_connectionTimeout), [this] {
            return !_connectionQue.empty();
        });
        // LOG("getConnection() 解除阻塞");
        // 谓词条件不成立 无空闲连接
        if(!isSuccess) {
            LOG("获取空闲连接超时...");
            return nullptr;
        }
        
        // 智能指针计数为0 会默认delete
        // 这里接管Connection普通指针 并 自定义删除器重新将连接指针加入到连接队列中
        // !!! 连接队列是临界资源 注意线程安全 !!!
        sharedPtr.reset(_connectionQue.front(), [this](Connection* connPtr) {
            unique_lock<mutex> lock(_queueMutex);
            connPtr->refreshIdleStart();
            _connectionQue.push(connPtr);
        });
        _connectionQue.pop();
    }
    
    // 通知生产者生产连接
    _cvProducer.notify_all();

    return sharedPtr;

}

// 关闭连接池
// 结束生产者线程和定时回收线程
void ConnectionPool::stop() {
    _isStop = true;
    // 唤醒生产者线程停止
    _cvProducer.notify_all();
}

ConnectionPool::~ConnectionPool() {
    while (!_connectionQue.empty()) {
        Connection* connPtr = _connectionQue.front();
        _connectionQue.pop();
        if(connPtr != nullptr) {
            delete connPtr;
        }
    }
}