 Student ID:20170844
- Your Name:Bereket Assefa
- Submission date and time: 2020/dec 22 @ 2:20 am

## Ethics Oath
I pledge that I have followed all the ethical rules required by this course (e.g., not browsing the code from a disallowed source, not sharing my own code with others, so on) while carrying out this assignment, and that I will not distribute my code to anyone related to this course even after submission of this assignment. I will assume full responsibility if any violation is found later.

Name: ___Bereket Assefa___
Date: _____2020/dec 22 @ 2:20 am_

## Brief description
Briefly describe here how you implemented the project.
I implemented supernodes as an asynchronous multi-threaded(5 threads) server which serves both child nodes and peer supernode. The methods that supernode server implements is PeerGetKey and NodeGetKey. The supernode gives equal files to child nodes and half the file to the other supernode. Then child nodes will translate asynchronously. The supernode will asynchronously request translation. The child nodes also are multi-threaded (2 threads) , as well asynchronous for better efficiency. The child nodes implements Translate and TryKey to serve supernodes. the client uses a simple protocol similar to hw1 to send a file and also to receive a file I have used condition variables for the server thread that serves the client and will send it the file.

# Misc
Describe here whatever help (if any) you received from others while doing the assignment.

How difficult was the assignment? (1-5 scale) 4

How long did you take completing? (in hours) ~30 hrs