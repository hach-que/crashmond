cmake_minimum_required(VERSION 2.8.5)
project(ancillary)

add_library(ancillary STATIC
    ancillary.h
    fd_recv.c
    fd_send.c
)
set_property(TARGET ancillary PROPERTY FOLDER "third-party")
cotire(ancillary)
