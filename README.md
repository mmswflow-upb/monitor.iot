# Monitor.IoT 🌐📷💡🌡️
End-to-End Service for Monitoring & Controlling IoT Devices

# Description

This is a smart things project which implements cameras, sensors and rgb lamps that can be easily controlled or monitored by users through an app that can run on any device as long as it's connected to the internet. Some of the key features are:

- Enables monitoring and controlling installed smart devices from anywhere in the world at any time
- User-friendly, very easy to setup and start using it, it only requires that devices stay connected to the internet
- Allows for multiple users simulataineously to connect on the same account at the same time or just have multiple users with different accounts using it at the same time

# Architecture

![a](monitor.iot_architecture.png)

- Micorcontrollers (smart devices) equipped with cameras, connected to sensors or RGB lamps
- A client app that can run on mobile or PC
- An expressJS-based server hosted on Heroku, that handles socket connections in order to exchange data between the client app and MCUs by using Redis pub/sub channels
- A database hosted on MongoDB Atlas for storing user credentials

# Technical Details

Enter the directories for the different parts of the project (backend, frontend, firmware), and you will find there details about each one of them.
