





![Screen Shot 2021-05-16 at 1 52 39 PM](https://user-images.githubusercontent.com/60803336/118432988-f1ddb580-b714-11eb-87b6-23084669095a.png)

## Brief description
I implemented supernodes as an asynchronous multi-threaded(5 threads) server which serves both child nodes and peer supernode. The methods that supernode server implements is PeerGetKey and NodeGetKey. The supernode gives equal files to child nodes and half the file to the other supernode. Then child nodes will translate asynchronously. The supernode will asynchronously request translation. The child nodes also are multi-threaded (2 threads) , as well asynchronous for better efficiency. The child nodes implements Translate and TryKey to serve supernodes. the client uses a simple protocol similar to hw1 to send a file and also to receive a file I have used condition variables for the server thread that serves the client and will send it the file.

# testing the code
- This project requires a lot of real databases with key and values


