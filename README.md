Look in the makefile for directions on how to compile code into static and dynamic libraries and place them into system directories so they can be used for different projects.

DISCLAIMER: This project was made for fun, do not use this code in any serious endeavors, there's still a lot of bugs and blank code bits to resolve.

This library comes with different modules to do the following operations asynchronously on Linux:
1) File system I/O
2) Simple DNS resolution
3) Socket programming (TCP, UNIX domain sockets/IPC) for listening servers and client connecting sockets
4) HTTP transaction flow with request and response handling (built on top of TCP server/socket module)
5) Child Process execution that feeds standard output and custom data to the main/ancestor process (using UNIX domain sockets)
