#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "auth.pb.h"
#include "auth.grpc.pb.h"

#include <utility>
#include <thread>
#include <future>
#include <condition_variable>
#include <mutex>
using namespace auth;


class myservice;

class AuthServerImpl: public AuthServer::Service{
    private:
    std::string name;
    public:
    myservice* pt;
    AuthServerImpl();
    grpc::Status getCredentials(grpc::ServerContext* context, const request* req, Creds* cred);
    void NotifyMsgArrival(const request* req);
};

class myservice{
    public:
    std::string addr;
    request* reqs;
    std::shared_ptr<AuthServerImpl> service;
    std::shared_ptr<std::thread> t;
    std::shared_ptr<grpc::Server> server;
    std::shared_ptr<std::mutex> mtx;
    std::shared_ptr<std::condition_variable> cv;
    bool stopRequested;
    myservice();
    //void RunServer();
    void SetRequest(const request* msgReq);
    std::string GetRequest();
    void ConfigService();

    void StartServer();
    void RunServer();
    void StopServer();
    void ReadMessage();

};
AuthServerImpl::AuthServerImpl(){}
void AuthServerImpl::NotifyMsgArrival(const request* req){
    if(pt){
        pt->SetRequest(req);
    }
}
grpc::Status AuthServerImpl::getCredentials(grpc::ServerContext* context, const request* req, Creds* cred) {
    cred->set_username("administrator");
    cred->set_password("P@$$w0rd");
    cred->set_domain("local");
    cred->set_type(cred->DOMAIN);
    NotifyMsgArrival(req);
    
    return grpc::Status::OK;
}


void myservice::ConfigService(){
    service->pt = this;
}
myservice::myservice(){
    reqs = nullptr;
    service = std::make_shared<AuthServerImpl>();
    addr = "unix:/tmp/server.sock";
    mtx = std::make_shared<std::mutex>();
    cv = std::make_shared<std::condition_variable>();
    stopRequested =false;
}
void myservice::SetRequest(const request* msgReq){
    std::unique_lock<std::mutex> ulock(*mtx);
    reqs = new request();
    //msgRequest has no pointer member, thus below assignment is not susceptible to shallow copy
    *reqs = *msgReq;
    cv->notify_one();
}
std::string myservice::GetRequest(){
    
    std::unique_lock<std::mutex> ulock(*mtx);
    cv->wait(ulock, [this] { return (this->reqs ||this->stopRequested) ? true : false; });
    std::string message("");
    if(reqs){
        message = reqs->req();
        delete(reqs);
        reqs = nullptr;
    }
    
    return message;
}


void myservice::RunServer(){
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    server = std::move(std::unique_ptr<grpc::Server>(builder.BuildAndStart()));
    //std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}
void myservice::StartServer(){
    //t = std::thread(&myservice::RunServer, this);
    t = std::make_shared<std::thread>(&myservice::RunServer, this);
}
void myservice::StopServer(){
    if(server){
        server->Shutdown();
    }
    if(t->joinable()){
        t->join();
    }
    stopRequested = true;
    cv->notify_one();
}

void myservice::ReadMessage(){
    while(!stopRequested){
        std::cout<<GetRequest()<<std::endl;
    } 
}


int main(){
    myservice serv;
    serv.ConfigService();
    serv.StartServer();
    std::thread readT(&myservice::ReadMessage, &serv);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    serv.StopServer();
    readT.join();
    
    return 0;
}
