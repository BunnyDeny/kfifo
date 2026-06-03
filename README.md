# 🚀 kfifo — Linux 内核风格环形缓冲区 🎯

> 🧠 为嵌入式 / MCU 设计的高性能环形 FIFO，移植自 Linux 内核 kfifo ✨

---

## 🎨 设计亮点 💡

### 1️⃣ 与运算取模 —— 除法？不存在的 🙅‍♂️

传统环形缓冲区用 `%` 取模做索引回绕，但 MCU 上 **硬件除法器不是标配** 🐌，一次 `%` 几十个时钟周期就没了 💀。

kfifo 要求 `size` 必须是 **2 的幂** 🎯，用 `mask = size - 1` 把取模变成按位与运算 ⚡：

```c
// ❌ 传统做法：取模（慢到怀疑人生）
index = pos % size;

// ✅ kfifo 做法：按位与（一个周期搞定）
index = pos & mask;
```

**原理超简单** 🧑‍🏫：假设 `size = 8`，`mask = 7 = 0b0111` 🧙

| pos | 二进制 | pos & 7 | % 8 |
|-----|--------|---------|-----|
| 5 | 0b0101 | **5** ✅ | 5 |
| 9 | 0b1001 | **1** ✅ | 1 |
| 15 | 0b1111 | **7** ✅ | 7 |

低几位原样保留 👶，高位直接清零 💀，天然在 `0 ~ size-1` 之间跳舞 🕺！

> 🎉 **性能差距：`&` 1 个时钟周期 vs `%` 数十个时钟周期，吊打没商量！** 🏎️💨

---

### 2️⃣ `in` / `out` 只增不减 —— 溢出？故意的！😎

大多数环形缓冲区的 `head` / `tail` 要在 `[0, size-1]` 之间反复横跳 🐒。

kfifo 的 `in` 和 `out` 直接用 `uint32_t`，**只增不减** 📈，跑到 42 亿再自然溢出回去 ♻️。

```c
// 📏 当前数据量 —— 就一行！
static inline uint32_t kfifo_len(kfifo_t *fifo)
{
    return fifo->in - fifo->out;
}
```

🎯 **为啥这能行？** C 标准（C99 起）白纸黑字 📜：

> *"A computation involving unsigned operands can never overflow, because a result that cannot be represented by the resulting unsigned integer type is reduced modulo the number that is one greater than the largest value that can be represented by the resulting type."*
>
> — C99 §6.2.5/9

💬 翻译成人话 👇

无符号整数运算**永不溢出** 🚫💥。结果表示不了怎么办？对 **"最大值 + 1" 取模**。

`uint32_t` 最大值是 `0xFFFFFFFF`，所以模数是 2³²。于是：

```
0xFFFFFFFF + 1
= 4294967296 mod 2³²          ← 标准要求取模 📐
= 0                            ← 自然回绕 🔄
```

标准只规定了这一条取模规则，"回绕到 0"是它的自然推论 🎓。没有特判，不是编译器仁慈，是数学保证。

🧮 **溢出后 `in - out` 依然正确**：

| in | out | in - out | 推演 | 含义 |
|----|-----|----------|------|------|
| 100 | 80 | **20** ✅ | 100 - 80 | 20 字节数据 |
| 50 | 0xFFFFFF00 | **306** ✅ | (50 + 2³² - 0xFFFFFF00) mod 2³² | 刚好溢出绕回 |

直观理解 🧠：`in` 在 2³² 的环上绕着 `out` 跑 🏃‍♂️💨，差值就是它领先的距离。无论它在环上超了多少圈，数学恒成立 🔐。

```c
// 💪 有标准兜底，写代码直接莽
fifo->in += len;   // 随便加，溢出行为确定
return fifo->in - fifo->out;  // 永远正确
```

  有符号整数就没这待遇 🌋：`int32_t` 溢出是**未定义行为**，编译器可以删代码、格式化硬盘、召唤 Nasus 🐊。所以 `in` / `out` 必须用 `uint32_t`，不是碰巧。

> 🔮 不需要记"头尾位置"，只需要知道"写了多少、读了多少"，索引通过 `& mask` 随时算出来。干净利落！🧹

---

### 3️⃣ 两段 memcpy —— 回绕也能打 📐

写入时，如果 `pos + len` 超出 buffer 末尾 🏁，咋办？分两段拷！

```
    buffer[0]  ...  buffer[7]        size=8
    ┌──┬──┬──┬──┬──┬──┬──┬──┐
    │  │  │  │  │  │ A│ B│  │     in = 5, 要写 5 字节
    └──┴──┴──┴──┴──┴──┴──┴──┘
                          ↑
                    in 在位置 5

    第 1 段 🔵：拷到末尾 [5..7] → 3 字节
    第 2 段 🔴：回绕到 [0..1] → 2 字节

    结果：
    ┌──┬──┬──┬──┬──┬──┬──┬──┐
    │🔴│🔴│  │  │  │🔵│🔵│🔵│
    └──┴──┴──┴──┴──┴──┴──┴──┘
```

```c
// 🍰 第一段：拷到 buffer 末尾
l = len < (size - (in & mask)) ? len : (size - (in & mask));
memcpy(buffer + (in & mask), data, l);

// 🎂 第二段：剩下的从 buffer 头部开始拷
memcpy(buffer, data + l, len - l);
```

> 🦾 两次 `memcpy`，代码清晰，编译器还能优化成高效的批量拷贝指令！比逐字节循环不知道高到哪里去了 🚀

---

### 4️⃣ 内存屏障 —— 多核 / ISR 安全 🛡️

单生产者 + 单消费者（SPSC）场景下，**不加锁** 🤸！靠内存屏障保证数据对消费者可见：

```c
// 🏭 生产者：写数据 → 屏障 → 更新 in
memcpy(buffer + ..., data, ...);
smp_wmb();          // 💪 确保数据写完再更新 in
fifo->in += len;

// 🛒 消费者：读 in → 屏障 → 读数据 → 屏障 → 更新 out
len = min(len, fifo->in - fifo->out);
smp_rmb();          // 👀 确保看到 in 对应的最新数据
memcpy(data, buffer + ..., ...);
smp_mb();           // 🔒 确保数据读完再更新 out
fifo->out += len;
```

> ⚠️ 在单核 MCU 上这些屏障基本是空操作，不影响理解和使用！把它当空气就行 🌬️

---

## 📚 API 速览 🔍

### 🏗️ `kfifo_init`

```c
void kfifo_init(kfifo_t *fifo, uint8_t *buffer, uint32_t size);
```

🎬 **初始化一个 kfifo 实例**

| 参数 | 说明 |
|------|------|
| `fifo` | 📦 kfifo 结构体指针 |
| `buffer` | 🗂️ 用户提供的外部缓冲区 |
| `size` | 📏 缓冲区大小，**必须是 2 的幂** ⚠️ |

```c
uint8_t buf[128];        // 128 = 2⁷ ✅
kfifo_t fifo;
kfifo_init(&fifo, buf, 128);
```

---

### 📥 `kfifo_put`

```c
uint32_t kfifo_put(kfifo_t *fifo, const uint8_t *data, uint32_t len);
```

📥 **向 FIFO 写入数据**

| 参数 | 说明 |
|------|------|
| `fifo` | 📦 kfifo 实例 |
| `data` | ✍️ 要写入的数据 |
| `len` | 📏 期望写入长度 |

🔙 **返回值**：实际写入的字节数。空间不够时会被截断 ✂️

```c
uint32_t wrote = kfifo_put(&fifo, my_data, 100);
// wrote <= 100，取决于还剩多少空间 🪣
```

---

### 📤 `kfifo_get`

```c
uint32_t kfifo_get(kfifo_t *fifo, uint8_t *data, uint32_t len);
```

📤 **从 FIFO 读出数据**

| 参数 | 说明 |
|------|------|
| `fifo` | 📦 kfifo 实例 |
| `data` | 📋 读出数据存放位置 |
| `len` | 📏 期望读出长度 |

🔙 **返回值**：实际读出的字节数。数据不够时会被截断 ✂️

```c
uint8_t buf[64];
uint32_t got = kfifo_get(&fifo, buf, sizeof(buf));
// got <= 64，取决于 FIFO 里还有多少数据 📊
```

---

### 📏 `kfifo_len`

```c
static inline uint32_t kfifo_len(kfifo_t *fifo)
```

🔢 **查询当前 FIFO 中有多少数据**

```c
uint32_t pending = kfifo_len(&fifo);
if (pending > 0) {
    // 🎉 有数据可以读！
}
```

---

### 🪣 `kfifo_avail`

```c
static inline uint32_t kfifo_avail(kfifo_t *fifo)
```

🪣 **查询 FIFO 还剩多少空闲空间**

```c
uint32_t free_space = kfifo_avail(&fifo);
if (free_space >= need) {
    // 🆗 空间够，放心写！
}
```

---

### 🔄 `kfifo_reset`

```c
static inline void kfifo_reset(kfifo_t *fifo)
```

🔄 **清空 FIFO**（直接把 `in` 设为 `out`，数据原地不动但标记为"已读"）

```c
kfifo_reset(&fifo);  // 🧹 一键清空，重新开始
```

---

## 🎮 完整使用示例 🧪

```c
#include "kfifo.h"

#define FIFO_SZ 32        // 32 = 2⁵ ⚡

uint8_t buf[FIFO_SZ];
uint8_t rbuf[64];
kfifo_t fifo;

int main(void)
{
    kfifo_init(&fifo, buf, FIFO_SZ);        // 🏗️ 初始化

    // 📥 写入 20 字节
    uint8_t data[20] = "Hello kfifo! 🚀";
    kfifo_put(&fifo, data, 20);

    // 📏 看看有多少数据
    printf("len = %u\n", kfifo_len(&fifo));  // → 20

    // 📤 读出 7 字节
    uint32_t got = kfifo_get(&fifo, rbuf, 7); // → 7, "Hello k"

    // 🪣 还剩多少空间
    printf("free = %u\n", kfifo_avail(&fifo)); // → 32 - 13 = 19

    // 🔄 清空
    kfifo_reset(&fifo);
    printf("len = %u\n", kfifo_len(&fifo));  // → 0 🧹

    return 0;
}
```

---

## 🗿 kfifo_t 结构体一览 👀

```c
typedef struct {
    uint8_t *buffer;  // 🗂️ 数据缓冲区指针（外部提供）
    uint32_t size;    // 📦 缓冲区总大小（2 的幂）
    uint32_t mask;    // 🎭 size - 1，取模用
    uint32_t in;      // 📥 累计写入字节数（只增不减，自然溢出）
    uint32_t out;     // 📤 累计读出字节数（只增不减，自然溢出）
} kfifo_t;
```

> 💡 总共 20 字节（32 位平台），轻量化到极致 🪶

---

## ⚡ 总结 🎤

| 特性 | 说明 |
|------|------|
| 🔒 无锁 | SPSC 场景不需要锁，靠内存屏障 |
| ⚡ 极速取模 | `& mask` 代替 `% size` |
| ♻️ 自然溢出 | `uint32_t` 索引只增不减 |
| 📐 两段拷贝 | memcpy 处理回绕，清晰高效 |
| 🪶 超轻量 | 核心不到 30 行，零依赖 |
| 🎯 广泛适用 | UART 缓冲 🖨️、CLI 输入输出 💻、任务间通信 📡 …|

> 🐰 **简单即正义。没有黑魔法，只有扎实的基础功！** ✨
