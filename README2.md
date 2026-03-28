# ⚡ RISC-V RV32I Emulator — Desktop GUI

Ứng dụng desktop mô phỏng bộ vi xử lý **RISC-V RV32I** chạy hoàn toàn bằng C, giao diện điều khiển bằng Electron. Dùng để học kiến trúc máy tính, hiểu cách CPU thực thi lệnh, quan sát peripheral hoạt động theo thời gian thực.

---

## Mục lục

- [Bạn học được gì?](#bạn-học-được-gì)
- [Cài đặt và chạy](#cài-đặt-và-chạy)
- [Giao diện chi tiết](#giao-diện-chi-tiết)
  - [Toolbar](#toolbar)
  - [Disassembly](#disassembly)
  - [Registers](#registers)
  - [Peripherals](#peripherals)
  - [Memory Inspector](#memory-inspector)
- [Chương trình demo](#chương-trình-demo)
- [Kiến trúc kỹ thuật](#kiến-trúc-kỹ-thuật)
- [Keyboard shortcuts](#keyboard-shortcuts)

---

## Bạn học được gì?

### Hiểu CPU hoạt động như thế nào

Mỗi lần bấm **Step**, bạn thấy đúng một lệnh được thực thi: PC tăng lên 4 byte, register thay đổi, memory bị ghi. Không còn là lý thuyết trừu tượng — bạn thấy tận mắt từng bước fetch → decode → execute.

### Đọc và hiểu Assembly

Panel Disassembly hiển thị toàn bộ chương trình đã được dịch ngược từ binary sang assembly. Bạn đọc được từng lệnh như `addi`, `sw`, `bge`, `jal` và hiểu tại sao chương trình lại sinh ra những lệnh đó.

### Quan sát memory layout

Memory Inspector cho thấy ROM (code nằm ở đâu), RAM (dữ liệu được lưu như thế nào), địa chỉ hex của từng byte. Bạn hiểu được tại sao pointer là con số `0x20000000`.

### Hiểu peripheral I/O

GPIO, Timer, UART không còn là khái niệm mơ hồ — bạn thấy chính xác chương trình ghi giá trị `0xFF` vào địa chỉ `0x40000004` thì LED nào bật, Timer đếm ngược đến 0 thì cờ IRQ được set, UART nhận byte nào thì in ký tự gì ra console.

### Debug thực tế

Breakpoint, step-by-step, quan sát register thay đổi — đây là workflow debug thực tế của embedded engineer. Bạn tập được thói quen đọc state machine của CPU.

---

## Cài đặt và chạy

### Bước 1 — Cài MSYS2

Tải file cài đặt tại [msys2.org](https://www.msys2.org):

```
msys2-x86_64-20260322.exe
```

Chạy file đó, cài vào đường dẫn mặc định `C:\msys64`. Sau khi cài xong, MSYS2 tự mở terminal — đóng nó lại, **không dùng terminal này**.

---

### Bước 2 — Cài Node.js

Tải và cài [Node.js LTS](https://nodejs.org) (v18 trở lên). Cài theo mặc định, không cần thay đổi gì.

---

### Bước 3 — Build C binary

Vào **Start Menu**, tìm và mở **MSYS2 MINGW64** (không phải MSYS2 MSYS hay UCRT64).

![MSYS2 MINGW64 terminal](https://www.msys2.org/docs/environments.png)

Trong terminal MINGW64, di chuyển đến thư mục project. Ví dụ nếu project nằm ở `D:\riscv-desktop`:

```bash
cd /d/riscv-desktop
```

> **Lưu ý:** Trong MSYS2, ổ `D:\` viết thành `/d/`, dấu `\` đổi thành `/`.

Chạy lần lượt hai lệnh sau:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

Build thành công sẽ thấy:

```
[100%] Linking C executable riscv_emu_cli.exe
[100%] Built target riscv_emu_cli
```

File `build/riscv_emu_cli.exe` đã sẵn sàng.

---

### Bước 4 — Chạy app

Mở **Command Prompt** hoặc **PowerShell** bình thường (không cần MSYS2), cd vào thư mục project rồi chạy:

```bash
npm install
npm start
```

Lần đầu `npm install` sẽ tải Electron về, mất vài phút tùy tốc độ mạng. Từ lần sau chỉ cần `npm start`.

---

## Giao diện chi tiết

```
┌────────────────────────────────────────────────────────────────────────────┐
│  TOOLBAR: Load · Run · Pause · Step · Reset · Speed · Reg format · Status  │
├──────────────┬──────────────────────────────┬──────────────────────────────┤
│              │                              │                              │
│ DISASSEMBLY  │         REGISTERS            │        PERIPHERALS           │
│              │                              │  GPIO Port A / Port B        │
│  Danh sách   │  PC, x0–x31                  │  Timer progress bar          │
│  lệnh ASM    │  ABI names                   │  UART Console                │
│  Breakpoint  │  Hex / Dec / Bin             │  GPIO Registers              │
│              │                              │                              │
├──────────────┴──────────────────────────────┤                              │
│              MEMORY INSPECTOR               │                              │
│  ROM / RAM · Hex dump · ASCII · Go to addr  │                              │
└─────────────────────────────────────────────┴──────────────────────────────┘
```

---

### Toolbar

Thanh công cụ nằm trên cùng, điều khiển toàn bộ vòng đời thực thi.

| Thành phần | Chức năng |
|---|---|
| **Load** | Mở dialog chọn file `.bin` hoặc chọn chương trình mẫu từ `programs/` |
| **Run** | Chạy liên tục, số cycle mỗi frame do Speed quyết định |
| **Pause** | Dừng tại lệnh hiện tại, giữ nguyên toàn bộ state |
| **Step** | Thực thi đúng một lệnh, cập nhật tất cả panel |
| **Reset** | Xóa sạch state, trả về IDLE |
| **Speed** | Số cycle chạy mỗi lần GUI poll — xem bảng bên dưới |
| **Regs** | Định dạng hiển thị register: HEX / DEC / BIN |
| **MIPS** | Tốc độ thực thi thực tế (triệu lệnh/giây) |
| **Cycle** | Tổng số cycle đã thực thi từ lúc load |
| **Status pill** | Trạng thái hiện tại: `IDLE` / `PAUSED` / `RUNNING` / `HALTED` |

**Chọn Speed phù hợp:**

| Speed | Dùng khi nào |
|---|---|
| GPIO (200/frame) | Muốn thấy LED blink, Knight Rider, animation GPIO mượt mà |
| Slow (1K/frame) | Step chậm để theo dõi từng thay đổi register |
| Med (10K/frame) | Mặc định — cân bằng giữa tốc độ và quan sát |
| Fast (100K/frame) | Chạy nhanh chương trình tính toán như Prime Sieve, Bubble Sort |
| Turbo (500K/frame) | Chạy tối đa, bỏ qua quan sát chi tiết |

---

### Disassembly

Panel bên trái, hiển thị toàn bộ chương trình dưới dạng assembly sau khi load binary.

```
▶  ●  0x00000068   fddff06f   j       -36
   ○  0x0000006c   05d00893   addi    a7, zero, 93
   ○  0x00000070   00000073   ecall
```

| Thành phần | Ý nghĩa |
|---|---|
| `▶` | Mũi tên xanh chỉ lệnh PC đang trỏ tới |
| `●` đỏ / `○` | Breakpoint đang bật / tắt tại địa chỉ đó |
| Địa chỉ | Vị trí lệnh trong ROM, bắt đầu từ `0x00000000` |
| Hex raw | 4 byte mã máy của lệnh, little-endian |
| Mnemonic | Tên lệnh màu cam: `addi`, `sw`, `bge`, `jal`, `ecall`... |
| Operands | Toán hạng: register, immediate value, offset nhảy |

**Cách dùng breakpoint:** Click vào bất kỳ dòng nào để toggle breakpoint. Khi Run gặp địa chỉ có breakpoint thì tự động chuyển sang PAUSED — giống debugger thực tế. Panel tự scroll theo PC khi đang chạy.

**Học được gì từ đây:** Đọc assembly giúp hiểu tại sao vòng `for` trong C lại sinh ra cặp lệnh `bge` + `jal`, tại sao function call dùng `jal ra` + `jalr ra`, tại sao struct field access là `lw rd, offset(rs1)`.

---

### Registers

Panel ở giữa, hiển thị toàn bộ 32 general-purpose register cùng PC.

```
PC   = 0x00000068      Cycle: 163000

x0   zero   0x00000000     x1   ra     0x00000000
x2   sp     0x00000000     x3   gp     0x00000000
x4   tp     0x00000000     x5   t0     0x40002000
x6   t1     0x00000001     x7   t2     0x0000000a
x8   fp     0x00000015     x9   s1     0x00000022
...
```

| Register | ABI name | Vai trò |
|---|---|---|
| x0 | zero | Luôn bằng 0, không ghi được — đặc điểm thiết kế của RISC-V |
| x1 | ra | Return address — lưu địa chỉ trả về khi gọi function |
| x2 | sp | Stack pointer |
| x5–x7 | t0–t2 | Temporary — dùng trong tính toán ngắn hạn |
| x8–x9 | s0–s1 | Saved — giữ giá trị qua function call |
| x10–x17 | a0–a7 | Argument / return value, `a7` dùng cho syscall number |
| x18–x27 | s2–s11 | Saved register mở rộng |
| x28–x31 | t3–t6 | Temporary register mở rộng |

Register vừa thay đổi ở lệnh trước sẽ được **highlight xanh lá** một khoảnh khắc để dễ theo dõi.

**Chuyển đổi format:** Dropdown **Regs** trên toolbar đổi giữa HEX (mặc định), DEC (số thập phân có dấu), BIN (32 bit nhị phân). BIN đặc biệt hữu ích khi quan sát kết quả các lệnh `AND`, `OR`, `XOR`, `SLL`, `SRL`.

**Học được gì từ đây:** Nhìn `t0 = 0x40002000` biết ngay đó là UART base address. Nhìn `a7 = 93` trước `ecall` hiểu đây là syscall exit(0). Thấy `sp = 0` trong các demo đơn giản vì chưa dùng stack frame.

---

### Peripherals

Panel bên phải, hiển thị real-time state của 3 peripheral theo thứ tự từ trên xuống.

#### GPIO (0x40000000)

```
Port A — Output
  [●] [○] [●] [○] [●] [○] [●] [○]
  PA0  PA1  PA2  PA3  PA4  PA5  PA6  PA7

Port B — Output
  [●] [●] [○] [○] [●] [●] [○] [○]
  PB0  PB1  PB2  PB3  PB4  PB5  PB6  PB7

Input Inject (click PA bit):
  [○] [○] [○] [○] [○] [○] [○] [○]
  ↑PA0 ↑PA1 ↑PA2 ↑PA3 ↑PA4 ↑PA5 ↑PA6 ↑PA7
```

Mỗi LED tương ứng 1 bit trong register `PORTA_OUT` hoặc `PORTB_OUT`. LED sáng xanh = bit 1, tối = bit 0.

**Input Inject:** Click vào hàng `↑PA` để toggle bit input. Chương trình đang chạy sẽ đọc được giá trị đó khi `lw` vào `0x40000008` (PORTA_IN). Dùng để mô phỏng nút bấm hoặc tín hiệu từ sensor.

**GPIO Registers** hiển thị bên dưới:

```
PORTA_DIR   0xFF   11111111   @0x40000000
PORTA_OUT   0x0F   00001111   @0x40000004
PORTA_IN    0x00   00000000   @0x40000008
PORTA_IE    0x00   00000000   @0x4000000C
─────────────────────────────────────────
PORTB_DIR   0xFF   11111111   @0x40000010
PORTB_OUT   0xF0   11110000   @0x40000014
PORTB_IN    0x00   00000000   @0x40000018
PORTB_IE    0x00   00000000   @0x4000001C
```

| Register | Ý nghĩa |
|---|---|
| DIR | Direction: bit 1 = output, bit 0 = input |
| OUT | Output value: chương trình ghi vào đây để bật/tắt LED |
| IN | Input value: chương trình đọc từ đây để nhận tín hiệu ngoài |
| IE | Interrupt Enable cho từng pin |

#### Timer (0x40001000)

```
[████████████████░░░░░░░░]

CNT: 37      LOAD: 100      Periodic
```

Thanh progress bar hiển thị `CNT / LOAD` theo phần trăm. Timer đếm ngược từ `LOAD` về 0, sau đó set cờ `STATUS` và tự reload nếu ở chế độ Periodic.

| Thông tin | Ý nghĩa |
|---|---|
| CNT | Giá trị đếm hiện tại |
| LOAD | Giá trị nạp lại sau mỗi lần expire |
| Stopped | Timer đang tắt |
| One-shot | Timer chạy một lần rồi dừng |
| Periodic | Timer tự reload liên tục |

**Cách timer tick:** Timer tick mỗi 100 CPU cycle. Vậy `LOAD = 5` có nghĩa timer fire sau `5 × 100 = 500` CPU cycle. Các chương trình demo dùng polling: `lw t, 0xC(t3); beq t, zero, -4` để chờ cờ STATUS.

#### UART Console (0x40002000)

```
Sieve of Eratosthenes N=200
002 003 005 007 011 013 017 019 023 029
031 037 041 043 047 053 059 061 067 071
...
Total primes: 046
Done
```

Mọi byte chương trình ghi vào `0x40002000` (UART Data Register) đều hiện ra đây dưới dạng ký tự ASCII. Đây là cách duy nhất chương trình RISC-V "in ra màn hình" trong emulator này.

Nút **Clear** xóa sạch nội dung console mà không ảnh hưởng đến state emulator.

**Học được gì từ đây:** Hiểu tại sao embedded software dùng UART để debug — `printf` cuối cùng cũng gọi xuống một hàm ghi từng byte vào UART hardware. UART ở đây không có baud rate, không có buffer overflow — đơn giản hóa để tập trung vào việc học CPU.

---

### Memory Inspector

Panel dưới cùng, trải dài từ trái sang phải bên dưới Disassembly và Registers.

```
Region: [ROM (0x0000_0000) ▼]   Go to: [0x00000000] [Go]

0x00000000   b7 22 00 40  13 03 10 00  23 a4 62 00  13 04 00 00   .".@....#.b.....
0x00000010   93 04 10 00  13 09 80 00  63 d8 24 01  33 04 94 00   ........c.$.3...
0x00000020   93 84 14 00  6f f0 5f ff  93 03 30 05  23 a0 72 00   ....o._...0.#.r.
```

| Thành phần | Ý nghĩa |
|---|---|
| Region | Chọn vùng nhớ: ROM (code) hoặc RAM (data) |
| Go to | Nhảy đến địa chỉ hex bất kỳ |
| Địa chỉ | Địa chỉ đầu mỗi hàng, mỗi hàng = 16 byte |
| Hex dump | Giá trị từng byte dưới dạng 2 chữ số hex |
| `·` | Dấu phân cách giữa byte 7 và byte 8 |
| ASCII | Byte nào là ký tự in được thì hiện ký tự, còn lại hiện `.` |

**ROM** bắt đầu tại `0x00000000` — chứa machine code của chương trình, read-only trong lúc chạy.

**RAM** bắt đầu tại `0x20000000` — chứa dữ liệu runtime: biến, mảng, kết quả tính toán. Các demo như `bubble_sort`, `prime_sieve`, `matrix_mul` đều lưu dữ liệu tại đây.

**Học được gì từ đây:** Nhìn vào ROM thấy `b7 22 00 40` là encoding của lệnh `lui t0, 0x40002` — little-endian, 4 byte/lệnh. Sau khi `bubble_sort` chạy xong, chuyển sang RAM và xem tại `0x20000000` — 32 số đã được sắp xếp tăng dần nằm ngay đó dưới dạng 32-bit integers.

---

## Chương trình demo

Tất cả file `.bin` trong thư mục `programs/`. Load bằng nút **Load** trên toolbar.

| Chương trình | Mô tả | GPIO | UART | Timer | Speed gợi ý |
|---|---|---|---|---|---|
| `fibonacci` | Dãy Fibonacci 8 số | — | ✅ | — | Med |
| `sum_1_to_10` | Tính tổng 1+2+…+10=55 | — | ✅ | — | Med |
| `gpio_blink` | Blink tất cả LED Port A 10 lần | ✅ A | ✅ | — | GPIO |
| `timer_test` | Đợi timer interrupt 5 lần, in số thứ tự | — | ✅ | ✅ | Med |
| `knight_rider` | LED chạy qua lại Port A, Port B đếm ngược | ✅ A+B | ✅ | ✅ | GPIO |
| `led_show` | 32 pattern đèn đẹp, lặp 80 vòng | ✅ A+B | ✅ | ✅ | GPIO |
| `pwm_fade` | LED fade sáng dần tối dần 0→255→0 | ✅ A+B | ✅ | — | Turbo |
| `lfsr_random` | LFSR-16 random generator, 4096 tick | ✅ A+B | ✅ | ✅ | Slow |
| `counter_marathon` | Đếm 0→32767, A=low byte, B=high byte | ✅ A+B | ✅ | ✅ | GPIO |
| `prime_sieve` | Sàng Eratosthenes N=200, in tất cả số nguyên tố | ✅ A | ✅ | — | Fast |
| `bubble_sort` | Sort 32 số ngẫu nhiên, in từng pass | ✅ A+B | ✅ | — | Fast |
| `matrix_mul` | Nhân ma trận 4×4 trong RAM, in kết quả | ✅ A | ✅ | — | Fast |

---

## Kiến trúc kỹ thuật

```
┌──────────────────────────────────────────────────────┐
│                  Electron (main.js)                  │
│  Spawn C process · Relay IPC · File dialog           │
└───────────────────────┬──────────────────────────────┘
                        │ IPC
┌───────────────────────┴──────────────────────────────┐
│            renderer.js (Electron renderer)           │
│  Giao diện · Gửi lệnh · Nhận JSON · Render DOM       │
│  Không có simulation logic                           │
└───────────────────────┬──────────────────────────────┘
                        │ stdin / stdout (JSON protocol)
┌───────────────────────┴──────────────────────────────┐
│              riscv_emu_cli --json (C binary)         │
│                                                      │
│  CPU       RV32I full instruction set                │
│  Bus       Address decoding ROM / RAM / Peripheral   │
│  Memory    ROM 64KB (0x00000000) + RAM 64KB          │
│            (0x20000000)                              │
│  GPIO      2 port x 8 bit, DIR / OUT / IN / IE       │
│            (0x40000000)                              │
│  Timer     32-bit countdown, periodic / one-shot     │
│            (0x40001000)                              │
│  UART      TX buffer 8KB (0x40002000)                │
└──────────────────────────────────────────────────────┘
```

**Giao thức JSON stdin/stdout:** GUI gửi lệnh text vào stdin của C process, C trả về một dòng JSON qua stdout sau mỗi lệnh.

| Lệnh GUI → C | Phản hồi C → GUI |
|---|---|
| `load <path>` | `{"ok": true}` |
| `run <N>` | JSON state đầy đủ |
| `step` | JSON state đầy đủ |
| `state` | JSON state đầy đủ |
| `disasm` | JSON array các instruction |
| `mem rom` / `mem ram` | `{"base": N, "data": [...]}` |
| `bp_add <addr>` | `{"ok": true}` |
| `bp_del <addr>` | `{"ok": true}` |
| `gpio_input <port> <val>` | `{"ok": true}` |
| `pause` | JSON state đầy đủ |
| `reset` | `{"ok": true}` |

---

## Keyboard shortcuts

| Phím | Tác dụng |
|---|---|
| `F5` | Run |
| `F10` | Step |
| `Space` | Toggle Run / Pause |
| `Ctrl + R` | Reset |
| `Ctrl + O` | Load file |
