#include "common.h"
#include <ctype.h>
#include <bits/stdc++.h>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "master_slave.grpc.pb.h"
#include "peer.grpc.pb.h"
#include "peer.pb.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <grpc/support/log.h>
#include <future>

#define MAX_CHILDREN 5
#define MAX_IP_SIZE 15


enum NumOfKeys {
    SINGLE ,
    MULTIPLE
};

enum NodeType {
    NODE1,
    NODE2
};


using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::CompletionQueue;
using grpc::ClientAsyncResponseReader;

using peer::RequestBuddy;
using master_slave::TranslateKeyWords;

#define SUPERNODE_CACHE_SIZE 30720

string myaddr = "";
int num_of_child = 0;
enum NodeType node_type;
string child_addresses[MAX_CHILDREN];
thread *threads[MAX_CHILDREN] = {0};
thread *listener_threads[MAX_CHILDREN] = {0};
string file_to_be_converted = "";

string child_files[MAX_CHILDREN];
string node1_files[MAX_CHILDREN];
string node2_files[MAX_CHILDREN];
string node1_result = "";
bool node1_processed = false;
bool node2_processed = false;
string node2_result = "";
string final_result = "";

bool done_decrypting = false;
string  peer_addr = "";
bool file_loaded = false;

mutex mutex_;
mutex mutex_2;
condition_variable cond_var;

condition_variable decrypt_cond_var;

mutex cache_mutex;
LRUCache cache(SUPERNODE_CACHE_SIZE);


class SuperNode
{

public:
    SuperNode(shared_ptr<Channel> channel)
        : stub_(TranslateKeyWords::NewStub(channel)) {}


    string TryKey(const string& key)
    {
        master_slave::Request request;
        master_slave::Response response;
        ClientContext context;
        Status status;

        request.set_req(key);

        status = stub_->TryKey(&context,request,&response);


        if(status.ok())
        {
            return response.res();
        }

        else
        {
            return "TryKey RPC failed";
        }

    }

    string Translate(const string &file_chunk)
    {
        master_slave::Request request;
        master_slave::Response response;
        ClientContext context;
        Status status;

        request.set_req(file_chunk);
        
        status = stub_->Translate(&context, request, &response);

        if (status.ok())
        {
            return response.res();
        }

        else
        {
            return "Translate RPC failed";
        }

    }

    unique_ptr<TranslateKeyWords::Stub> stub_;

};


class PeerToPeerClient
{
public:
    PeerToPeerClient(shared_ptr<Channel> channel)
        : stub_(RequestBuddy::NewStub(channel)) {}

    string PeerGetKey(const string &key, bool isFile)
    {

        peer::Request request;
        peer::Response response;
        ClientContext context;
        string file;
        int len_per_child = 0, file_len;
        int now, prev = 0;

        request.set_req(key);
        request.set_file_req(isFile);

        Status status = stub_->PeerGetKey(&context, request, &response);

        
        if (status.ok())
        {
            return response.res();
        }

        else
        {
            return "RPC failed";
        }
    }

private:
    unique_ptr<RequestBuddy::Stub> stub_;
};

string request_key_from_peer(string key, bool isFile)
{
    PeerToPeerClient cli(grpc::CreateChannel(peer_addr, grpc::InsecureChannelCredentials()));
    return cli.PeerGetKey(key, isFile);
}

void distribute_the_files(string file)
{
    int file_len, len_per_child, now, prev = 0;

    file_len = (int)file.size();
    len_per_child = file_len / num_of_child;

    now = len_per_child;

    /* divide the file among the child nodes */
    for (int i = 0; i < num_of_child; i++)
    {
        // jump if now is nonalphanumeric
        while (now < file_len && isalnum(file[now]))
        {
            now++;
        }

        string tmp = file.substr(prev, now - prev);

        child_files[i] = tmp;

        int buf = now;
        now += min(len_per_child, (file_len - now));
        prev = buf;
    }
}

void concurrent_translates()
{
    SuperNode *nodes[MAX_CHILDREN] = {0};
    future<string> fut[MAX_CHILDREN];


    for (int i = 0; i < num_of_child; i++)
    {
        nodes[i] = new SuperNode(grpc::CreateChannel(child_addresses[i], grpc::InsecureChannelCredentials()));
    }

    /*============================================================================================*/

    for (int i = 0; i < num_of_child; i++)
    {
        fut[i] = async(&SuperNode::Translate, nodes[i], child_files[i]);
    }

    for(int i = 0;i < num_of_child;i++)
    {
        switch ((node_type))
        {
        case NODE1:
            /* code */
            node1_files[i]  = fut[i].get();
            break;

        case NODE2:
            node2_files[i] = fut[i].get();
            break;
        
        default:
            break;
        }
    }

    /* if node2 send to your peer   */
    if(node_type == NODE2)
    {
        /* construct the results */
        for(int i = 0;i < num_of_child;i++)
        {
            node2_result += node2_files[i];
        }
        request_key_from_peer(node2_result,true);
    }

    /* else form result and store it */
    if(node_type == NODE1)
    {
        for(int i = 0;i < num_of_child;i++)
        {
            node1_result += node1_files[i];
            node1_processed = true;

        }

        if (node2_processed)
        {
            final_result = node1_result + node2_result;
            /* here you have to notify the server of the client */
            done_decrypting = true;
            decrypt_cond_var.notify_one();
        }

    }
}




class ServerImpl final {
    public:
    ~ServerImpl(){
        server_->Shutdown();
        cq_->Shutdown();
    }

    void Run(string address){
        ServerBuilder builder;

        builder.AddListeningPort(address,grpc::InsecureServerCredentials());

        builder.RegisterService(&service_);

        cq_ = builder.AddCompletionQueue();

        server_ = builder.BuildAndStart();

        cout << "Server listening on " << address << endl;

        thread *thread_id = new thread(&ServerImpl::HandleRpcs, this);
        thread *thread_id2 = new thread(&ServerImpl::HandleRpcs, this);
        thread *thread_id3 = new thread(&ServerImpl::HandleRpcs,this);
        thread *thread_id4 = new thread(&ServerImpl::HandleRpcs,this);
        thread *thread_id5 = new thread(&ServerImpl::HandleRpcs, this);

        HandleRpcs();
    }

    private:
        class CallData
        {
        public:
            CallData(RequestBuddy::AsyncService *service, ServerCompletionQueue *cq,bool isPeer)
                : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE)
            {
                // Invoke the serving logic right away.
                peer = isPeer;
                Proceed(isPeer);
            }

        bool peer;


        bool isFileLoaded()
        {
            return file_loaded;
        }

        void Proceed(bool isPeer)
        {
            if (status_ == CREATE)
            {
                // Make this instance progress to the PROCESS state.
                status_ = PROCESS;

                if(!isPeer)
                    service_->RequestNodeGetKey(&ctx_, &request_, &responder_, cq_, cq_,
                                          this);
                else{
                    service_->RequestPeerGetKey(&ctx_, &request_, &responder_, cq_, cq_,
                                                this);
                }

            }
            else if (status_ == PROCESS)
            {
                new CallData(service_, cq_,true);
                new CallData(service_,cq_,false);

                /* The actual processing.    */

                /* PeerGetKey request handle */
                if(peer == true)
                {
                    /* if the supernode_1 is looking for address */
                    if(request_.file_req() && node_type == NODE1 && file_loaded == false)
                    {
                        /* store supernode_2 address */
                        peer_addr = request_.req();

                        unique_lock<mutex> mlock(mutex_);

                        /* you will have to wait till node_file_2 is available */
                        cond_var.wait(mlock,bind(&CallData::isFileLoaded,this));

                        /* divide the file and also give it to your peer */
                        int tot_file_size = (int)file_to_be_converted.size();
                        int prev = 0, now = 0, boundary = (tot_file_size / 2);

                        while (boundary < (int)file_to_be_converted.size() && isalnum(file_to_be_converted[boundary]))
                        {
                            boundary++;
                        }

                        int file_size_per_child = boundary / num_of_child;
                        now = file_size_per_child;

                        string node_file_1 = file_to_be_converted.substr(0,boundary);
                        string node_file_2 = file_to_be_converted.substr(boundary, tot_file_size - boundary);

                        distribute_the_files(node_file_1);
                        thread *tid = new thread(concurrent_translates);
                        reply_.set_res(node_file_2);
                        /*=============================================================================================*/

                    }

                    else if(request_.file_req() && node_type == NODE1)
                    {
                        node2_result = request_.req();
                        node2_processed = true;
                        if(node1_processed)
                        {
                            final_result = node1_result + node2_result;
                            /* here you have to notify the server of the client */
                            done_decrypting = true;
                            decrypt_cond_var.notify_one();
                        }
                        reply_.set_res("");
                        
                    }
                    /* the supernode is looking for keys */
                    else
                    {
                        /*============================================*/
                        /* ask all child nodes for value to a key     */
                        /* should be parallel requests for efficiency */

                        string request = request_.req();

                        string value_from_cache = cache.get(request);
                    
                        /* cache miss */
                        if(value_from_cache.compare("") == 0)
                        {
                            for (int i = 0; i < num_of_child; i++)
                            {
                                SuperNode node(grpc::CreateChannel(
                                    child_addresses[i], grpc::InsecureChannelCredentials()));
                                string val = node.TryKey(request);

                                if (val.compare("") != 0)
                                {
                                    cache.put(request,val);
                                    reply_.set_res(val);
                                    break;
                                }
                            }
                        }


                        else
                        {
                            reply_.set_res(value_from_cache);
                        }
                        
                        /*=============================================*/
                    }

                }

                /* NodeGetKey request handle */
                else
                {
                    string request = request_.req();

                    string value_from_cache = cache.get(request);
                    bool local_found = false;


                    if(value_from_cache.compare("") == 0)
                    {

                        for (int i = 0; i < num_of_child; i++)
                        {
                            SuperNode node(grpc::CreateChannel(
                                child_addresses[i], grpc::InsecureChannelCredentials()));
                            string val = node.TryKey(request);

                            if (val.compare("") != 0)
                            {
                                reply_.set_res(val);
                                cache.put(request, val);
                                local_found = true;
                                break;
                            }
                        }

                        /* request your peer */
                        if(!local_found)
                        {
                            /*=================================*/
                            /* ask the peerNode for key value */
                            reply_.set_res(request_key_from_peer(request,false));
                            /*===================================*/
                        }
                    }

                    else
                    {
                        reply_.set_res(value_from_cache);
                    }
                    
                }
                

                status_ = FINISH;
                responder_.Finish(reply_, Status::OK, this);
            }
            else
            {
                GPR_ASSERT(status_ == FINISH);
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
            }
        }

    private:
        
        RequestBuddy::AsyncService *service_;
        // The producer-consumer queue where for asynchronous server notifications.
        ServerCompletionQueue *cq_;

        ServerContext ctx_;

        // What we get from the client.
        peer::Request request_;
        // What we send back to the client.
        peer::Response reply_;
       
        // The means to get back to the client.
        ServerAsyncResponseWriter<peer::Response> responder_; 

        // Let's implement a tiny state machine with the following states.
        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH
        };
        CallStatus status_;

};

    // This can be run in multiple threads if needed.
    void HandleRpcs()
    {
        // Spawn a new CallData instance to serve new clients.
        new CallData(&service_, cq_.get(),true);
        new CallData(&service_,cq_.get(),false);
        void *tag; // uniquely identifies a request.
        bool ok;
        while (true)
        {

            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData *>(tag)->Proceed(static_cast<CallData *>(tag)->peer);
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    RequestBuddy::AsyncService service_;
    std::unique_ptr<Server> server_;
};


void translate_server(int port)
{
    int listenfd, connfd;

    struct sockaddr_in cliaddr = {0};
    socklen_t clilen = sizeof(cliaddr);
    size_t len, cmd, recv_amt;

    char buf[BUFSIZE];

    struct Hdr *hdr;

    bool server_job_done = false;

    if ((listenfd = open_listenfd(port)) < 0)
    {
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr,
                             &clilen)) < 0)
        {
            perror("accept");
            /* should change it */
            exit(EXIT_FAILURE);
        }

        /*=======================================================================================*/
        /* supernode waits for client to send file                                               */
        /* after it gets a file it will distribute files to childs and half of it to supernode_2 */
        for (;;)
        {
            if (!server_job_done)
            {
                recv_amt = recv_from_host(connfd, buf, HDR_LEN);
                buf[recv_amt] = '\0';

                hdr = (struct Hdr *)buf;
                if (hdr->msg_type == htonl(ERROR))
                {
                    fprintf(stderr, "Client has encountered an Error\n");
                    exit(EXIT_FAILURE);
                }

                cmd = ntohl(hdr->msg_type);

                switch (cmd)
                {

                case DELIVERY:
                {
                    /* deduct the header size */
                    len = ntohl(hdr->tot_len) - HDR_LEN;

                    recv_amt = recv_from_host(connfd, buf, len);
                    buf[recv_amt] = '\0';

                    string *data = new string(buf);
                    file_to_be_converted += *data;

                    break;
                }

                case DONE:
                {
                    server_job_done = true;
                }

                default:
                    break;
                }
            }

            if(server_job_done)
            {
                file_loaded = true;
                cond_var.notify_one();

                unique_lock<mutex> lock_(mutex_2);

                /* you will have to wait till node_file_2 is available */
                decrypt_cond_var.wait(lock_, [] { return done_decrypting == true; });
               

                
                size_t sent = 0,tmp,lft;
                size_t file_size = (int)final_result.size();
                lft = file_size;

                struct Hdr * header = create_pckt(0,DELIVERY);
                
                header->tot_len = htonl(file_size + HDR_LEN);

                // send the file
                memcpy(buf,(void *)header,HDR_LEN);
                memcpy(buf+HDR_LEN,final_result.c_str(),file_size);
                buf[file_size+HDR_LEN] = '\0';



                send_to_host(connfd,buf,file_size + HDR_LEN);

                // send done packet
                header->msg_type = htonl(DONE);
                header->tot_len = htonl(HDR_LEN);
                send_to_host(connfd,(const char *)header,HDR_LEN);
                close(connfd);
                close(listenfd);

                break;
            }
            
        }

        if(server_job_done)
        {
            break;
        }

    }

    close(listenfd);
}


int main(int argc, char *argv[])
{
    string file_to_be_converted = "";
    int tot_file_size;

    char interface[6];

    /* I set the interface to "kaist"*/
    memcpy(interface, "kaist", 6);
    unsigned char *ip_address = (unsigned char *)malloc(MAX_IP_SIZE * sizeof(unsigned char));
    get_ip(ip_address, interface);

    string myip((char *)ip_address);

    if (argc < 4)
    {
        printf("usage<>: ./super 12345 [gRPC port] [child1’s ip_address]:[child1’s port] [child2’s ip_address]:[child2’s port]"
                "[child3’s ip_address]:[child3’s port] ... ");
        exit(EXIT_FAILURE);
    }

    char *node_port = argv[1];
    char *node_grpc_port = argv[2];
    string grpc_port(node_grpc_port);
    myaddr = "" + myip + ":" + grpc_port;


    /* supernode_2 */
    if (argv[3][0] == '-' && argv[3][1] == 's')
    {
        if (argc < 6)
        {
            printf("usage<>: ./super 12346 [gRPC port] -s [another super node’s ip_address]:[another super node’s port]"
                    "[child4’s ip_address]:[child4’s port] [child5’s ip_address]:[child5’s port] [child6’s ip_address]:[child6’s port] ...");
            exit(EXIT_FAILURE);
        }

        peer_addr = argv[4];

        for (int i = 5; i < argc; i++)
        {
            child_addresses[i - 5] = argv[i];
            num_of_child++;
        }

        string  file = request_key_from_peer(myaddr,true);

        /* distribute the files among the children */
        /* then call translate method */
        distribute_the_files(file);
        thread * tid = new thread(concurrent_translates);
        
        node_type = NODE2;
        ServerImpl server;
        server.Run(myaddr);
    }

    /* supernode_1 waiting for client */
    else
    {
        /* parsing the arguments */
        for (int i = 3; i < argc; i++)
        {
            child_addresses[i - 3] = argv[i];
            num_of_child++;
        }

        /* serve the client request */
        thread *server_thread = new thread(translate_server, atoi(node_port));

        ServerImpl server;
        node_type = NODE1;
        server.Run(myaddr);
    }
    

    return 0;
}

