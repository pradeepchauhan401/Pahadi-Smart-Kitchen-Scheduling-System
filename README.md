# 🏔️ Smart Pahadi Kitchen Scheduling System

A C++ console application that simulates a smart food-delivery scheduling system for a Pahadi (mountain-style) kitchen. It uses real-time order batching, dynamic priority scoring, and classic OOP design patterns to dispatch orders efficiently across multiple delivery locations.

---

## 📋 Table of Contents

- [Features](#features)
- [Design Patterns & OOP Concepts](#design-patterns--oop-concepts)
- [How the Scheduling Works](#how-the-scheduling-works)
- [Menu](#menu)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Version History](#version-history)

---

## ✨ Features

- **Live terminal order entry** via a `do-while` loop — enter as many orders as you need, no upfront count required
- **Real-time batching** — orders for the same location placed within a 5–10 minute window are automatically grouped into a single delivery run
- **Fuzzy batch extension** — orders that arrive up to 1.5 minutes past the batch window are silently merged rather than creating a new batch
- **Variable batch window** — the window scales with average customer wait time at a location (5 min baseline → up to 10 min)
- **Priority-based dispatch** — a max-heap scheduler dispatches the highest-priority batch first
- **First-order boost** — the first order at any new location gets a small priority bump so early customers aren't pushed down by later high-distance orders
- **Multi-chef round-robin** — dispatched orders are assigned to chefs in rotation (default: 3 chefs)
- **Order customization** — spice level, Dahi, Extra Large, and three chutney options per order

---

## 🧱 Design Patterns & OOP Concepts

| Concept | Where it's used |
|---|---|
| **Singleton** | `KitchenManager` — one kitchen instance for the whole program |
| **Factory** | `FoodFactory::createItem(id)` — creates menu items by ID without exposing their concrete types |
| **Strategy** | `IPriorityStrategy` / `SmartSchedulingStrategy` — the scoring formula is swappable at runtime |
| **Polymorphism** | `MenuItem` base class with virtual `getPrepTime()` — each dish can override prep-time logic |
| **Smart Pointers** | `unique_ptr<MenuItem>` and `unique_ptr<IPriorityStrategy>` for safe heap ownership |

---

## ⚙️ How the Scheduling Works

### Priority Score

Every order (or batch) gets a score computed by `SmartSchedulingStrategy`:

```
score = (distance × 10.0) + (waitTime × 1.5) − (prepTime × 0.2)

if waitTime > 20 min  →  score × 1.5      (urgent escalation)
if first order at location  →  score + 8.0  (first-order boost)
```

Higher score = dispatched sooner. Orders are stored in a max-heap (`priority_queue`) and popped in descending score order.

### Batch Window

When a new order arrives for a location that already has an open batch:

```
avgWait = sum of all wait times at location / number of orders
window  = clamp(5.0 + avgWait × 0.5, 5.0, 10.0)  minutes
```

- If the new order's timestamp is within `window` minutes of the batch → **merged**
- If it is within `window + 1.5` minutes → **fuzzy-merged** (logged as `[FUZZY-BATCH]`)
- Otherwise → **new batch** created for that location

### Prep Time Modifiers

| Customization | Extra time |
|---|---|
| Extra Large | base × 1.4 |
| Any chutney | +1.5 min |
| Add Dahi | +2.0 min |

---

## 🍽️ Menu

| ID | Item | Base Prep Time |
|---|---|---|
| 1 | Pahadi Pakodi | 10 min |
| 2 | Paneer Tikka | 15 min |
| 3 | Special Pahadi Thali | 25 min |
| 4 | Kumaoni Chicken Fry | 20 min |
| 5 | Jhangora Soup | 7 min |
| 6 | Buransh Juice | 4 min |
| 7 | Pahadi Rai Saag | 12 min |

---

## 🚀 Getting Started

### Prerequisites

- A C++14-compatible compiler (`g++`, `clang++`, MSVC 2017+)
- No external libraries — uses only the C++ standard library

### Build

```bash
# g++ (Linux / macOS / WSL)
g++ -std=c++14 -O2 -o pahadi_kitchen pahadi_kitchen_v4.cpp

# Windows (MinGW)
g++ -std=c++14 -O2 -o pahadi_kitchen.exe pahadi_kitchen_v4.cpp

# clang++
clang++ -std=c++14 -O2 -o pahadi_kitchen pahadi_kitchen_v4.cpp
```

### Run

```bash
./pahadi_kitchen
```

---

## 🖥️ Usage

When the program starts it prints the menu, then enters the live order loop:

```
  --- Order #1 ---
  Customer name         : Riya Sharma
  Address / location    : Hostel-A Room-1
  Distance in km        : 0.8
  Already waited (min)  : 5
  Dish ID (1-7)         : 5
  Spice level (1-5)     : 3
  Add Dahi?  (1=Yes 0=No): 0
  Extra Large? (1=Yes 0=No): 0
  Chutney (0=None 1=Mint 2=Radish 3=Walnut): 3

  [ORDER]  #1 | Riya Sharma @ Hostel-A Room-1 | Jhangora Soup | initial-window=5.0 min [+FIRST-ORDER BOOST]

  Add another order? (y/n): y
```

After all orders are entered (`n`), the system prints the full dispatch table and a batching summary:

```
===================================================...
         SMART PAHADI KITCHEN -- DISPATCH ORDER (Highest Priority First)
===================================================...
Chef  ID   Customer            Location          Dist  Wait   Prep   Score   Merged  Items
---------------------------------------------------...
C1    3     Prof. Tiwari        Faculty Block-2   2.0   28.0   ...    ...     1       ...
C2    1     Riya Sharma         Hostel-A Room-1   0.8   5.0    ...    ...     2       ...
...
```

---

## 📁 Project Structure

```
pahadi_kitchen_v4.cpp        ← single-file implementation
README.md                    ← this file
```

All logic lives in one file, organised into clearly commented sections:

- **Real-time helper** — `getCurrentTime()` using `std::chrono`
- **Domain types** — `ChutneyType`, `Customization`, `Order`
- **Menu hierarchy** — `MenuItem` base + 7 concrete subclasses
- **Factory** — `FoodFactory`
- **Strategy** — `IPriorityStrategy`, `SmartSchedulingStrategy`
- **Singleton** — `KitchenManager` (batching, scheduling, dispatch, summary)
- **Main** — do-while input loop → `processQueue()` → `showBatchingSummary()`

---

## 🔧 Configuration

All tunable constants are `static constexpr` inside `KitchenManager` and `SmartSchedulingStrategy`:

| Constant | Default | Effect |
|---|---|---|
| `MIN_WIN` | `5.0` min | Minimum batch window per location |
| `MAX_WIN` | `10.0` min | Maximum batch window per location |
| `WAIT_SCALE` | `0.5` | How fast window grows with avg wait |
| `FUZZY_TOLERANCE` | `1.5` min | Grace period past the base window |
| `DIST_W` | `10.0` | Weight of distance in priority score |
| `WAIT_W` | `1.5` | Weight of wait time in priority score |
| `PREP_W` | `0.2` | Penalty weight of prep time in score |
| `WAIT_TH` | `20.0` min | Wait threshold that triggers 1.5× escalation |
| `FIRST_ORDER_BOOST` | `8.0` | Score bonus for first order at a location |
| `chefCount` | `3` | Number of chefs for round-robin assignment |

To change chef count at runtime, edit `kitchen->setChefCount(N)` in `main()`.

---

## 📝 Version History

| Version | Changes |
|---|---|
| v1 | Basic scheduling with static priority |
| v2 | Added Factory, Singleton, Strategy patterns |
| v3 | Real system clock via `std::chrono`; avg-wait variable window (16–25 min); fuzzy auto-extend (±2 min) |
| **v4** | Do-while live input loop; removed preloaded orders; tightened window to 5–10 min; fuzzy tolerance to 1.5 min; first-order priority boost (+8.0) |

---

## 👨‍💻 Author

Built as a demonstration of OOP design patterns (Factory, Singleton, Strategy) and real-time scheduling concepts in C++.

---

## 📄 License

This project is open source. Feel free to use, modify, and distribute it.
