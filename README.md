


# Overview
In this Project, I designigned and implemented a distributed computing
framework that translates source keywords to target values in parallel. By leveraging multiple machines in a distributed environment, better performance can be provided. When you implement a distributed computing framework, your framework should provide two core concepts, Parallelism and Remote Procedure Call (RPC). Parallel processing saves as much time as the many resources you work with. RPC hides all of the network code into stub functions, so application programs don’t have to worry about socket-level details.
The framework follows ‘master-slave architecture’, which has two types of nodes; 1) super node and 2) child node. Super nodes accept the client’s request and split a large file into smaller files. And then, they send them to child nodes. Child nodes receive small files and translates them with the key-value information communicated with the remote database (DB) server. Super nodes can communicate each other, but child nodes only communicate with connected super node. Figure 1 shows the topology of the framework.

# topology of the framework
![Screen Shot 2021-05-17 at 1 36 47 PM](https://user-images.githubusercontent.com/60803336/118433024-04f08580-b715-11eb-9c23-7b416838fb91.png)

## Brief description
I implemented supernodes as an asynchronous multi-threaded(5 threads) server which serves both child nodes and peer supernode. The methods that supernode server implements is PeerGetKey and NodeGetKey. The supernode gives equal files to child nodes and half the file to the other supernode. Then child nodes will translate asynchronously. The supernode will asynchronously request translation. The child nodes also are multi-threaded (2 threads) , as well asynchronous for better efficiency. The child nodes implements Translate and TryKey to serve supernodes. the client uses a simple protocol similar to hw1 to send a file and also to receive a file I have used condition variables for the server thread that serves the client and will send it the file.

# framework advantages
- parallelism
- gRPC
- DB miss handling
- Cache management

# Requirements for testing the code
- This project requires real databases with key as source and values as translation

# how to run the client 
./client [input_file] [ip] [port]

# how to run supernode 1

./super 12345 [gRPC port] [child1’s ip_address]:[child1’s port] [child2’s ip_address]:[child2’s port] [child3’s ip_address]:[child3’s port] ...

# how to run supernode 2
./super 12346 [gRPC port] -s [another super node’s ip_address]:[another super node’s port] [child4’s ip_address]:[child4’s port] [child5’s ip_address]:[child5’s port] [child6’s ip_address]:[child6’s port] ...

# how to run child node 
./child 50051 [super node’s ip_address]:[super node’s gRPC port] [DB server’s ip_address]:[DB server’s port]
