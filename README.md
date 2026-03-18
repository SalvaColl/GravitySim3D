# 🌌 3D Gravity Simulator

A real-time, interactive 3D solar system simulator built from scratch using C++ and OpenGL. 

This project uses Newtonian physics and numerical integration to calculate the gravitational pull between celestial bodies frame-by-frame and the real data for distances, radii and speed. 

## Features

* **True-to-Life Physics:** Simulates gravitational forces using real-world constants, masses, and initial orbital velocities.
* **Compact View (Logarithmic Scaling):** Space is too big. A toggleable "Compact View" applies square-root scaling to orbital distances with respect to 1 AU to pull gas planets into visual range without breaking the math.
* **Dynamic Orbit Trails:** Press R to map out the exact path of a planet.
* **Optimization:** I get around 800-900 fps on my laptop. Toggle off V-sync for this.
* **Dynamic Camera System:** Flying camera as well as target-locking (Side View and Top-Down) for each body.

## 🎮 Controls & Usage 

### Camera & Movement
| Key | Action |
| :--- | :--- |
| **W, A, S, D** | Move camera Forward, Left, Backward, Right (Breaks target lock) |
| **Space** / **L-Shift** | Move camera Up / Down |
| **L-Ctrl (Hold)** | Sprint (5x camera speed) |
| **Mouse** | Look around |

### Simulation Controls
| Key | Action |
| :--- | :--- |
| **Right / Left Arrow** | Cycle camera target |
| **Up / Down Arrow** | Increase / Decrease Simulation Time Scale (Fast-forward time) |
| **9 / 2** | Increase / Decrease the visual render size of the planets |
| **C** | Toggle **Compact View** |
| **R** | Toggle **Orbit Recording** for the currently tracked planet |
| **Q** | Quit Simulation |

## Dependencies

* **Language:** C++ Compiler
* **Graphics API:** OpenGL (Version 3.3+ Core)
* **Window/Input:** [GLFW](https://www.glfw.org/)
* **GL Loader:** [GLAD](https://glad.dav1d.de/)
* **Mathematics:** [GLM](https://github.com/g-truc/glm) 
* **GUI:** [Dear ImGui](https://github.com/ocornut/imgui)

Everything is ready in the download, just set up a C++ compiler and type ./build in the terminal