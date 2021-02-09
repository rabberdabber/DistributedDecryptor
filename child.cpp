
#include "common.h"
#include <stdio.h>
#include <grpcpp/grpcpp.h>
#include "master_slave.grpc.pb.h"
#include "assign4.grpc.pb.h"
#include "peer.grpc.pb.h"
#include <thread>
#include <mutex>


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ServerCompletionQueue;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::CompletionQueue;

using peer::RequestBuddy;
using master_slave::TranslateKeyWords;
using assign4::Database;


#define MAX_IP_SIZE 15
#define CHILD_CACHE_SIZE 10240

char *supernode_addr;
char *database_addr;
string myaddr = "";
mutex cache_mutex;
LRUCache cache(CHILD_CACHE_SIZE);


enum State {
    ALPHABET,
    NON_ALPHABET
};


enum DBtype {
    DB,
    NODE
};

enum NumOfKeys {
    SINGLE,
    MULTIPLE
};



class DBChildClient
{
public:
    DBChildClient(shared_ptr<Channel> channel)
        : stub_(Database::NewStub(channel)) {}

    string AccessDB(const string &user)
    {
        assign4::Request request;
        assign4::Response response;
        ClientContext context;

        request.set_req(user);

        Status status = stub_->AccessDB(&context, request, &response);

        if (status.ok())
        {
            return response.res();
        }
        else
        {
            cout << status.error_code() << ": " << status.error_message()
                 << endl;
            return "AccessDB RPC failed";
        }
    }

private:
    unique_ptr<Database::Stub> stub_;
};

class NodeChildClient
{
public:
    NodeChildClient(shared_ptr<Channel> channel)
        : stub_(RequestBuddy::NewStub(channel)) {}

    string NodeGetKey(const string &user)
    {
        peer::Request request;
        peer::Response response;
        ClientContext context;

        request.set_req(user);

        Status status = stub_->NodeGetKey(&context, request, &response);

        if (status.ok())
        {
            return response.res();
        }
        else
        {
            cout << status.error_code() << ": " << status.error_message()
                 << endl;
            return "NodeGetKey RPC failed";
        }
    }

private:
    unique_ptr<RequestBuddy::Stub> stub_;
};

string getDB(string addr,string key,enum DBtype type)
{
    string result = "";

    switch(type)
    {
        case DB:
        {
            DBChildClient cli(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
            result = cli.AccessDB(key);

            break;
        }
            
        case NODE:
        {
            NodeChildClient cli_2(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
            result = cli_2.NodeGetKey(key);
            break;
        }
    }

    return result;

}


string translate(string request)
{
   
    string reply = "";

    /*=====================*/
    /* translate request   */

    vector<string> wordtokens;
    vector<string> trash;
    vector<string> translated_tokens;

    enum State curr_state;
    int prev_word = 0, prev_trash = 0, request_size = (int)request.size();

    if (isalnum(request[0]))
    {
        curr_state = ALPHABET;
    }
    else
    {
        curr_state = NON_ALPHABET;
    }

    for (int i = 0; i < request_size; i++)
    {
        switch (curr_state)
        {
        case ALPHABET:
            if (!isalnum(request[i]))
            {
                curr_state = NON_ALPHABET;
                wordtokens.push_back(request.substr(prev_word, i - prev_word));
                prev_trash = i;
            }

            break;

        case NON_ALPHABET:

            if (isalnum(request[i]))
            {
                curr_state = ALPHABET;
                trash.push_back(request.substr(prev_trash, i - prev_trash));
                prev_word = i;
            }

            break;
        }

        if (i == request_size - 1)
        {
            if (curr_state == ALPHABET)
                wordtokens.push_back(request.substr(prev_word, request_size - prev_word));

            else
                trash.push_back(request.substr(prev_trash, request_size - prev_trash));
        }
    }

    int wordtokens_size = (int)wordtokens.size();
    int trash_size = (int)trash.size();
    string final_str = "";

    for (int i = 0; i < wordtokens_size; i++)
    {
        string DB_addr(database_addr);

        cache_mutex.lock();
        string value = cache.get(wordtokens[i]);
        cache_mutex.unlock();

        if(value.compare("") == 0)
        {
            value = getDB(DB_addr, wordtokens[i], DB);
        }
       
        // if in the data base
        if (value.c_str()[0] != '\0')
        {
            cache_mutex.lock();
            cache.put(wordtokens[i], value);
            cache_mutex.unlock();
        }

        // ask the supernode for the value from the key
        else
        {
            string node_addr(supernode_addr);
            value = getDB(node_addr, wordtokens[i], NODE);
        }

        translated_tokens.push_back(value);
    }

    /* reconstruction of the request */
    for (int i = 0; i < max(wordtokens_size, trash_size); i++)
    {

        if (isalnum(request[0]))
        {
            if (i <= wordtokens_size - 1)
            {
                final_str += translated_tokens[i];
            }

            if (i <= trash_size - 1)
            {
                final_str += trash[i];
            }
        }
        else
        {
            if (i <= trash_size - 1)
            {
                final_str += trash[i];
            }
            if (i <= wordtokens_size - 1)
            {
                final_str += translated_tokens[i];
            }
        }
    }

    /*======================*/
    return final_str;
}

string trykey(string request)
{

    string db_addr(database_addr);
   
    cache_mutex.lock();
    string value_from_cache = cache.get(request);
    cache_mutex.unlock();


    if (value_from_cache.compare("") == 0)
    {
        string reply = getDB(db_addr, request, DB);

        if(reply.c_str()[0] == '\0')
        {
            return "";
        }
        else
        {
            return reply;
        }
        
    }
    else
    {
       return value_from_cache;
    }
}



class ServerImpl final
{
public:
    ~ServerImpl()
    {
        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();
    }

    // There is no shutdown handling in this code.
    void Run()
    {
        std::string server_address(myaddr);

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&service_);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;


        thread * thread_id = new thread(&ServerImpl::HandleRpcs,this);
        thread *thread_id_2 = new thread(&ServerImpl::HandleRpcs, this);

        // Proceed to the server's main loop.
        HandleRpcs();
    }

private:
    // Class encompasing the state and logic needed to serve a request.
    class CallData
    {
    public:
        // Take in the "service" instance (in this case representing an asynchronous
        // server) and the completion queue "cq" used for asynchronous communication
        // with the gRPC runtime.
        CallData(TranslateKeyWords::AsyncService *service, ServerCompletionQueue *cq,enum NumOfKeys no_of_keys)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE)
        {
            // Invoke the serving logic right away.
            num_keys = no_of_keys;
            Proceed();
        }

        void Proceed()
        {
            if (status_ == CREATE)
            {
                // Make this instance progress to the PROCESS state.
                status_ = PROCESS;

                // As part of the initial CREATE state, we *request* that the system
                // start processing SayHello requests. In this request, "this" acts are
                // the tag uniquely identifying the request (so that different CallData
                // instances can serve different requests concurrently), in this case
                // the memory address of this CallData instance.

                if(num_keys == SINGLE)
                    service_->RequestTryKey(&ctx_, &request_, &responder_, cq_, cq_,
                                          this);

                else
                    service_->RequestTranslate(&ctx_, &request_, &responder_, cq_, cq_,
                                            this);
            }
            else if (status_ == PROCESS)
            {
                // Spawn a new CallData instance to serve new clients while we process
                // the one for this CallData. The instance will deallocate itself as
                // part of its FINISH state.
                new CallData(service_, cq_,SINGLE);
                new CallData(service_,cq_,MULTIPLE);


                // The actual processing.
                if(num_keys == SINGLE)
                {
                    reply_.set_res(trykey(request_.req()));
                }
                else
                {
                    reply_.set_res(translate(request_.req()));
                }
                

                // And we are done! Let the gRPC runtime know we've finished, using the
                // memory address of this instance as the uniquely identifying tag for
                // the event.
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
        // The means of communication with the gRPC runtime for an asynchronous
        // server.
        TranslateKeyWords::AsyncService *service_;
        // The producer-consumer queue where for asynchronous server notifications.
        ServerCompletionQueue *cq_;
        // Context for the rpc, allowing to tweak aspects of it such as the use
        // of compression, authentication, as well as to send metadata back to the
        // client.
        ServerContext ctx_;

        // What we get from the client.
        master_slave::Request request_;
        // What we send back to the client.
        master_slave::Response reply_;

        // The means to get back to the client.
        ServerAsyncResponseWriter<master_slave::Response> responder_;

        // Let's implement a tiny state machine with the following states.
        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH
        };
        CallStatus status_; // The current serving state.
        enum NumOfKeys num_keys;
    };

    // This can be run in multiple threads if needed.
    void HandleRpcs()
    {
        // Spawn a new CallData instance to serve new clients.
        new CallData(&service_, cq_.get(),SINGLE);
        new CallData(&service_,cq_.get(),MULTIPLE);

        void *tag; // uniquely identifies a request.
        bool ok;
        while (true)
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData *>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    TranslateKeyWords::AsyncService service_;
    std::unique_ptr<Server> server_;
};


int main(int argc, char ** argv){

    if(argc != 4)
    {
        fprintf(stderr, "usage<>: ./child 50051 [super node’s ip_address]:[super node’s gRPC port]"
        "[DB servers ip_address]:[DB servers port] \n");
        exit(EXIT_FAILURE);
    }

    supernode_addr = argv[2];
    string grpc_port(argv[1]);
    database_addr = argv[3];

    char interface[6];
    memcpy(interface, "kaist", 6);
    unsigned char *ip_address = (unsigned char *)malloc(MAX_IP_SIZE * sizeof(unsigned char));
    get_ip(ip_address, interface);

    string myip((char *)ip_address);
    myaddr = "" + myip + ":" + grpc_port;

    ServerImpl server;
    server.Run();
    return 0;
}