# grSim Repository Audit

## Overview
- **Project**: grSim — RoboCup Small Size League Simulator
- **Version**: 1.0.0a2
- **License**: GPL-3.0
- **Language**: C++11 with Qt5
- **Build System**: CMake 3.5+
- **Total Source Lines**: ~7,515 (excluding generated protobuf)
- **Audit Date**: 2026-04-14
- **Auditor**: grsim-rl project

## Source Tree Structure

```
grsim-rl/
├── CMakeLists.txt          # Single monolithic build file
├── vcpkg.json              # Package manager (qt5-base, ode, protobuf)
├── src/
│   ├── main.cpp            # Entry point, QApplication, headless flag
│   ├── mainwindow.cpp      # GUI main window, timer-driven step loop
│   ├── sslworld.cpp        # *** CORE: world setup, physics step, vision, protocol ***
│   ├── robot.cpp           # Robot body, wheels, kicker, velocity control
│   ├── configwidget.cpp    # VarTypes-based configuration system
│   ├── glwidget.cpp        # OpenGL rendering widget
│   ├── graphics.cpp        # Graphics abstraction (drawSphere, drawCylinder, etc.)
│   ├── statuswidget.cpp    # Status bar widget
│   ├── robotwidget.cpp     # Robot parameter display widget
│   ├── getpositionwidget.cpp # Position input widget
│   ├── logger.cpp          # Simple logging utility
│   ├── net/
│   │   ├── robocup_ssl_server.cpp  # UDP multicast server (vision output)
│   │   └── robocup_ssl_client.cpp  # UDP client (unused in main, for example clients)
│   ├── physics/
│   │   ├── pworld.cpp      # ODE world wrapper, collision handling, stepping
│   │   ├── pobject.cpp     # Base physics object (body + geom)
│   │   ├── pball.cpp       # Sphere physics object
│   │   ├── pcylinder.cpp   # Cylinder physics object
│   │   ├── pbox.cpp        # Box physics object
│   │   ├── pfixedbox.cpp   # Static box (walls, goals)
│   │   ├── pground.cpp     # Ground plane
│   │   └── pray.cpp        # Ray for mouse picking
│   └── proto/              # 14 protobuf definitions
│       ├── grSim_*.proto   # Legacy grSim command protocol (4 files)
│       └── ssl_*.proto     # Official SSL simulation protocol (10 files)
├── include/                # Headers mirror src/ structure
├── config/                 # INI configuration files
├── resources/              # Textures, icons, Qt resources
├── clients/qt/             # Example Qt client
├── cmake/                  # CMake modules (ODE build, protobuf find)
└── docs/                   # Minimal existing docs
```

## Dependencies

| Dependency | Purpose | Coupling Level |
|-----------|---------|---------------|
| Qt5 (Core, Widgets, OpenGL, Network) | GUI, event loop, UDP sockets, OpenGL context | **Very High** — pervasive |
| ODE (Open Dynamics Engine) | Rigid body physics simulation | **High** — physics layer |
| Protobuf | SSL protocol serialization | **Medium** — network layer |
| VarTypes | Configuration tree GUI widget | **Medium** — config system |
| OpenGL | 3D rendering | **Medium** — graphics layer |

## Key Classes and Their Roles

### SSLWorld (sslworld.h/cpp) — ~1,050 lines
**The central god-object of grSim.** Combines:
- Physics world construction (walls, ball, robots, surfaces)
- Simulation stepping with ball friction model
- Vision packet generation (SSL_WrapperPacket)
- UDP socket handling for commands, control, status
- SSL simulation protocol handling (teleport, robot specs)
- Camera simulation (multi-camera visibility)
- Mouse picking / cursor interaction
- Robot creation and formation management

**Critical coupling points:**
- Inherits QObject for signals/slots
- Takes QGLWidget* parent (for graphics ratio)
- Global pointer `_w` used by ODE collision callbacks
- Mixes rendering (g->initScene, g->drawSkybox) in step()
- UDP socket operations interleaved with physics

### Robot (robot.h/cpp) — ~525 lines
- 4-wheel omni-directional robot model
- Nested Wheel class (ODE hinge + motor joints)
- Nested Kicker class (kick, chip, dribbler/roller, ball holding)
- Velocity control with acceleration limits
- Body velocity → wheel velocity decomposition
- Drawing (OpenGL label rendering) mixed with physics

### PWorld (physics/pworld.h/cpp) — ~190 lines
- ODE world/space wrapper
- Surface/contact matrix for collision pairs
- Collision callback dispatch
- Physics step (dWorldStep)
- Drawing delegation to objects

### ConfigWidget (configwidget.h/cpp)
- VarTypes-based configuration tree
- Macro-heavy field definitions (DEF_VALUE, DEF_FIELD_VALUE, DEF_ENUM)
- Division A/B field dimension support
- Robot physical/geometric settings (RobotSettings struct)
- Network port configuration
- Ball physics parameters
- Noise/vanishing parameters

### MainWindow (mainwindow.h/cpp) — ~400 lines
- QMainWindow with timer-driven update loop
- Creates SSLWorld, ConfigWidget, GLWidget
- Manages all UDP sockets (vision, command, control, status)
- Timer fires → SSLWorld::step() → vision packets sent
- Socket signal routing to SSLWorld slot handlers
- Restart logic on config changes

## Protocol Support

### Legacy grSim Protocol (port 20011 default)
- `grSim_Packet` containing:
  - `grSim_Commands`: wheel speeds or body velocity per robot
  - `grSim_Replacement`: teleport robots and ball

### SSL Simulation Protocol (ports 10300, 10301, 10302)
- `SimulatorCommand`/`SimulatorResponse`: teleport, config, robot specs
- `RobotControl`/`RobotControlResponse`: wheel/local/global velocity, kick, dribble
- Supports: teleport ball, teleport robot, robot specs, move commands
- Missing: safe teleport, roll ball, geometry config, realism config, vision port config

### Vision Output (multicast 224.5.23.2:10020)
- `SSL_WrapperPacket`: detection frames + geometry
- Multi-camera simulation (4 quadrants)
- Configurable noise and vanishing
- Ball model parameters in geometry

## Headless Mode

Current headless support (`--headless` / `-H` flag):
- Hides window, disables GL
- Still requires QApplication (event loop for sockets)
- Still initializes CGraphics
- Timer still drives stepping
- All UDP networking still active
- **Cannot step faster than wall clock without custom DT hack**

## Global State Issues

1. `SSLWorld* _w` — global pointer used by ODE collision callbacks
2. `QColor ROBOT_BLUE_CHASSIS_COLOR` / `ROBOT_YELLOW_CHASSIS_COLOR` — global color vars
3. `rand0_1()`, `randn_*()` — global random functions (no seed control)

## Identified Refactoring Targets

### For grsim_core extraction:
1. **SSLWorld::step()** — must separate physics step from rendering
2. **Ball friction model** — inline in step(), needs extraction
3. **PWorld** — remove CGraphics dependency for headless
4. **PObject hierarchy** — remove draw/glinit for headless
5. **Robot** — remove drawLabel, QImage dependencies
6. **ConfigWidget** — extract pure-data config from Qt widget
7. **Global _w pointer** — replace with context parameter
8. **Random number generation** — add seed control

### For grsim_realtime preservation:
1. MainWindow timer loop → keep as-is, delegate to core
2. UDP sockets → adapter layer between core and network
3. GLWidget/graphics → keep coupled to Qt, reference core state

### For Python bindings:
1. Need C API or pybind11 wrapper around core step/reset/observe/act
2. ConfigWidget data → pure struct or JSON-loadable config
3. State serialization for snapshot/restore
4. ODE world serialization is non-trivial (may need rebuild-from-state)

## Build System Notes

- CMake monolithic (single CMakeLists.txt)
- External project builds for VarTypes and optionally ODE/Protobuf
- Qt5 MOC for QObject-derived classes
- Protobuf codegen for .proto files
- Qt resource compilation for textures
- Cross-platform: Windows (MSVC), Linux, macOS
- vcpkg support for dependency management

## Risk Assessment

| Risk | Severity | Mitigation |
|------|---------|------------|
| ODE state not serializable | High | Rebuild world from state snapshot |
| Global _w pointer prevents multiple instances | High | Context parameter or instance registry |
| Qt event loop required even headless | Medium | Alternative event loop or pure step mode |
| VarTypes deep coupling to config | Medium | Extract config struct, make VarTypes optional |
| No test infrastructure | Medium | Add from scratch |
| Single-threaded physics | Low | Acceptable for RL (batching via processes) |
