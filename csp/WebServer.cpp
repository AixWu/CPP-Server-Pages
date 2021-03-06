#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory.h>
#include "WebServer.h"
#include"Session.h"
#include"Epoll.h"
#include"DebugPrint.h"
#include"Listener.h"
#include<string>
#include<iostream>
#include"ReadEpoll.h"
#include "WriteEpoll.h"

Web::Web()noexcept :_doing(true),_count(0)
{
	dout << "---class Web constructor";
}

Web::~Web() noexcept
{
    dout << "---class Web deconstructor";
}

void Web::startRead(){
    std::cout<<"init read"<<std::endl;
    ReadEpoll rpoll(_task_queue,1);

    notifyInit();

    while(_doing) {
        rpoll.wait();
    }
    notifyExit();
    std::cout<<"read"<<std::endl;
}

void Web::startWrite(){
    std::cout<<"init write"<<std::endl;
    WriteEpoll wpoll(_task_queue);

    notifyInit();
    while(_doing){
        wpoll.wait();
    }
    notifyExit();
    std::cout<<"W"<<std::endl;
}

void Web::startHandle(){
    std::cout<<"init handle"<<std::endl;
    Session session(_task_queue);

    notifyInit();
    while(_doing){
        session.handle();
    }
    notifyExit();
    std::cout<<"H"<<std::endl;
}

void Web::startAccept() noexcept {
    Listener listener;
    InetAddress addr("127.0.0.1", 8888, AF_INET);

    if (!listener.bind(addr) || !listener.listen()) {
        notifyInit();
        return;
    }

    InetAddress accept_addr;

    Epoll epoll("Listen");
    epoll.eventAdd(listener.fd(), Epoll::READ);

    notifyInit();

    dout << "进入 web循环";
    while (_doing) {
        if (!epoll.waitAccept(1000)){
            dout << "accept return";
            continue;
        }

        InetAddress accept_addr;
        int fd;
        while ((fd = listener.accept(accept_addr)) > 0) {
            std::cout<<"fd "<<fd<<std::endl;
            _task_queue.addReadTask(fd);
        }
        dout << "accept "<< fd;
        _task_queue.notifyRead();

        int savedErrno = errno;

        switch (savedErrno) {
            case EAGAIN:
            case ECONNABORTED: //POSIX
            case EINTR:
            case EPROTO: // SVR4
            case EPERM:
            //case EWOULDBLOCK:
            case EMFILE: // per-process lmit of open file desctiptor ???
                // expected errors
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                // unexpected errors
                dout << "unexpected error of ::accept " << savedErrno;
                break;
            default:
                dout << "unknown error of ::accept " << savedErrno;
                break;
        }
    }
    notifyExit();
    dout << "退出 accept循环";
}

void Web::prepare(){

}

void Web::start()noexcept {

    prepare();

    _th_read = std::thread(&Web::startRead, this);
    _th_handle = std::thread(&Web::startHandle, this);
    _th_write = std::thread(&Web::startWrite, this);
    _th_accept = std::thread(&Web::startAccept, this);

    {
        std::unique_lock<std::mutex> ul(_m);
        _cnd.wait(ul, [this] { return _count == 4; });
    }

    std::cout << "init finsh" << std::endl;
    std::string str;
    std::cin >> str;

    std::cout << str << std::endl;
    stop();
    std::cout << str << std::endl;

}

void Web::stop()noexcept {
    _doing = false;
    _task_queue.notifyExit();

    _th_accept.join();
    _th_read.join();
    _th_handle.join();
    _th_write.join();
}
