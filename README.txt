Project:    SNT-DEVS-2023
Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
Date:       10.04.2023

This is a DEVS simulation library and a demo application with four examples.

Single header DEVS library: ./include/devs/lib.hpp
It supports atomic and compound DEVS models with arbitrary nesting, printing, and more.

The other files belong to the demo application.
Application build dependencies:
  - GCC         v9.5.0
  - GNU Make    v3.82

Building the application:
  - make

Building the application in debug mode:
  - make debug

Running the application:
  - ./bin/devs_demo_app [ARGUMENTS]

Running the debug application build:
  - ./bin/devs_demo_app_d [ARGUMENTS]

Running the application using Make (the build command can be omitted):
  - make run-[ARGUMENTS]

Running the debug application build using Make (the build command can be omitted):
  - make debug-run-[ARGUMENTS]

where ARGUMENTS are the following:

  -h | --help           - Display help message and the example list.
  minimal-atomic        - Empty atomic model.
  minimal-compound      - Empty compound model.
  traffic-light         - Traffic light example with input and output messages.
  queue-short           - Queue theory example with a 10 minute duration.
  queue-long            - Queue theory example with a 10 day duration (same parameters as queue-short).
  queue-large           - Queue theory example with a 1 hour duration (queue-short arrivals and duration 
                                                                        multiplied by a factor of 10).

More than one example can be provided for running.
Examples:
  - ./bin/devs_demo_app
  - ./bin/devs_demo_app -h
  - ./bin/devs_demo_app queue-short
  - ./bin/devs_demo_app queue-long queue-large
  - ./bin/devs_demo_app_d queue-short
  - make run
  - make run--h
  - make run-queue-short
  - make run-"queue-long queue-large"
  - make debug-run-queue-short