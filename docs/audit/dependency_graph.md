# grSim Dependency Graph

## Module Dependency Map

```
┌─────────────────────────────────────────────────────────────────┐
│                        main.cpp                                  │
│                    QApplication + MainWindow                     │
└──────────┬──────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      MainWindow                                  │
│  ┌─────────┐  ┌──────────┐  ┌──────────┐  ┌───────────────┐   │
│  │GLWidget  │  │ConfigWdgt│  │RobotWdgt │  │StatusWidget   │   │
│  └────┬─────┘  └────┬─────┘  └──────────┘  └───────────────┘   │
│       │              │                                           │
│  QTimer ──── fires ──┼──── SSLWorld::step()                     │
│       │              │              │                             │
│  UDP Sockets ────────┼──── recvActions(), *ControlSocketReady() │
└──────────┬───────────┼──────────────┼───────────────────────────┘
           │           │              │
           ▼           ▼              ▼
┌─────────────────────────────────────────────────────────────────┐
│                       SSLWorld                                   │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────────┐  │
│  │ PWorld   │  │ PBall    │  │ Robot[]  │  │ PFixedBox[]   │  │
│  │ (ODE)    │  │          │  │          │  │ (walls/goals) │  │
│  └────┬─────┘  └──────────┘  └────┬─────┘  └───────────────┘  │
│       │                           │                              │
│       │         ┌─────────────────┤                              │
│       │         │                 │                               │
│       ▼         ▼                 ▼                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                      │
│  │CGraphics │  │ Wheel[]  │  │ Kicker   │                      │
│  │(OpenGL)  │  │(4 per bot│  │(per bot) │                      │
│  └──────────┘  └──────────┘  └──────────┘                      │
│                                                                  │
│  ┌──────────────────────┐  ┌────────────────────────────────┐  │
│  │ RoboCupSSLServer     │  │ QUdpSocket (cmd, ctrl, status)│  │
│  │ (vision multicast)   │  │                                │  │
│  └──────────────────────┘  └────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Protobuf messages (grSim_* + ssl_*)                      │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Class-Level Dependencies

### SSLWorld depends on:
- QObject, QGLWidget*, QUdpSocket, QList, QElapsedTimer (Qt)
- PWorld, PBall, PGround, PFixedBox, PRay (physics)
- CGraphics (rendering)
- Robot (robot model)
- ConfigWidget (configuration)
- RoboCupSSLServer (vision output)
- All protobuf message types

### Robot depends on:
- PWorld, PCylinder, PBox, PBall (physics)
- ConfigWidget (configuration)
- QImage (textures — GUI coupling!)
- OpenGL functions (drawLabel)

### PWorld depends on:
- ODE (dWorld, dSpace, dJointGroup)
- CGraphics (drawing — optional for headless)
- PObject, PSurface

### PObject hierarchy depends on:
- ODE (dBody, dGeom, dMass)
- CGraphics (draw, glinit — GUI coupling)

### ConfigWidget depends on:
- VarTypes library (VarTreeView, VarList, VarDouble, etc.)
- QWidget, QSettings, QDockWidget (Qt GUI)

## External Library Dependency Chain

```
grSim
  ├── Qt5::Core          (event loop, containers, strings)
  ├── Qt5::Widgets       (GUI framework)
  ├── Qt5::OpenGL        (GL widget)
  ├── Qt5::Network       (UDP sockets)
  ├── ODE                (physics engine)
  ├── Protobuf           (serialization)
  ├── VarTypes           (config GUI — fetched via ExternalProject)
  └── OpenGL             (rendering)
```

## Coupling Analysis for Core Extraction

### Hard Couplings (must break):
1. `SSLWorld::step()` calls `g->initScene()` and `g->finalizeScene()` — rendering in physics step
2. `SSLWorld::step()` calls `p->draw()` and `g->drawSkybox()` — rendering in physics step
3. `PWorld` constructor takes `CGraphics*` — physics needs graphics
4. All `PObject` subclasses have `draw()` and `glinit()` — physics objects know about rendering
5. `Robot::drawLabel()` uses raw OpenGL calls
6. `Robot` stores `QImage* img, *number` — GUI data in physics model
7. Global `SSLWorld* _w` — ODE callbacks need world reference
8. `SSLWorld` takes `QGLWidget* parent` — physics needs GL widget for device pixel ratio
9. `ConfigWidget` inherits `VarTreeView` — config is a GUI widget

### Soft Couplings (can adapter):
1. UDP sockets in SSLWorld/MainWindow — network adapter
2. Protobuf serialization in SSLWorld — protocol adapter  
3. Timer-driven step in MainWindow — timing adapter
4. Qt signal/slot for socket readiness — event adapter

## Proposed Layer Boundaries

```
┌─────────────────────────────────────────────────┐
│  Layer 4: pygrsim (Python)                      │
│  Gymnasium/PettingZoo envs, NumPy observations  │
├─────────────────────────────────────────────────┤
│  Layer 3: grsim_ref (C++)                       │
│  Event detection, referee integration           │
├─────────────────────────────────────────────────┤
│  Layer 2: grsim_realtime (C++ / Qt)             │
│  GUI, UDP networking, timer loop, visualization │
├─────────────────────────────────────────────────┤
│  Layer 1: grsim_core (C++)                      │
│  Pure physics: ODE world, robots, ball, config  │
│  step(), reset(), snapshot(), observe(), act()  │
│  No Qt, No OpenGL, No network, No protobuf     │
└─────────────────────────────────────────────────┘
```

## Data Flow: Current vs Target

### Current (real-time mode):
```
Timer tick → SSLWorld::step()
  → ball friction → ODE step (×5) → selection logic
  → robot steps → draw world → draw skybox → draw cursor
  → generate vision packets → send via UDP multicast
  
UDP recv → recvActions() / *ControlSocketReady()
  → parse protobuf → apply wheel speeds / teleport
```

### Target (training mode):
```
Python call → grsim_core::step(actions)
  → apply actions → ball friction → ODE step (×5)
  → robot steps → extract observations
  → return (obs, reward, done, info)
  
Python call → grsim_core::reset(seed, scenario)
  → destroy/rebuild ODE world OR restore snapshot
  → apply scenario initial state
  → return initial observations
```
