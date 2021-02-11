#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "auth.pb.h"
#include "auth.grpc.pb.h"

using namespace auth;

class AuthClient{
    private:
    std::unique_ptr<AuthServer::Stub> stub_;
    public:
    AuthClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(AuthServer::NewStub(channel)) {}

    std::string getCredentials(const std::string& msg,Creds& creds){
        request rqst;
        rqst.set_req(msg);
        Creds resp;
        grpc::ClientContext context;
        grpc::Status status = stub_->getCredentials(&context,rqst,&resp);
        
        if(status.ok()){
            creds = resp;
            return "ok";
        }
        return "RPC_FAILED";
    }
};


int main(){
    std::string grpcSvr ="unix:/tmp/server.sock";
    AuthClient client(grpc::CreateChannel(grpcSvr, grpc::InsecureChannelCredentials()));
    Creds credResp;
    std::string reply = client.getCredentials("hello",credResp);
    std::cout<<credResp.domain()<<"\n";
    std::cout<<credResp.username()<<"\n";
    std::cout<<credResp.password()<<"\n";
    std::cout<<credResp.type()<<"\n";
    return 0;
}
