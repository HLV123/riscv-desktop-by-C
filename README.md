# RISC-V RV32I Emulator

Emulator bộ vi xử lý **RISC-V RV32I** chạy dưới dạng ứng dụng desktop. Toàn bộ thuật toán mô phỏng CPU được viết bằng **C thuần**, giao diện quan sát và điều khiển được xây dựng bằng **Electron**. Hai phần này giao tiếp với nhau qua giao thức JSON trên stdin/stdout — JavaScript không thực thi bất kỳ lệnh RISC-V nào.

---

## Dành cho ai?

**Sinh viên ngành Khoa học Máy tính / Kỹ thuật Điện tử** đang học các môn Kiến trúc Máy tính, Hệ thống Nhúng, Hợp ngữ — muốn có công cụ trực quan thay vì chỉ đọc lý thuyết.

**Người tự học embedded systems** muốn hiểu cách CPU thực thi lệnh, cách memory-mapped I/O hoạt động, cách viết chương trình bare-metal không có OS.

**Lập trình viên C** muốn thực hành viết emulator, hiểu kiến trúc load/store, pipeline, instruction encoding.

---

## Project này giải quyết vấn đề gì?

Học kiến trúc máy tính từ sách giáo khoa thường rất trừu tượng. Bạn đọc về thanh ghi, về pipeline, về memory-mapped I/O — nhưng không có cách nào *nhìn thấy* chúng thực sự hoạt động như thế nào.

Project này cho phép bạn:

- Nạp một file binary RISC-V và xem từng lệnh được thực thi theo từng bước
- Quan sát 32 thanh ghi thay đổi giá trị sau mỗi lệnh
- Thấy LED GPIO bật tắt khi chương trình ghi vào địa chỉ `0x40000004`
- Thấy Timer đếm ngược và set cờ interrupt
- Thấy UART in từng ký tự ra console khi chương trình ghi vào `0x40002000`
- Đặt breakpoint, pause, step — giống debugger thực tế trên vi điều khiển

---

## Tech stack

### Backend — C (1.500 dòng)

Toàn bộ logic mô phỏng được viết bằng C11 thuần, không có dependency ngoài. Biên dịch thành một file binary duy nhất chạy ở chế độ `--json`, nhận lệnh qua stdin và trả kết quả JSON qua stdout.

| Thành phần | Công nghệ |
|---|---|
| Ngôn ngữ | C11 |
| Build system | CMake 3.10+ |
| Compiler | GCC (MinGW trên Windows) |
| Instruction set | RISC-V RV32I (47 lệnh) |
| Memory model | ROM 64KB + RAM 64KB |
| Peripheral | GPIO, Timer, UART — memory-mapped |

### Frontend — Electron + Vanilla JS

Giao diện hoàn toàn là HTML/CSS/JavaScript thuần, không dùng framework. Electron đóng vai trò shell để spawn C process và expose API qua `contextBridge`.

| Thành phần | Công nghệ |
|---|---|
| Desktop shell | Electron 36 |
| UI | HTML5 + CSS3 + Vanilla JS |
| IPC | Electron `ipcMain` / `ipcRenderer` |
| Giao tiếp với C | `child_process.spawn` + stdin/stdout JSON |

### Tools — Python

Script sinh file `.bin` cho các chương trình demo, encode trực tiếp RISC-V machine code không cần assembler bên ngoài.

| Thành phần | Công nghệ |
|---|---|
| Demo generator | Python 3 |
| Instruction encoder | Viết tay, không dùng thư viện |

---

## Cấu trúc thư mục

```
riscv-desktop/
│
├── main.js                  # Electron main process
│                            # Spawn C binary, relay IPC, file dialog
│
├── preload.js               # Electron preload — expose API an toàn
│                            # cho renderer qua contextBridge
│
├── package.json             # Node.js config, Electron dependency
├── CMakeLists.txt           # Build config cho C binary
│
├── src/                     # C source — toàn bộ emulator core
│   ├── main_cli.c           # Entry point, vòng lặp JSON protocol
│   ├── cpu.c / cpu.h        # RV32I CPU: decode, execute, 32 registers
│   ├── bus.c / bus.h        # Address bus: routing read/write đến đúng vùng
│   ├── memory.c / memory.h  # ROM và RAM, load binary
│   ├── emulator.c / .h      # Orchestrator: kết nối CPU + Bus + Peripheral
│   ├── waveform.c / .h      # Ghi lại lịch sử tín hiệu GPIO/Timer/UART
│   └── peripheral/
│       ├── peripheral.h     # Interface chung cho tất cả peripheral
│       ├── gpio.c / .h      # GPIO 2 port × 8 bit (0x40000000)
│       ├── timer.c / .h     # Timer 32-bit countdown (0x40001000)
│       └── uart.c / .h      # UART TX buffer 8KB (0x40002000)
│
├── web/                     # Frontend — giao diện Electron renderer
│   ├── index.html           # Layout: Toolbar, Disassembly, Registers,
│   │                        # Peripherals, Memory Inspector
│   ├── css/
│   │   └── style.css        # Dark theme (Catppuccin Mocha palette)
│   └── js/
│       └── renderer.js      # Toàn bộ UI logic: gửi lệnh → nhận JSON
│                            # → render DOM. Không có simulation code.
│
├── programs/                # RISC-V binary demos (.bin)
│   ├── fibonacci.bin        # Dãy Fibonacci
│   ├── sum_1_to_10.bin      # Tính tổng 1..10
│   ├── gpio_blink.bin       # Blink LED
│   ├── timer_test.bin       # Timer interrupt
│   ├── knight_rider.bin     # LED animation + binary counter
│   ├── led_show.bin         # 32 LED pattern, 80 vòng
│   ├── pwm_fade.bin         # LED fade in/out
│   ├── lfsr_random.bin      # LFSR-16 random generator
│   ├── counter_marathon.bin # Đếm 0→32767
│   ├── prime_sieve.bin      # Sàng Eratosthenes N=200
│   ├── bubble_sort.bin      # Bubble sort 32 phần tử
│   └── matrix_mul.bin       # Nhân ma trận 4×4
│
├── tools/                   # Python utilities
│   ├── gen_demos.py         # Sinh file .bin cho tất cả demo
│   └── asm.py               # RISC-V assembler đơn giản
│
├── build/                   # Output của CMake (tự sinh)
│   └── riscv_emu_cli.exe    # C binary sau khi build
│
└── README.md                # Hướng dẫn sử dụng GUI chi tiết
```

---

## Memory map

```
0x00000000 ── 0x0000FFFF   ROM   64KB   Chứa machine code (read-only)
0x20000000 ── 0x2000FFFF   RAM   64KB   Dữ liệu runtime, stack, heap
0x40000000 ── 0x4000001F   GPIO  32B    Port A + Port B registers
0x40001000 ── 0x4000100F   Timer 16B    CTRL, LOAD, CNT, STATUS
0x40002000 ── 0x40002008   UART  12B    DR, SR, CR
```

---

## Giao thức C ↔ Electron

C binary chạy ở chế độ `--json` như một long-running process. Electron giao tiếp qua stdin/stdout — mỗi lệnh một dòng, mỗi phản hồi một dòng JSON.

```
Electron                          C binary
   │                                 │
   │── "load programs/fib.bin" ──▶   │  load file vào ROM
   │◀── {"ok": true} ────────────    │
   │                                 │
   │── "run 10000" ──────────────▶   │  thực thi 10000 cycles
   │◀── {"state":"RUNNING",          │
   │     "pc": 104,                   │
   │     "regs": [...],               │
   │     "gpio": {...},               │
   │     "timer": {...},              │
   │     "uart": "Fib:\n0\n1\n..."} ─ │
   │                                  │
   │── "step" ──────────────────▶    │  thực thi 1 lệnh
   │◀── {"state":"PAUSED", ...} ──   │
```

---

## Hướng dẫn sử dụng GUI

Xem file **[README2.md](README2.md)** để biết chi tiết từng thành phần giao diện, cách dùng breakpoint, ý nghĩa từng register, cách đọc Memory Inspector và gợi ý Speed phù hợp cho từng chương trình demo.
