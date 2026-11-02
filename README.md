# Indigenous Land Name Record System (ILNRS)

A robust, object-oriented C++ console application designed to securely manage, record, and share indigenous place names, geographic coordinates, ecological observations, and access control tiers.

## ✨ Features

- **Role-Based Access Control (RBAC):** Supports four distinct user roles (`Admin`, `Elder`, `Youth Researcher`, and `Public Explorer`).
- **Access Tiers:** Place names are categorized into **Public** (Tier 1), **Community-Only** (Tier 2), and **Restricted** (Tier 3) with dynamic access request and approval workflows.
- **Persistent Storage:** Automatically saves records, access grants, observations, and audit trails to local text files (`records.txt`, `grants.txt`, `observations.txt`, `audit.txt`).
- **Comprehensive Audit Logging:** Tracks sensitive user actions and authorization checks with exact timestamps.
- **Robust Input Validation:** Guards against malformed data, out-of-bounds coordinates, and delimiter injection (`|`).

---

## 🚀 Getting Started

### Prerequisites
You need a C++ compiler that supports **C++11** or higher (e.g., `g++`, `clang++`, or MSVC).

### Compilation & Execution

1. **Clone the repository:**
   ```bash
   git clone [https://github.com/aakashnandhansn-lab/ilnrs.git](https://github.com/aakashnandhansn-lab/ilnrs.git)
   cd ilnrs