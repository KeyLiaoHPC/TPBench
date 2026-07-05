# TPBench TPBlog 模块设计

文档语言：[中文](./design_tpblog_CN.md)

## 1. 目标

`tpblog` 是 TPBench 的日志与格式化输出模块，位于 `src/corelib/tpblog/`，对外通过 `tpbench.h`（源文件 `tpb-public.h`）暴露 `tpblog_*` API。

职责：

- 管理每个 run 的共享日志文件（`<workspace>/rafdb/log/tpbrunlog_*.log`）
- 提供带级别、标签、时间戳的格式化输出
- 提供定宽列（column）布局输出
- 与错误码/函数返回机制解耦（日志只负责输出，不决定 exit）

## 2. 模块结构

```
src/corelib/tpblog/
├── tpb-log.c/.h      # run log 生命周期
└── tpb-printf.c/.h   # 格式化输出与列布局
```

相关调用方：

| 组件 | 作用 |
|------|------|
| `tpb_corelib_state.c` | `tpb_corelib_init()` 时调用 `tpblog_init()` |
| `tpb-driver.c` | PLI fork 前发布 `TPB_LOG_FILE`，子进程 append 同一 log |
| `tpb-impl.c` | `tpb_report_error()` 经 `tpblog_printf_f()` 输出 |
| `tpb-io.c` | CLI benchmark 结果格式化，调用 `tpblog_printf_f()` |
| `tpbcli-print-kernel-help.c` | kernel help 表格，调用 `tpblog_printf_c()` |

## 3. 公共 API

### 3.1 日志会话

```c
int tpblog_init(void);
void tpblog_cleanup(void);
const char *tpblog_get_filepath(void);
void tpblog_set_level(tpb_log_level_t level);
tpb_log_level_t tpblog_get_level(void);
```

- 环境变量 `TPB_LOG_FILE`（`TPBLOG_FILE_ENV`）：PLI 子进程 append 父进程 log，不写第二份 session header。
- 正常 `tpbcli run`：在 workspace 下创建新 log 并写 session header。

### 3.2 格式化输出

```c
void tpblog_printf(tpb_log_level_t level, uint32_t log_type,
                   uint32_t flags, const char *fmt, ...);
void tpblog_printf_f(...);  /* 与 tpblog_printf 相同，均双写 stdout + run log */
```

**标签**（`flags` 含 `TPBLOG_FLAG_TAG` 时）：

| log_type | 输出标签 |
|----------|----------|
| `TPBLOG_TYPE_INFO` | `[INFO]` |
| `TPBLOG_TYPE_WARN` | `[WARN]` |
| `TPBLOG_TYPE_ERRO` | `[ERRO]` |

不再使用 `[NOTE]`。

**verbosity**：`level < tpblog_get_level()` 时静默。

### 3.3 定宽列输出

```c
void tpblog_printf_c(const float *col_ratios, int ncol, int gap,
                     const char *const *cells);
```

算法：

1. 用 `ioctl(TIOCGWINSZ)` 读取 stdout 终端宽度，失败则 85。
2. 可用宽度 = 总宽度 − `gap * (ncol − 1)`。
3. 按 `col_ratios` 比例 `floor` 分配；`NULL` 表示等分。
4. 每列再减 1 字符，留给连字符断词 `-`。
5. 任一列最终宽度 < 1 时，退化为逐 cell 逐行输出。
6. 否则按列宽做空白断词 / 连字符断词，并支持多行对齐。

测试可通过环境变量 `TPBLOG_TEST_WIDTH` 固定终端宽度。

### 3.4 辅助宏

```c
#define tpblog_snprintf(buf, bufsz, fmt, ...) snprintf((buf), (bufsz), (fmt), ##__VA_ARGS__)
```

## 4. 数据流

```
tpbcli / kernel
    → tpblog_printf_f / tpblog_printf_c
        → stdout（用户可见）
        → run log file（持久化）
```

PLI 共享 log：

```
parent: tpblog_cleanup → setenv(TPB_LOG_FILE) → fork
child:  tpblog_init (append)
parent: tpblog_init (append)
```

## 5. 错误处理边界

- `tpb_report_error()` 根据原因码选择 `[INFO]` / `[WARN]` / `[ERRO]` 日志 tag。
- 日志 tag 使用 `TPB_LOG_TAG_*`（或 `TPBLOG_TYPE_*` alias），与 `TPBE_*` 原因码分离。
- 需要退出的调用点显式调用 `exit(err)`；`TPBE_KERN_VERIFY_FAIL` 等 warn 级原因码不退出。
- `TPB_RETURN_ON_ERROR` 仅调用 `tpb_report_error()` 并 return。

## 6. 使用示例

### 6.1 常规日志

```c
tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
                "Kernel %s finished.\n", name);
```

### 6.2 定宽列

```c
char left[64], right[256];
const char *cells[2];
float ratios[] = {32.0f, 50.0f};

tpblog_snprintf(left, sizeof(left), "%s", "ntest");
tpblog_snprintf(right, sizeof(right), "%s", "Number of iterations");
cells[0] = left;
cells[1] = right;
tpblog_printf_c(ratios, 2, 4, cells);
```

### 6.3 查看 run log

```bash
tpbcli run --kernel stream --kargs stream_array_size=524288,ntest=100
LOG_FILE=$(ls -t ~/.tpbench/rafdb/log/tpbrunlog_*.log | head -1)
tail -50 "$LOG_FILE"
```

## 7. 测试

Pack **A8**（`tests/corelib/test_tpblog.c`）覆盖：

- session header 创建
- `TPB_LOG_FILE` append
- stdout / log 双写一致性
- `[INFO]` / `[WARN]` / `[ERRO]` 标签
- 列宽计算、列输出、退化模式
- `tpblog_snprintf` 宏

```bash
cd build && ctest -R '^A8'
```

## 8. 迁移说明

- 旧 `tpb_printf()` / `tpb_log_*()` 已移除；统一使用 `tpblog_*`。
- `tpbcli-kernel-table.c` 已删除，由 `tpblog_printf_c()` 替代。
- 所有应用层用户可见输出统一走 **stdout**（经 `tpblog_*` 双写 log）。
