# Traffic Simulator - Project Documentation

---

## Assignment Information

**Assignment Number:** Assignment 1 - Queue Data Structure Implementation  
**Course:** Data Structures and Algorithms  
**Student Name:** [Aashutosh karki]  
**Roll Number:** [33]  
**Semester:** [3rd semester]  
**Date of Submission:** December 28, 2025  

---

## 1. Traffic Junction Simulation Using Queue

A traffic junction connects two major roads, forming a central point where vehicles must choose one of the available paths to continue. This project simulates a traffic junction management system using queue-based linear data structures. Vehicles arrive continuously from two perpendicular roads and are managed using traffic signals, predefined lane rules, and FIFO queues.

The simulation demonstrates how linear data structures such as queues can be applied to solve real-world traffic management problems. SDL2 is used to visualize vehicle movement, lane behavior, and signal-controlled intersections in real time.

---

## 2. Objective

The objectives of this assignment are as follows:

- To apply queue data structures for managing vehicle flow  
- To simulate a traffic junction with multiple roads and lanes  
- To visualize traffic movement and signal control using SDL2  
- To enforce signal-based stopping and movement rules  
- To support continuous vehicle generation  

---

## 3. System Details

### 3.1 Road and Lane Design

The junction consists of two perpendicular roads:

- **Road A:** Vertical direction (North to South)  
- **Road C:** Horizontal direction (East to West)  

These roads intersect at the center of the simulation window.

Each road contains three lanes:

- **Lane 1:** Right-side normal lane  
- **Lane 2:** Main lane (signal-controlled)  
- **Lane 3:** Free left-turn lane  

Lane rules are defined as follows:

- Vehicles in Lane 2 must obey traffic signal states  
- Vehicles in Lane 3 are allowed to turn left without stopping  
- Lane 1 and Lane 3 are not affected by signal states  
- Vehicles remain aligned to the center of their respective lanes during movement  

---

### 3.2 Vehicle Model

Each vehicle in the simulation contains the following attributes:

- Road identifier (A or C)  
- Lane number (1–3)  
- Position coordinates (x, y)  
- Speed  
- Crossing state  

Vehicle states are defined as:

- **State 0:** Approaching the junction  
- **State 1:** Crossed the junction  
- **State 2:** Exited the simulation  

---

### 3.3 Queue Implementation

Two FIFO (First-In-First-Out) queues are used in the system:

- One queue for Road A  
- One queue for Road C  

Each queue:

- Stores vehicles in order of arrival  
- Has a fixed maximum size  
- Uses enqueue-only insertion without reordering  

This fulfills the requirement of using linear data structures for vehicle management.

---

### 3.4 Traffic Signal System

The traffic signal system operates using three signal states:

- **ALL_RED:** All vehicles must stop  
- **AB_GREEN:** Vehicles on Road A are allowed to move  
- **CD_GREEN:** Vehicles on Road C are allowed to move  

Signal behavior rules:

- Only one road can have a green signal at a time  
- All other roads remain red to prevent deadlock  
- Only vehicles in Lane 2 are affected by signal states  

Signal control is handled using a separate thread.

---

### 3.5 Signal Timing Logic

The traffic signal follows a fixed timing cycle:

- Road A green for 5 seconds  
- All red for 2 seconds  
- Road C green for 5 seconds  
- All red for 2 seconds  

This timing mechanism ensures safe and fair vehicle movement through the junction.

---

### 3.6 Vehicle Movement Logic

Vehicle behavior in the simulation includes:

- Continuous movement toward the junction  
- Automatic alignment to lane centers  
- Stopping before the junction when the signal is red (Lane 2 only)  
- Left turns permitted from Lane 3 after crossing  
- Automatic removal after exiting the visible area  

---

### 3.7 Vehicle Generation

Vehicles are generated continuously using a dedicated generator thread.

Spawn behavior:

- Road A vehicles spawn above the window  
- Road C vehicles spawn to the right of the window  
- Vehicles are generated in all three lanes  
- Spawn interval is 1.2 seconds  

This provides continuous traffic flow throughout the simulation.

---

## 4. Programming Requirements

The simulator is implemented using the **C programming language** with **SDL2** for visualization.

### 4.1 Multithreading

The system uses multiple threads:

- Signal thread for traffic light control  
- Generator thread for vehicle creation  
- Main thread for rendering and vehicle updates  

This design ensures real-time simulation without blocking execution.

---

### 4.2 Libraries Used

The following libraries are used:

- Simple DirectMedia Layer (SDL2) for graphics rendering  
- Windows threading library (CreateThread) for concurrency  
- Standard C libraries for data handling and timing  

---

## 5. Key Features

- Continuous vehicle spawning  
- Signal-controlled traffic flow  
- Lane-based movement logic  
- Queue-based vehicle management  
- Deadlock-free signal operation  
- Real-time graphical visualization  

---

## 6. Conclusion

This project demonstrates the effective use of queue-based linear data structures in managing traffic flow at a junction. By combining FIFO queues, traffic signal control, lane-based movement rules, and multithreading, the simulation models realistic traffic behavior.

The use of SDL2 provides clear visual representation, making the system suitable for academic analysis and demonstration of data structure concepts applied to real-world problems.

---
